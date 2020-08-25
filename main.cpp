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

std::pair<EntityId, bool> actor_at(const Ecs& ecs, glm::ivec2 pos) {
  for (const auto [id, epos, unused_actor] :
       ecs.read_all<GridPos, Actor>())
    if (epos.pos == pos) return {id, true};
  return {EntityId(), false};
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

float text_width(FontMap& font_map, std::string_view text) {
  auto get_width =
    [&font_map](char c) { return font_map.get(c).bottom_right.x; };
  float w = reduce_by(text, 0.0f, get_width);
  // There will be some trailing space at the start. Make it the same at the
  // end.
  if (text.size()) w += font_map.get(text[0]).top_left.x;
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

  auto get_width =
    [&game](const std::string& s) { return text_width(game.font_map(), s); };
  float w = max_by(lines, 0.0f, get_width);

  glm::vec2 start = glm::vec2(grid_pos.pos) + glm::vec2(1.f, 0.f);
  glm::vec2 end = start + glm::vec2(w, -float(lines.size()) + 1.f);
  glm::vec2 center = (start + end) / 2.f;
  glm::vec3 graphical_center = game.to_graphical_pos(center, 0);

  // Check that it's not overflowing offscreen. Maybe move it to the other
  // side of the entity whose information we're printing.
  glm::vec3 graphical_end = game.to_graphical_pos(end, 0);
  if (graphical_end.x > 1.f) {
    glm::vec2 offset = 2.f * (glm::vec2(grid_pos.pos) - center); 
    graphical_center = game.to_graphical_pos(offset + center, 0);
    start += offset;
  }

  game.marker_shader().render_marker(
      graphical_center,
      TILE_SIZE, glm::vec4(0.f, 0.f, 0.f, .9f),
      glm::vec2(w, float(lines.size())));

  for (unsigned int i = 0; i < lines.size(); ++i) {
    float cursor = start.x + 0.5f;
    for (char c : lines[i]) {
      glm::vec2 pos = glm::vec2(cursor, start.y - i);
      const Glyph& glyph = game.font_map().get(c);
      GlyphRenderConfig rc(glyph, glm::vec4(1.f));
      game.glyph_shader().render_glyph(game.to_graphical_pos(pos, 0.f),
                                       TILE_SIZE, rc);

      cursor += glyph.bottom_right.x + TILE_SIZE * 0.1f;
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

struct DijkstraNode {
  glm::ivec2 prev;
  unsigned int dist;
  EntityId entity;

  DijkstraNode(EntityId entity)
      : entity(entity) {
    prev = {0, 0};
    dist = std::numeric_limits<int>::max();
  }

  DijkstraNode() : DijkstraNode(EntityId()) { }
};

class DijkstraGrid {
  std::unordered_map<glm::ivec2, DijkstraNode> nodes_;

public:
  void generate(const Game& game, glm::ivec2 source);

  DijkstraNode& at(glm::ivec2 pos) { return nodes_.at(pos); }
  const DijkstraNode& at(glm::ivec2 pos) const { return nodes_.at(pos); }

  auto begin() { return nodes_.begin(); }
  auto begin() const { return nodes_.begin(); }
  auto end() { return nodes_.end(); }
  auto end() const { return nodes_.end(); }
};

static glm::ivec2 min_node(
    const std::unordered_map<glm::ivec2, DijkstraNode>& nodes,
    const std::unordered_set<glm::ivec2>& Q) {
  glm::ivec2 min_pos;
  unsigned int min_dist = std::numeric_limits<int>::max();
  for (const glm::ivec2& pos : Q) {
    if (nodes.at(pos).dist < min_dist) {
      min_pos = pos;
      min_dist = nodes.at(pos).dist;
    }
  }

  return min_pos;
}

void DijkstraGrid::generate(const Game& game, glm::ivec2 source) {
  nodes_.clear();

  std::unordered_set<glm::ivec2> Q;
  for (const auto& [pos, tile] : game.grid()) {
    if (!tile.walkable) continue;
    auto [entity, entity_exists] = actor_at(game.ecs(), pos);
    // We want paths to nodes with entities on them, but we do not want to
    // expand them or paths would go right through!
    if (!entity) Q.insert(pos);

    nodes_.emplace(pos, DijkstraNode(entity));
  }
  nodes_[source].dist = 0;
  Q.insert(source);

  while (!Q.empty()) {
    glm::ivec2 pos = min_node(nodes_, Q);
    Q.erase(pos);

    DijkstraNode& node = nodes_.at(pos);

    for (glm::ivec2 next_pos : adjacent_positions(pos)) {
      auto next_node = nodes_.find(next_pos);
      if (next_node == nodes_.end()) continue;

      unsigned int alt = node.dist + 1;
      if (alt < next_node->second.dist) {
        next_node->second.dist = alt;
        next_node->second.prev = pos;
      }
    }
  }
}

std::vector<glm::ivec2> ipath_to(const DijkstraGrid& dijkstra,
                                 glm::ivec2 pos) {
  const DijkstraNode* node = &dijkstra.at(pos);

  std::vector<glm::ivec2> path;
  path.resize(node->dist + 1);
  path[node->dist] = pos;
  while (node->dist) {
    pos = node->prev;
    node = &dijkstra.at(pos);
    path[node->dist] = pos;
  }

  return path;
}

std::vector<glm::vec2> path_to(const DijkstraGrid& dijkstra, glm::ivec2 pos) {
  auto ipath = ipath_to(dijkstra, pos);
  std::vector<glm::vec2> path(ipath.begin(), ipath.end());
  return path;
}


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
  tile_types['.'] = Tile{.walkable = true, .glyph = '.',
                         .fg_color = glm::vec4(.23f, .23f, .23f, 1.f),
                         .bg_color = glm::vec4(.2f, .2f, .2f, 1.f)};
  tile_types['#'] = Tile{.walkable = false, .glyph = '#',
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
  game.set_grid(grid_from_string(STARTING_GRID, tile_types));

  EntityId player;
  {
    GlyphRenderConfig rc(game.font_map().get('@'),
                         glm::vec4(.9f, .6f, .1f, 1.f));
    rc.center();
    Stats stats{.hp = 10, .max_hp = 10, .strength = 5, .speed = 10};
    player = game.ecs().write_new_entity(Transform{{5.f, 5.f}, -1},
                                         GridPos{{5, 5}},
                                         rc, Actor{"player", stats},
                                         Agent{10},
                                         ActorState::SETUP);
  }

  EntityId spider;
  {
    GlyphRenderConfig rc(game.font_map().get('s'),
                         glm::vec4(0.f, 0.2f, 0.6f, 1.f));
    rc.center();
    Stats stats{.hp = 10, .max_hp = 10, .strength = 5, .speed = 8};
    spider = game.ecs().write_new_entity(Transform{{4.f, 4.f}, -1},
                                         GridPos{{4, 4}},
                                         Agent{8},
                                         rc, Actor{"spider", stats});
  }

  EntityId whose_turn;
  DijkstraGrid dijkstra;
  bool turn_over = true;
  bool did_move = false;
  bool did_action = false;

  bool keep_going = true;
  SDL_Event e;
  Time t = now();
  bool mouse_down = false;
  while (keep_going) {
    while (SDL_PollEvent(&e) != 0) {
      switch (e.type) {
        case SDL_QUIT: keep_going = false;
        case SDL_KEYDOWN: {
          switch (e.key.keysym.sym) {
            case 'q': keep_going = false;
          }
          break;
        }
        case SDL_MOUSEBUTTONDOWN:{
          if (e.button.button == SDL_BUTTON_LEFT)
            mouse_down = true;
          break;
        }
      }
    }

    Time new_time = now();
    Milliseconds dt =
      std::chrono::duration_cast<std::chrono::milliseconds>(new_time - t);

    // The selector graphically represents what tile the mouse is hovering
    // over. This might be a bit round-a-bout, but find where to place the
    // selector on screen by finding its position on the grid. Later, we
    // convert it to a grid-a-fied screen position.
    int mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    glm::vec2 mouse_screen_pos(float(mouse_x * 2) / WINDOW_WIDTH - 1,
                               float(-mouse_y * 2) / WINDOW_HEIGHT + 1);
    glm::ivec2 mouse_grid_pos = (mouse_screen_pos + TILE_SIZE/2) / TILE_SIZE +
                                game.camera_offset() / TILE_SIZE;

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

    if (whose_turn_state == ActorState::SETUP) {
      const GridPos& grid_pos = game.ecs().read_or_panic<GridPos>(whose_turn);
      dijkstra.generate(game, grid_pos.pos);
      whose_turn_state = ActorState::DECIDING;
    }

    auto [defender, defender_exists] = actor_at(game.ecs(), mouse_grid_pos);

    if (defender_exists && mouse_down &&
        whose_turn_state == ActorState::DECIDING && !did_action) {
      const DijkstraNode& dnode = dijkstra.at(mouse_grid_pos);
      if (dnode.dist == 1 || !did_move) {
        Path path = dnode.dist > 1 ? path_to(dijkstra, dnode.prev) : Path{};
        game.ecs().write(whose_turn,
                         mele_action(game.ecs(), whose_turn, defender, path),
                         Ecs::CREATE_OR_UPDATE);
        did_action = true;
        did_move |= !path.empty();
        whose_turn_state = ActorState::TAKING_TURN;
      }
    }

    if (!defender_exists && mouse_down &&
        whose_turn_state == ActorState::DECIDING && !did_move) {
      auto action = move_action(whose_turn, path_to(dijkstra, mouse_grid_pos));
      game.ecs().write(whose_turn, std::move(action), Ecs::CREATE_OR_UPDATE);
      did_move = true;
      whose_turn_state = ActorState::TAKING_TURN;
    }

    mouse_down = false;

    // See Action documentation for why this exists.
    std::vector<std::function<void()>> deferred_events;
    for (auto [id, action] : game.ecs().read_all<ActionPtr>()) {
      if (!action) continue;
      action->run(game, dt, deferred_events);
      if (id == whose_turn && action->finished()) {
        action.reset();
        whose_turn_state = ActorState::SETUP;
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

    for (const auto& [_, transform, render_config] :
         game.ecs().read_all<Transform, GlyphRenderConfig>()) {
      game.glyph_shader().render_glyph(game.to_graphical_pos(transform),
                                       TILE_SIZE, render_config);
    }

    if (!did_move) for (const auto& [pos, node] : dijkstra) {
      if (node.dist > 5) continue;
      game.marker_shader().render_marker(
          game.to_graphical_pos(pos, 0),
          TILE_SIZE, glm::vec4(0.1f, 0.2f, 0.4f, 0.5f));
    }

    game.marker_shader().render_marker(
        game.to_graphical_pos(mouse_grid_pos, 0),
        TILE_SIZE, glm::vec4(0.1f, 0.3f, 0.6f, .5f));

    if (auto [id, exists] = actor_at(game.ecs(), mouse_grid_pos); exists) {
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
