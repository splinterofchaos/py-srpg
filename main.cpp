#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_ttf.h>
#include <GL/glu.h>

#include <chrono>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <unordered_set>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "ecs.h"
#include "components.h"
#include "decision.h"
#include "dijkstra.h"
#include "font.h"
#include "game.h"
#include "glpp.h"
#include "grid.h"
#include "graphics.h"
#include "math.h"
#include "script.h"
#include "shaders.h"
#include "timer.h"
#include "ui.h"
#include "user_input.h"

using Time = std::chrono::high_resolution_clock::time_point;
using Milliseconds = std::chrono::milliseconds;

Time now() {
  return std::chrono::high_resolution_clock::now();
}

std::ostream& operator<<(std::ostream& os, glm::ivec2 v) {
  return os << '<' << v.x << ", " << v.y << '>';
}

std::ostream& operator<<(std::ostream& os, glm::vec2 v) {
  return os << '<' << v.x << ", " << v.y << '>';
}

// TODO: Just move this to a conversations file instead of forward declaring
// here.
Script demo_convo();

const char* const STARTING_GRID = R"(
##############
#............#
#............#
#....####....####
#....#  #.......####
#....#  #..........#
#....####..........#
#...............####
#...............#
#################)";

// Adds entity description text to an info box.
void add_entity_desc_text(const Game& game, TextBoxPopup& info_box,
                          EntityId id) {
  const Actor* actor = nullptr;
  game.ecs().read(id, &actor);

  // First, figure out what we need to print. Figuring out where it goes on
  // screen comes later.
  info_box.clear();
  if (actor) {
    info_box.add_text(actor->name);
    info_box.add_text("HP: ", std::to_string(actor->hp), "/",
                      std::to_string(actor->stats.max_hp));
    info_box.add_text("MOV: ", std::to_string(actor->stats.move));
    info_box.add_text("SPD: ", std::to_string(actor->stats.speed));
    info_box.add_text("STR: ", std::to_string(actor->stats.strength));
    info_box.add_text("DEF: ", std::to_string(actor->stats.defense));

    std::unordered_set<std::string> stat_effs;
    for (const StatusEffect& eff : actor->statuses) {
      if (eff.slowed) stat_effs.emplace("slowed");
    }
    for (const std::string& s : stat_effs) info_box.add_text(s);
  } else {
    info_box.add_text("UNKNOWN");
  }
}

struct GlyphRenderTask {
  glm::vec3 pos;
  GlyphRenderConfig rc;
};

struct MarkerRenderTask {
  glm::vec3 pos;
  Marker marker;
};

class RenderTasks {
  struct Layer {
    std::vector<GlyphRenderTask> glyphs;
    std::vector<MarkerRenderTask> markers;
  };

  std::vector<Layer> layers_;

public:
  RenderTasks() : layers_(Transform::ZLayer::N_Z_LAYERS, Layer()) { }

  void add_glyph_task(const Game& game, Transform transform,
                      const GlyphRenderConfig& rc) {
    layers_[transform.z].glyphs.push_back({game.to_graphical_pos(transform),
                                           rc});
  }

  void add_marker_task(const Game& game, Transform transform,
                       const Marker& marker) {
    layers_[transform.z].markers.push_back({game.to_graphical_pos(transform),
                                            marker});
  }

  void execute_tasks_and_clear(const Game& game) {
    for (Layer& layer : layers_) {
      for (const GlyphRenderTask& task : layer.glyphs)
        game.glyph_shader().render_glyph(task.pos, TILE_SIZE, task.rc);
      for (const MarkerRenderTask& task : layer.markers)
        game.marker_shader().render_marker(task.pos, TILE_SIZE,
                                           task.marker.color,
                                           task.marker.stretch);

      layer.glyphs.clear();
      layer.markers.clear();
    }
  }
};

// When an actor wants to take a turn, its "SPD" or "speed" stat contributes to
// its initial "energy" which then accrues over time. One tick, speed energy.
// The entity with the most energy after any has more than ENERGY_REQUIRED
// shall go next and then have its energy reverted to the value of its speed.
// Ties are won by the entity with the smallest ID.
//
// At least one tick shall pass between turns. If an entity's energy is above
// the threshold, after its turn, its energy becomes the overflow plus its
// speed.
//
// The goal is that an entity with twice the speed of another shall move
// roughly twice as often.
constexpr int ENERGY_REQUIRED = 1000;

