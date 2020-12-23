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

#include "action.h"
#include "ecs.h"
#include "components.h"
#include "dijkstra.h"
#include "font.h"
#include "game.h"
#include "glpp.h"
#include "grid.h"
#include "graphics.h"
#include "math.h"
#include "shaders.h"
#include "timer.h"
#include "ui.h"

constexpr int WINDOW_HEIGHT = 800;
constexpr int WINDOW_WIDTH = 800;

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

std::vector<Path> expand_walkable(const Game& game,
                                  glm::ivec2 start, unsigned int max_dist) {
  std::unordered_set<glm::ivec2> taken_spaces;
  for (const auto [unused_id, pos, unused_actor] :
       game.ecs().read_all<GridPos, Actor>())
    taken_spaces.insert(pos.pos);
  
  std::deque<Path> edges = {{start}};
  std::unordered_set<glm::ivec2> visited;
  std::vector<Path> paths;
  while (!edges.empty()) {
    Path path = std::move(edges.front());
    edges.pop_front();

    if (!visited.insert(path.back()).second) continue;
    if (glm::ivec2(path.back()) != start) paths.push_back(path);

    if (path.size() + 1 >= max_dist) continue;
    for (glm::ivec2 step : adjacent_steps()) {
      glm::vec2 next_pos = glm::vec2(step) + path.back();
      auto [tile, in_grid] = game.grid().get(next_pos);
      if (!in_grid || !tile.walkable || taken_spaces.contains(next_pos))
        continue;

      Path new_path = path;
      new_path.push_back(next_pos);
      edges.push_back(new_path);
    }
  }

  return paths;
}

struct PossibleAttack {
  Path path;
  EntityId defender;
  glm::ivec2 defender_pos;
};

std::vector<PossibleAttack> expand_attacks(const Ecs& ecs,
                                           glm::ivec2 start,
                                           const std::vector<Path>& paths) {
  std::vector<PossibleAttack> possible_attacks;
  for (glm::ivec2 step : adjacent_steps()) {
    glm::ivec2 pos = step + start;
    auto [id, is_there] = actor_at(ecs, pos);
    if (is_there) possible_attacks.push_back({{}, id, pos});
  }
  for (const Path& path : paths) {
    for (glm::ivec2 step : adjacent_steps()) {
      glm::ivec2 pos = step + glm::ivec2(path.back());
      auto [id, is_there] = actor_at(ecs, pos);
      if (is_there) possible_attacks.push_back({path, id, pos});
    }
  }
  return possible_attacks;
}

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
  
  ecs.write(max_agent->id, ActorState::SETUP, Ecs::CREATE_OR_UPDATE);
  return max_agent->id;
}

struct UserInput {
  glm::vec2 mouse_pos_f;
  glm::ivec2 mouse_pos;
  bool left_click;
  bool right_click;
  // Inclusion in this set means that a key is pressed.
  std::unordered_set<char> keys_pressed;

  bool pressed(char c) { return keys_pressed.contains(c); }
  void press(char c) { keys_pressed.insert(c); }

  void reset(const Game& game) {
    // The selector graphically represents what tile the mouse is hovering
    // over. This might be a bit round-a-bout, but find where to place the
    // selector on screen by finding its position on the grid. Later, we
    // convert it to a grid-a-fied screen position.
    int mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    glm::vec2 mouse_screen_pos(float(mouse_x * 2) / WINDOW_WIDTH - 1,
                               float(-mouse_y * 2) / WINDOW_HEIGHT + 1);
    mouse_pos_f = (mouse_screen_pos + TILE_SIZE/2) / TILE_SIZE +
                  game.camera_offset() / TILE_SIZE;
    mouse_pos = glm::floor(mouse_pos_f);

    left_click = false;
    right_click = false;
    keys_pressed.clear();
  }
};

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

  actor.on_hit_enemy.push_back([guy](Game& game, ActionManager& manager)
                               -> ScriptResult {
      glm::ivec2 pos = game.ecs().read_or_panic<GridPos>(guy).pos;

      EntityId defender = game.decision().attack_target;
      glm::ivec2 defender_pos =
          game.ecs().read_or_panic<GridPos>(defender).pos;

      glm::ivec2 target_tile = defender_pos + (defender_pos - pos);
      
      auto [tile, exists] = game.grid().get(target_tile);
      if (exists && tile.walkable) {
        manager.add_ordered_action(
            move_action(defender, {defender_pos, target_tile}));
      }

      return ScriptResult::CONTINUE;
  });
      
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

