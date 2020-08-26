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

constexpr int WINDOW_HEIGHT = 640;
constexpr int WINDOW_WIDTH = 480;

using Time = std::chrono::high_resolution_clock::time_point;
using Milliseconds = std::chrono::milliseconds;

Time now() {
  return std::chrono::high_resolution_clock::now();
}

std::ostream& operator<<(std::ostream& os, glm::ivec2 v) {
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

constexpr float TEXT_SCALE = 0.75f;

float text_width(FontMap& font_map, std::string_view text) {
  auto get_width =
    [&font_map](char c) { return font_map.get(c).bottom_right.x; };
  float w = reduce_by(text, 0.0f, get_width);
  // There will be some trailing space at the start. Make it the same at the
  // end.
  if (text.size()) w += font_map.get(text[0]).top_left.x * TEXT_SCALE;
  // Add spacing between the letters.
  w += text.size() * TILE_SIZE * 0.1f;
  return w;
}

// Print's an entity's description next to its location. The view of the entity
// itself shall be unobstructed and the text will always be on screen.
void render_entity_desc(Game& game, EntityId id) {
  GridPos grid_pos = game.ecs().read_or_panic<GridPos>(id);

  const Actor* actor = nullptr;
  game.ecs().read(id, &actor);

  std::vector<std::string> lines;
  if (actor) {
    lines.push_back(actor->name);
    lines.push_back(concat_strings("HP: ", std::to_string(actor->stats.hp),
                                   "/", std::to_string(actor->stats.max_hp)));
    lines.push_back(concat_strings("STR: ",
                                   std::to_string(actor->stats.strength)));
  } else {
    lines.push_back("UNKNOWN");
  }

  auto get_width = [&game](const std::string& s) {
    return text_width(game.text_font_map(), s);
  };
  float w = max_by(lines, 0.0f, get_width);
  float h = lines.size() * TEXT_SCALE;

  glm::vec2 start = glm::vec2(grid_pos.pos) + glm::vec2(1.f, 0.f);
  if (start.x + w > game.bottom_right_screen_tile().x)
    start.x -= 2.f + w;
  start.y = std::max(start.y, game.bottom_right_screen_tile().y + h);

  glm::vec2 end = start + glm::vec2(w, -h);
  glm::vec2 center = (start + end) / 2.f;
  glm::vec3 graphical_center = game.to_graphical_pos(center, 0);

  game.marker_shader().render_marker(
      graphical_center,
      TILE_SIZE, glm::vec4(0.f, 0.f, 0.f, .9f),
      glm::vec2(w, h));

  for (unsigned int i = 0; i < lines.size(); ++i) {
    float cursor = center.x - w/2.f + 0.5f;
    const float highest = center.y + h/2.f - TEXT_SCALE/2.f;
    for (char c : lines[i]) {
      glm::vec2 pos = glm::vec2(cursor, highest - i * TEXT_SCALE);
      const Glyph& glyph = game.text_font_map().get(c);
      GlyphRenderConfig rc(glyph, glm::vec4(1.f));
      game.glyph_shader().render_glyph(game.to_graphical_pos(pos, 0.f),
                                       TILE_SIZE * TEXT_SCALE, rc);

      cursor += glyph.bottom_right.x + TILE_SIZE * TEXT_SCALE * 0.1f;
    }
  }
}

// When an actor wants to take a turn, its "SPD" or "speed" stat contributes to
// its initial "energy" which then accrues over time. One tick, one energy. The
// entity with the most energy, or more than a given threshold, shall go next
// and then have its energy reverted to the value of its speed. Ties are won by
// the entity with the smallest ID.
//
// At least one tick shall pass between turns. If an entity's energy is above
// the threshold, after its turn, its energy becomes the overflow plus its
// speed.
//
// The goal is that an entity with twice the speed of another shall move
// roughly twice as often.
EntityId pick_next(Ecs& ecs) {
  constexpr int THRESHHOLD = 1000;

  std::vector<std::pair<EntityId, int*>> energies;
  for (auto [id, agent] : ecs.read_all<Agent>())
    energies.emplace_back(id, &agent.energy);

  if (energies.empty()) return EntityId();
  
  auto cmp = [](const auto& id_energy_a, const auto& id_energy_b) {
    return std::make_pair(*id_energy_a.second, id_energy_a.first) <
           std::make_pair(*id_energy_b.second, id_energy_b.first);
  };
  sort(energies, cmp);

  unsigned int energy_taken =
    std::max(1, THRESHHOLD - *energies.back().second);
  for (auto& id_energy : energies) *id_energy.second += energy_taken;


  const auto& [id, energy] = energies.back();
  *energy -= THRESHHOLD;
  *energy += ecs.read_or_panic<Actor>(id).stats.speed;

  ecs.write(id, ActorState::SETUP, Ecs::CREATE_OR_UPDATE);
  return id;
}

struct UserInput {
  glm::ivec2 mouse_pos;
  bool left_click;
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
    mouse_pos = (mouse_screen_pos + TILE_SIZE/2) / TILE_SIZE +
                game.camera_offset() / TILE_SIZE;

    left_click = false;
    keys_pressed.clear();
  }
};