EntityId advance_until_next_turn(Ecs& ecs) {
  struct AgentRef {
    EntityId id;
    Actor& actor;
    int* energy;
  };

  std::vector<AgentRef> agents;
  for (auto [id, actor, agent] : ecs.read_all<Actor, Agent>())
    agents.push_back({id, actor, &agent.energy});

  if (agents.empty()) return EntityId();

  AgentRef* max_agent = nullptr;
  EntityId max_id;
  while (!max_agent || *max_agent->energy < ENERGY_REQUIRED) {
    for (AgentRef& a : agents) {
      a.actor.expire_statuses();
      *a.energy += a.actor.stats.speed;
      if (!max_agent || *a.energy > *max_agent->energy) max_agent = &a;
    }
  }

  if (!max_agent) return EntityId();

  *max_agent->energy -= ENERGY_REQUIRED;

  return max_agent->id;
}

EntityId spawn_agent(Game& game, std::string name, glm::ivec2 pos, Team team) {
  return game.ecs().write_new_entity(Transform{glm::vec2(pos), Transform::ACTORS},
                                     GridPos{pos},
                                     Actor(std::move(name), Stats()),
                                     Agent(team));
}

void make_human(Game& game, EntityId human) {
  GlyphRenderConfig rc(game.font_map().get('@'),
                       glm::vec4(.9f, .6f, .1f, 1.f));
  rc.center();
  game.ecs().write(human, std::vector{std::move(rc)}, Ecs::CREATE_OR_UPDATE);

  Actor& actor = game.ecs().read_or_panic<Actor>(human);
  game.ecs().write(human, actor);
}

void make_hammer_guy(Game& game, EntityId guy) {
  GlyphRenderConfig rc(game.font_map().get('H'),
                       glm::vec4(.9f, .6f, .1f, 1.f));
  rc.center();
  game.ecs().write(guy, std::vector{std::move(rc)}, Ecs::CREATE_OR_UPDATE);

  Actor& actor = game.ecs().read_or_panic<Actor>(guy);

  Script on_hit;
  on_hit.push([guy](Game& game) {
    glm::ivec2 pos = game.ecs().read_or_panic<GridPos>(guy).pos;

    EntityId defender = game.decision().target;
    glm::ivec2 defender_pos =
        game.ecs().read_or_panic<GridPos>(defender).pos;

    glm::ivec2 target_tile = defender_pos + (defender_pos - pos);
    
    auto [tile, exists] = game.grid().get(target_tile);
    if (exists && tile.walkable) {
      Script script;
      push_move_along_path(script, defender, {defender_pos, target_tile});
      game.add_ordered_script(std::move(script));
    }

    return ScriptResult::CONTINUE;
  });

  actor.triggers.set("on_hit_enemy", std::move(on_hit));
      
  game.ecs().write(guy, actor);
}

void make_spider(Game& game, EntityId spider) {
  // The shape we're making here:
  // =|=
  // =|=
  constexpr glm::vec4 COLOR = glm::vec4(0.f, 0.2f, 0.6f, 1.f);
  GlyphRenderConfig rc(game.font_map().get('='),
                       COLOR);
  rc.center();
  std::vector<GlyphRenderConfig> rcs(4, rc);
  rcs[0].offset = glm::vec3( 0.25f,  0.17f, 0.f);
  rcs[1].offset = glm::vec3(-0.25f,  0.17f, 0.f);
  rcs[2].offset = glm::vec3(-0.25f, -0.17f, 0.f);
  rcs[3].offset = glm::vec3( 0.25f, -0.17f, 0.f);

  rcs.emplace_back(game.font_map().get('|'), COLOR);
  rcs.back().center();
  rcs.back().offset_scale = 0.5f;

  game.ecs().write(spider, std::move(rcs), Ecs::CREATE_OR_UPDATE);

  Actor& actor = game.ecs().read_or_panic<Actor>(spider);
  actor.stats.speed -= 2;
  actor.stats.range = 3;
  actor.embue.slowed = true;
  actor.embue.ticks_left = ENERGY_REQUIRED / 5;
  game.ecs().write(spider, actor);
}

void make_bat(Game& game, EntityId bat) {
  // It should look like this: ^O^
  constexpr glm::vec4 COLOR = glm::vec4(0.f, 0.2f, 0.6f, 1.f);
  GlyphRenderConfig wing_rc(game.font_map().get('^'), COLOR);
  wing_rc.center();

  std::vector<GlyphRenderConfig> rcs(2, wing_rc);
  rcs[0].offset = glm::vec3( 0.3f, 0.1f, 0.f);
  rcs[1].offset = glm::vec3(-0.3f, 0.1f, 0.f);

  rcs.push_back(GlyphRenderConfig(game.font_map().get('o'), COLOR));
  rcs.back().center();

  game.ecs().write(bat, std::move(rcs), Ecs::CREATE_OR_UPDATE);

  Actor& actor = game.ecs().read_or_panic<Actor>(bat);
  actor.stats.speed += 2;
  actor.stats.max_hp -= 3;
  actor.hp -= 3;
  actor.lifesteal = true;
  game.ecs().write(bat, actor);
}