bool can_attack(const Game& game, glm::ivec2 from_pos, unsigned int attack_range,
                EntityId target) {
  return !game.turn().did_action &&
         diamond_dist(game.ecs().read_or_panic<GridPos>(target).pos, from_pos)
         <= attack_range;
}

bool can_talk(const Game& game, glm::ivec2 from_pos, EntityId target) {
  return can_attack(game, from_pos, 3, target);
}

Decision player_decision(const Game& game, EntityId id, const UserInput& input) {
  Decision decision;
  if (!input.left_click) return decision;

  glm::ivec2 pos = game.ecs().read_or_panic<GridPos>(id).pos;
  auto [enemy, exists] = actor_at(game.ecs(), input.mouse_pos);

  const Actor& actor = game.ecs().read_or_panic<Actor>(id);
  if (pos == input.mouse_pos) {
    decision.type = Decision::PASS;
  } else if (exists && can_attack(game, pos, actor.stats.range, enemy)) {
    decision.type = Decision::ATTACK_ENTITY;
    decision.attack_target = enemy;
  } else if (!exists && manh_dist(pos, input.mouse_pos) <= actor.stats.move) {
    decision.type = Decision::MOVE_TO;
    decision.move_to = input.mouse_pos;
  }

  return decision;
}

std::vector<EntityId> enemies_in_range(const Ecs& ecs, Team team,
                                       glm::ivec2 pos, unsigned int range) {
  std::vector<EntityId> enemies;
  for (const auto& [id, grid_pos, agent] : ecs.read_all<GridPos, Agent>()) {
    if (agent.team != team && manh_dist(pos, grid_pos.pos) <= range)
      enemies.push_back(id);
  }
  return enemies;
}