Error run() {
  Graphics gfx;
  if (Error e = gfx.init(WINDOW_WIDTH, WINDOW_HEIGHT); !e.ok) return e;

  Game game;
  if (Error e = game.init(); !e.ok) return e;

  gl::clearColor(0.f, 0.f, 0.f, 1.f);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  glOrtho(-1, 1, -1, 1, -10, 10);

  GLuint vbo_elems[] = {0, 1, 2,
                        2, 3, 0};
  GLuint vbo_elems_id = gl::genBuffer();
  gl::bindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_elems_id);
  gl::bufferData(GL_ELEMENT_ARRAY_BUFFER, vbo_elems, GL_STATIC_DRAW);

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
  game.set_grid(arena_grid({50, 50}, wall, floor));

  EntityId player;
  {
    GlyphRenderConfig rc(game.font_map().get('@'),
                         glm::vec4(.9f, .6f, .1f, 1.f));
    rc.center();
    Stats stats{.hp = 10, .max_hp = 10, .strength = 5, .speed = 10};
    player = game.ecs().write_new_entity(Transform{{3.f, 3.f}, -1},
                                         GridPos{{3, 3}},
                                         std::vector{rc},
                                         Actor{"player", stats},
                                         Agent{10, Team::PLAYER},
                                         ActorState::SETUP);
  }

  EntityId spider;
  {
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

    Stats stats{.hp = 10, .max_hp = 10, .strength = 5, .speed = 8};
    spider = game.ecs().write_new_entity(Transform{{12.f, 12.f}, -1},
                                         GridPos{{12, 12}},
                                         Agent{8, Team::CPU},
                                         rcs, Actor{"spider", stats});
  }

  EntityId whose_turn;
  DijkstraGrid dijkstra;
  bool turn_over = true;
  bool did_move = false;
  bool did_action = false;

  UserInput input;

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
          break;
        }
      }
    }

    Time new_time = now();
    Milliseconds dt =
      std::chrono::duration_cast<std::chrono::milliseconds>(new_time - t);

    if (turn_over || !game.ecs().is_active(whose_turn)) {
      whose_turn = pick_next(game.ecs());
      if (whose_turn.id == EntityId::NOT_AN_ID)
        return Error("No one left alive");

      std::cout << "it is now the turn of " << game.ecs().read_or_panic<Actor>(whose_turn).name << std::endl;

      turn_over = false;
      did_move = false;
      did_action = false;
    }

    ActorState& whose_turn_state =
      game.ecs().read_or_panic<ActorState>(whose_turn);
    Agent& whose_turn_agent =
      game.ecs().read_or_panic<Agent>(whose_turn);

    if (whose_turn_state == ActorState::SETUP) {
      const GridPos& grid_pos = game.ecs().read_or_panic<GridPos>(whose_turn);
      dijkstra.generate(game, grid_pos.pos);
      whose_turn_state = ActorState::DECIDING;
    }

    bool act = false;
    glm::ivec2 action_pos;
    if (whose_turn_state == ActorState::DECIDING) {
      if (whose_turn_agent.team == Team::CPU) {
        // Move towards the player until in range.
        if (!did_action && !did_move) {
          auto [enemy_loc, pos] = nearest_enemy_location(
              game, dijkstra, whose_turn, Team::CPU);
          if (enemy_loc && enemy_loc->dist <= 5 + 1) {
            action_pos = pos;
          } else if (enemy_loc) {
            action_pos = rewind_until(dijkstra, pos, 5);
          }
          act = true;
        }

        if (!act) turn_over = true;
      } else {
        act = input.left_click;
        action_pos = input.mouse_pos;
        if (input.pressed(' ')) turn_over = true;
      }
    }

    if (act && whose_turn_state == ActorState::DECIDING && !did_action &&
        dijkstra.contains(action_pos)) {
      const DijkstraNode& dnode = dijkstra.at(action_pos);
      if (dnode.entity && (dnode.dist == 1 || (!did_move && dnode.dist < 6))) {
        Path path = dnode.dist > 1 ? path_to(dijkstra, dnode.prev) : Path{};
        game.ecs().write(whose_turn,
                         mele_action(game.ecs(), whose_turn, dnode.entity,
                                     path),
                         Ecs::CREATE_OR_UPDATE);
        did_action = true;
        did_move |= !path.empty();
        whose_turn_state = ActorState::TAKING_TURN;
      }
    }

    if (act && whose_turn_state == ActorState::DECIDING && !did_move &&
        dijkstra.contains(action_pos)) {
      const DijkstraNode& dnode = dijkstra.at(action_pos);
      if (!dnode.entity && dnode.dist <= 5) {
        auto action = move_action(whose_turn, path_to(dijkstra, action_pos));
        game.ecs().write(whose_turn, std::move(action), Ecs::CREATE_OR_UPDATE);
        did_move = true;
        whose_turn_state = ActorState::TAKING_TURN;
      }
    }

    // See Action documentation for why this exists.
    std::vector<std::function<void()>> deferred_events;
    for (auto [id, action] : game.ecs().read_all<ActionPtr>()) {
      if (!action) continue;
      action->run(game, dt, deferred_events);
      if (id == whose_turn && action->finished()) {
        action.reset();
        whose_turn_state = ActorState::SETUP;
        std::cout << "whose_turn's finished" << std::endl;  
      }
    }

    // The next frame, a new ID will be chosen to take their turn.
    if (whose_turn_state == ActorState::SETUP &&
        (turn_over || (did_move && did_action))) {
      whose_turn_state = ActorState::WAITING;
      turn_over = true;
    }

    for (std::function<void()>& f : deferred_events) f();

    for (const auto& [id, actor] : game.ecs().read_all<Actor>())
      if (actor.stats.hp == 0) game.ecs().mark_to_delete(id);
    game.ecs().deleted_marked_ids();

    gl::clear();

    for (const auto& [_, transform, render_configs] :
         game.ecs().read_all<Transform, std::vector<GlyphRenderConfig>>()) {
      for (const GlyphRenderConfig& rc : render_configs) {
        game.glyph_shader().render_glyph(game.to_graphical_pos(transform),
                                         TILE_SIZE, rc);
      }
    }

    if (!did_move) for (const auto& [pos, node] : dijkstra) {
      if (node.dist > 5) continue;
      game.marker_shader().render_marker(
          game.to_graphical_pos(pos, 0),
          TILE_SIZE, glm::vec4(0.1f, 0.2f, 0.4f, 0.5f));
    }

    game.marker_shader().render_marker(
        game.to_graphical_pos(input.mouse_pos, 0),
        TILE_SIZE, glm::vec4(0.1f, 0.3f, 0.6f, .5f));

    if (auto [id, exists] = actor_at(game.ecs(), input.mouse_pos); exists) {
      render_entity_desc(game, id);
    }

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