void make_imp(Game& game, EntityId imp) {
  // It should look like this: ^O^
  constexpr glm::vec4 COLOR = glm::vec4(0.f, 0.2f, 0.6f, 1.f);

  std::vector<GlyphRenderConfig> rcs;
  rcs.emplace_back(game.font_map().get('@'), COLOR);
  rcs.back().center();
  rcs.back().offset += glm::vec3(-0.25f, 0.f, 0.f);

  rcs.emplace_back(game.font_map().get('^'), COLOR);
  rcs.back().center();
  rcs.back().offset += glm::vec3(-0.50f, 0.3f, 0.f);

  rcs.emplace_back(game.font_map().get('^'), COLOR);
  rcs.back().center();
  rcs.back().offset += glm::vec3(-0.00f, 0.3f, 0.f);

  rcs.emplace_back(game.font_map().get('_'), COLOR);
  rcs.back().center();
  rcs.back().offset += glm::vec3(0.05f, -0.3f, 0.f);

  game.ecs().write(imp, std::move(rcs), Ecs::CREATE_OR_UPDATE);

  Actor& actor = game.ecs().read_or_panic<Actor>(imp);
  actor.stats.speed += 4;
  actor.stats.max_hp -= 5;
  actor.hp -= 5;

  actor.triggers.set("on_recruit", demo_convo());

  game.ecs().write(imp, actor);
}

Script demo_convo() {
  Script script;
  std::string* jump_label = new std::string("START");
  push_dialogue_block(
      script, jump_label, "START",
      "I can cooperate, but what can you offer me?",
      {{"> money", "MAYBE_MONEY"},
       {"> freedom", "ALREADY_FREE"}});
  push_dialogue_block(
      script, jump_label, "MAYBE_MONEY",
      "Money? I may look poor, but I'm just a temporarily embarrassed "
      "millionaire.",
      {{"> Okay, maybe something else.", "START"},
       {"> Together, we can take down larger monsters than either "
        "can apart.", "DEAL"}});
  push_dialogue_block(
      script, jump_label, "ALREADY_FREE",
      "I'm independent! As free as it gets! It seems to me you are the one "
      "who is trapped in here, working for either your king or some "
      "benefactor?",
      {{"> I was put here by the king, but I have my own goals.",
        "INDEPENDENT?"},
       {"> The king is just. The king is good.", "ROYALIST_SCUM"}});
  push_dialogue_block(
      script, jump_label, "INDEPENDENT?",
      "Hah! The power dynamic remains. You were coerced into entering and by "
      "joining you, that'd give the king leverage over me, too. "
      "Freedom, my ass.",
      {{"> I'm working in the system to change it.", "CHANGE"},
       {"> The king gets nothing without my labor. I'm the one with the power "
        "in this situation.", "POWER"},
       {"> A rebel scum like you doesn't deserve to live!", "END"}});
  push_dialogue_block(
      script, jump_label, "POWER",
      "True... and we can make ANY demand we want...");
  push_jump(script, "DEAL");
  push_dialogue_block(
      script, jump_label, "CHANGE",
      "Talking out of your ass or just naive? What're you going to do? Raise "
      "an army to overthrow the throne? Restructure the entire society?",
      // TODO: Maybe if the player has a certain number of allies on their
      // team, this could actually be a persuasive argument.
      {{"> Yes, exactly!", "NAIVE"},
       {"> ...", "NAIVE"}});
  push_dialogue_block(
      script, jump_label, "ROYALIST_SCUM",
      "The king good? He trapped all of us in this dungeon and throws his own "
      "citizens in to die. All who support the king must die.");
  push_jump(script, "END");
  push_dialogue_block(
      script, jump_label, "NAIVE",
      "There's no use talking to a fool like you.");
  push_jump(script, "END");
  push_dialogue_block(
      script, jump_label, "DEAL", "Deal!");
  script.push_label("END");
  push_end_dialogue(script);
  push_delete(script, jump_label);
  return script;
}

Script will_not_talk_conversation() {
  Script script;
  push_dialogue_block(script, nullptr, "START",
                      "(They don't want to speak with you.)");
  return script;
}