Decision cpu_decision(const Game& game, const DijkstraGrid& dijkstra,
                      EntityId id) {
  Decision decision;

  const Agent& agent = game.ecs().read_or_panic<Agent>(id);
  const Actor& actor = game.ecs().read_or_panic<Actor>(id);

  if (!game.turn().did_action) {
    std::vector<EntityId> enemies =
      enemies_in_range(game.ecs(), agent.team,
                       game.ecs().read_or_panic<GridPos>(id).pos,
                       actor.stats.range);
    if (enemies.size()) {
      decision.type = Decision::ATTACK_ENTITY;
      decision.attack_target = enemies.front();
    }
  }

  if (decision.type == Decision::WAIT && !game.turn().did_action &&
      !game.turn().did_move) {
    auto [enemy_loc, enemy_pos] = nearest_enemy_location(game, dijkstra, id,
                                                         agent.team);
    if (enemy_loc) {
      decision.type = Decision::MOVE_TO;
      decision.move_to = rewind_until(
          dijkstra, enemy_pos,
          [&](glm::ivec2 pos, const DijkstraNode& node) {
          return pos != enemy_pos &&
          node.dist <= actor.stats.move;
          });
    }
  }

  if (decision.type == Decision::WAIT) decision.type = Decision::PASS;

  return decision;
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
  make_spider(game, spawn_agent(game, "spiider", {10, 12}, Team::CPU));
  make_bat(game, spawn_agent(game, "bat", {10, 10}, Team::CPU));


  EntityId whose_turn;
  DijkstraGrid dijkstra;

  EntityPool movement_indicators;

  UserInput input;

  ActionManager action_manager;
  ScriptEngine active_script;

  bool keep_going = true;
  SDL_Event e;
  Time t = now();
  while (keep_going && !input.pressed('q')) {
    input.reset(game);

    while (SDL_PollEvent(&e) != 0) {
      switch (e.type) {
        case SDL_QUIT: keep_going = false; break;
        case SDL_KEYDOWN: input.press(e.key.keysym.sym); break;
        case SDL_MOUSEBUTTONDOWN: {
          if (e.button.button == SDL_BUTTON_LEFT)
            input.left_click = true;
          if (e.button.button == SDL_BUTTON_RIGHT)
            input.right_click = true;
          break;
        }
      }
    }

    Time new_time = now();
    Milliseconds dt =
      std::chrono::duration_cast<std::chrono::milliseconds>(new_time - t);

    // Check if we need to end the current turn, but wait until all scripts and
    // actions have completed first.
    if (!active_script.active() && !action_manager.have_ordered_actions() &&
        !game.popup_box() &&
        (game.turn().over() || !game.ecs().is_active(whose_turn))) {
      whose_turn = advance_until_next_turn(game.ecs());
      if (whose_turn.id == EntityId::NOT_AN_ID)
        return Error("No one left alive");

      std::cout << "it is now the turn of " << game.ecs().read_or_panic<Actor>(whose_turn).name << std::endl;

      game.turn().reset();

      game.set_camera_target(
          game.ecs().read_or_panic<Transform>(whose_turn).pos);

      ActorState& whose_turn_state =
        game.ecs().read_or_panic<ActorState>(whose_turn);
      whose_turn_state = ActorState::SETUP;
    }

    ActorState& whose_turn_state =
      game.ecs().read_or_panic<ActorState>(whose_turn);
    const Actor& whose_turn_actor =
      game.ecs().read_or_panic<Actor>(whose_turn);
    const Agent& whose_turn_agent =
      game.ecs().read_or_panic<Agent>(whose_turn);
    const Transform& whose_turn_trans =
      game.ecs().read_or_panic<Transform>(whose_turn);

    if (whose_turn_state == ActorState::SETUP) {
      const GridPos& grid_pos = game.ecs().read_or_panic<GridPos>(whose_turn);
      std::cout << "Generating new dijkstra" << std::endl;
      dijkstra.generate(game, grid_pos.pos);
      std::cout << "generated" << std::endl;

      movement_indicators.deactivate_pool(game.ecs());
      // Add markers that show to where this entity can move.
      for (const auto& [pos, node] : dijkstra) {
        if (!node.dist || node.dist > whose_turn_actor.stats.move) continue;

        Marker marker(node.dist ? glm::vec4(0.1f, 0.2f, 0.4f, 0.5f)
                                : glm::vec4(1.0f, 1.0f, 1.0f, 0.3f));
        movement_indicators.create_new(game.ecs(),
                                       Transform{pos, Transform::OVERLAY},
                                       marker);
      }

      whose_turn_state = ActorState::DECIDING;
    }

    if (game.turn().did_move && !game.turn().waiting) {
      movement_indicators.deactivate_pool(game.ecs());
    }

    if (game.popup_box()) {
      // Popup boxes rob input from the player. If they click anywhere, they go
      // away.
      if (input.left_click) {
        game.popup_box()->on_left_click(input.mouse_pos_f);
        game.popup_box().reset();
      }
    } else if (action_manager.have_ordered_actions() || active_script.active()) {
      // Any active scripts interrupt processing input.
    } else if (whose_turn_state == ActorState::DECIDING) {
      if (whose_turn_agent.team == Team::CPU) {
        game.decision() = cpu_decision(game, dijkstra, whose_turn);
      } else if (whose_turn_agent.team == Team::PLAYER) {

        if (input.right_click) {
          // Bring up a selection menu if something is there.
          auto [id, exists] = actor_at(game.ecs(), input.mouse_pos);
          if (exists) {
            auto look_at = [&game, id=id] {
              game.decision().type = Decision::LOOK_AT;
              game.decision().attack_target = id;
            };
            game.popup_box().reset(new SelectionBox(game));
            game.popup_box()->add_text_with_onclick("look", look_at);

            glm::vec2 pos = game.ecs().read_or_panic<GridPos>(whose_turn).pos;
            if (can_attack(game, pos, whose_turn_actor.stats.range, id)) {
              auto attack = [&game, id=id] {
                game.decision().type = Decision::ATTACK_ENTITY;
                game.decision().attack_target = id;
              };
              game.popup_box()->add_text_with_onclick("normal attack", attack);
            }

            if (can_talk(game, pos, id)) {
              auto talk = [&game, id=id] {
                game.decision().type = Decision::TALK;
                game.decision().attack_target = id;
              };
              game.popup_box()->add_text_with_onclick("talk", talk);
            }

            game.popup_box()->build_text_box_next_to(input.mouse_pos);
          }
        }
        game.decision() = player_decision(game, whose_turn, input);
      }
    }

    if (action_manager.have_ordered_actions() || active_script.active()) {
      // We're already acting on the previous decision or script.
    } else if (whose_turn_state == ActorState::DECIDING &&
        game.decision().type == Decision::PASS) {
      game.turn().did_pass = true;
    } else if (whose_turn_state == ActorState::DECIDING &&
               game.decision().type == Decision::WAIT) {
      // noop
    } else if (whose_turn_state == ActorState::DECIDING &&
               game.decision().type == Decision::MOVE_TO) {
      game.set_camera_target(game.decision().move_to);

      action_manager.add_ordered_sequence(
          move_action(whose_turn, path_to(dijkstra, game.decision().move_to)),
          generic_action([&waiting = game.turn().waiting] (const auto&...)
                         { waiting = false; }));
      game.turn().did_move = true;
      game.turn().waiting = true;
      whose_turn_state = ActorState::DECIDING;
    } else if (whose_turn_state == ActorState::DECIDING &&
               game.decision().type == Decision::ATTACK_ENTITY) {
      // Set the camera at the midpoint between attacker and defender.
      glm::vec2 attack_target_pos =
        game.ecs().read_or_panic<Transform>(game.decision().attack_target).pos;
      game.set_camera_target(glm::mix(whose_turn_trans.pos,
                                      attack_target_pos,
                                      0.5f));

      // Do the mele, but afterwards, reset the camera and continue the turn.
      std::vector<std::unique_ptr<Action>> sequence;
      sequence.push_back(
          mele_action(game.ecs(), whose_turn,
                      game.decision().attack_target, Path()));

      // TODO: It would be much cleaner to check this withing mele_action().
      if (!whose_turn_actor.on_hit_enemy.empty()) {
        sequence.push_back(
          generic_action([&active_script, script=whose_turn_actor.on_hit_enemy]
                         (const auto&...) {
              active_script.reset(std::move(script));
        }));
      }

      sequence.push_back(
          generic_action([&game, whose_turn] (const auto&...) {
            game.set_camera_target(
                game.ecs().read_or_panic<Transform>(whose_turn).pos);
            game.turn().waiting = false;
          }));

      action_manager.move_ordered_sequence(std::move(sequence));

      game.turn().did_action = true;
      game.turn().waiting = true;
      whose_turn_state = ActorState::DECIDING;
    } else if (whose_turn_state == ActorState::DECIDING &&
               game.decision().type == Decision::LOOK_AT) {
      game.popup_box().reset(new TextBoxPopup(game));
      add_entity_desc_text(game, *game.popup_box(), game.decision().attack_target);
      glm::vec2 pos = game.ecs().read_or_panic<GridPos>(
          game.decision().attack_target).pos;
      game.popup_box()->build_text_box_next_to(pos);
      game.decision().type = Decision::WAIT;
    } else if (whose_turn_state == ActorState::DECIDING &&
               game.decision().type == Decision::TALK) {
      game.popup_box().reset(new TextBoxPopup(game));
      game.popup_box()->add_text("(they have nothing to say to you)");

      // For positioning the text box, take advantage of the camera being
      // centered on the player.
      // TODO: Dialogues should have the camera center on the speaker. We can
      // then place the box relative to the speaker's position.
      glm::vec2 player_pos =
        game.ecs().read_or_panic<Transform>(whose_turn).pos;
      game.popup_box()->build_text_box_at(player_pos + glm::vec2(-10, -2),
                                          player_pos + glm::vec2( 10, -6));

      game.turn().did_action = true;
      game.decision().type = Decision::WAIT;
    }

    if (action_manager.have_ordered_actions()) {
      action_manager.process_ordered_actions(game, dt);
    } else if (!game.popup_box() && active_script.active())  {
      active_script.run(game, action_manager);
    }

    action_manager.process_independent_actions(game, dt);

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