Error run() {
  Graphics gfx;
  if (Error e = gfx.init(WINDOW_WIDTH, WINDOW_HEIGHT); !e.ok) return e;

  Game game;
  if (Error e = game.init(); !e.ok) return e;

  gl::clearColor(0.f, 0.f, 0.f, 1.f);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1, 10, -1, 1, -10, 10);

  GLuint vbo_elems[] = {0, 1, 2,
                        2, 3, 0};
  GLuint vbo_elems_id = gl::genBuffer();
  gl::bindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_elems_id);
  gl::bufferData(GL_ELEMENT_ARRAY_BUFFER, vbo_elems, GL_STATIC_DRAW);

  RenderTasks render_tasks;

  std::unordered_map<char, Tile> tile_types;
  Tile floor{.walkable = true, .glyph = '.',
             .fg_color = glm::vec4(.23f, .23f, .23f, 1.f),
             .bg_color = glm::vec4(.2f, .2f, .2f, 1.f)};
  Tile wall{.walkable = false, .glyph = '#',
            .fg_color = glm::vec4(.3f, .3f, .3f, 1.f),
            .bg_color = glm::vec4(.1f, .1f, .1f, 1.f)};

  // Create the tiles.
  //
  // Note that this MUST happen first so they are drawn first. Eventually, we
  // might want to consider z-sorting.
  //
  // Also note that the grid represents actual tile data so we don't have to
  // search the ECS every time we want to check a tile. The entities themselves
  // are just used for rendering.
  game.set_grid(arena_grid({24, 24}, wall, floor));

  make_human(game, spawn_agent(game, "Joe", {3, 3}, Team::PLAYER));
  make_human(game, spawn_agent(game, "Joa", {4, 3}, Team::PLAYER));
  make_hammer_guy(game, spawn_agent(game, "Jor", {5, 3}, Team::PLAYER));

  make_spider(game, spawn_agent(game, "spider", {12, 12}, Team::CPU));
  make_imp(game, spawn_agent(game, "imp", {10, 12}, Team::CPU));
  make_bat(game, spawn_agent(game, "bat", {10, 10}, Team::CPU));


  EntityId whose_turn;
  DijkstraGrid dijkstra;

  EntityPool movement_indicators;

  UserInput input;

  bool keep_going = true;
  SDL_Event e;
  Time t = now();
  while (keep_going && !input.pressed('q')) {
    input.poll(game);

    if (input.quit_requested) break;

    if (input.left_click) {
      std::cout << "click: " << input.mouse_pos_f << std::endl;
    }

    Time new_time = now();
    Milliseconds dt =
      std::chrono::duration_cast<std::chrono::milliseconds>(new_time - t);
    game.set_dt(new_time - t);

    // Check if we need to end the current turn, but wait until all scripts and
    // actions have completed first.
    if (!game.have_ordered_scripts() &&
        !game.popup_box() &&
        (game.turn().over() || !game.ecs().is_active(whose_turn))) {
      whose_turn = advance_until_next_turn(game.ecs());
      
      // TODO: We should in general be using this instead of whose_turn, but it
      // looks like I forgot it existed. This line allows scripts to know whose
      // turn it currently is.
      game.turn().reset();
      game.turn().actor = whose_turn;
      game.decision().type = Decision::DECIDING;

      if (whose_turn.id == EntityId::NOT_AN_ID)
        return Error("No one left alive");

      std::cout << "it is now the turn of "
        << game.ecs().read_or_panic<Actor>(whose_turn).name << std::endl;

      game.set_camera_target(
          game.ecs().read_or_panic<Transform>(whose_turn).pos);

      // Find this entity's walkable tiles.
      const GridPos& grid_pos = game.ecs().read_or_panic<GridPos>(whose_turn);
      dijkstra.generate(game, grid_pos.pos);

      auto move_range =
        game.ecs().read_or_panic<Actor>(whose_turn).stats.move;

      movement_indicators.deactivate_pool(game.ecs());
      // Add markers that show to where this entity can move.
      for (const auto& [pos, node] : dijkstra) {
        if (!node.dist || node.dist > move_range) continue;

        Marker marker(node.dist ? glm::vec4(0.1f, 0.2f, 0.4f, 0.5f)
                                : glm::vec4(1.0f, 1.0f, 1.0f, 0.3f));
        movement_indicators.create_new(game.ecs(),
                                       Transform{pos, Transform::OVERLAY},
                                       marker);
      }
    }

    const Actor& whose_turn_actor =
      game.ecs().read_or_panic<Actor>(whose_turn);
    const Agent& whose_turn_agent =
      game.ecs().read_or_panic<Agent>(whose_turn);
    const Transform& whose_turn_trans =
      game.ecs().read_or_panic<Transform>(whose_turn);

    if (game.turn().did_move) {
      movement_indicators.deactivate_pool(game.ecs());
    }

    // Decide on an action to take.
    if (game.popup_box()) {
      game.popup_box()->update(dt);

      // Popup boxes rob input from the player. If they click anywhere, they go
      // away.
      if (input.left_click) {
        TextBoxPopup::OnClickResponse r =
          game.popup_box()->on_left_click(input.mouse_pos_f);
        if (r == TextBoxPopup::DESTROY_ME) game.popup_box().reset();
      }
    } else if (game.have_ordered_scripts()) {
      // Any active scripts interrupt processing input.
    } else if (game.decision().type == Decision::DECIDING) {
      if (whose_turn_agent.team == Team::CPU) {
        cpu_decision(game, dijkstra, whose_turn);
      } else if (whose_turn_agent.team == Team::PLAYER) {
        player_decision(game, whose_turn, input);
      }
    }

    if (game.have_ordered_scripts()) {
      // We're already acting on the previous decision or script.
    } else if (game.decision().type == Decision::PASS) {
      game.turn().did_pass = true;
    } else if (game.decision().type == Decision::MOVE_TO) {
      game.set_camera_target(game.decision().move_to);

      Script script;
      push_move_along_path(script, whose_turn,
                           path_to(dijkstra, game.decision().move_to));
      game.add_ordered_script(std::move(script));
      game.turn().did_move = true;
      game.decision().type = Decision::DECIDING;
    } else if (game.decision().type == Decision::ATTACK_ENTITY) {
      Script attack_script;
      push_attack(attack_script, game, whose_turn, game.decision().target);
      game.add_ordered_script(std::move(attack_script));

      game.turn().did_action = true;
      game.decision().type = Decision::DECIDING;
    } else if (game.decision().type == Decision::LOOK_AT) {
      game.popup_box().reset(new TextBoxPopup(game));
      add_entity_desc_text(game, *game.popup_box(), game.decision().target);
      glm::vec2 pos = game.ecs().read_or_panic<GridPos>(
          game.decision().target).pos;
      game.popup_box()->build_text_box_next_to(pos);
      game.decision().type = Decision::DECIDING;
    } else if (game.decision().type == Decision::TALK) {
      // For positioning the text box, take advantage of the camera being
      // centered on the player.
      // TODO: Dialogues should have the camera center on the speaker. We can
      // then place the box relative to the speaker's position.
      glm::vec2 player_pos =
        game.ecs().read_or_panic<Transform>(whose_turn).pos;

      const Actor& other =
        game.ecs().read_or_panic<Actor>(game.decision().target);
      const Script* conversation = other.triggers.get_or_null("on_recruit");
      game.add_ordered_script(conversation ? *conversation :
                              will_not_talk_conversation());

      game.turn().did_action = true;
      game.decision().type = Decision::DECIDING;
    }

    if (!game.popup_box()) {
      game.execute_ordered_scripts();
    }
    game.execute_independent_scripts();

    for (const auto& [id, actor] : game.ecs().read_all<Actor>())
      if (actor.hp == 0) game.ecs().mark_to_delete(id);
    game.ecs().deleted_marked_ids();

    gl::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    game.smooth_camera_towards_target(dt);

    for (const auto& [_, transform, render_configs] :
         game.ecs().read_all<Transform, std::vector<GlyphRenderConfig>>()) {
      for (const GlyphRenderConfig& rc : render_configs) {
        render_tasks.add_glyph_task(game, transform, rc);
      }
    }

    for (const auto& [_, transform, marker] :
         game.ecs().read_all<Transform, Marker>()) {
      render_tasks.add_marker_task(game, transform, marker);
    }

    render_tasks.add_marker_task(
        game, 
        game.ecs().read_or_panic<Transform>(whose_turn),
        Marker{glm::vec4(1.f, 1.f, 1.f, 0.1f)});

    render_tasks.add_marker_task(
        game, Transform{input.mouse_pos, Transform::OVERLAY},
        Marker{glm::vec4(0.1f, 0.3f, 0.6f, .5f)});

    render_tasks.execute_tasks_and_clear(game);

    gfx.swap_buffers();

    t = new_time;
  }

  return Error();
}

int main() {
  if (Error e = run(); !e.ok) {
    std::cerr << "Error: " << e.reason << std::endl;
    return 1;
  }
  return 0;
}
