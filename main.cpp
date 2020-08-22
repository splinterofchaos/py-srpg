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
#include "glpp.h"
#include "grid.h"
#include "graphics.h"
#include "math.h"
#include "shaders.h"
#include "timer.h"

constexpr int WINDOW_HEIGHT = 640;
constexpr int WINDOW_WIDTH = 480;

constexpr float TILE_SIZE = 0.1f;

glm::vec3 to_graphical_pos(glm::vec2 pos, int z, glm::vec2 camera_offset) {
  return glm::vec3(pos.x * TILE_SIZE, pos.y * TILE_SIZE, z) -
         glm::vec3(camera_offset, 0.f);
}

glm::vec3 to_graphical_pos(const Transform& transform, glm::vec2 camera_offset) {
  return to_graphical_pos(transform.pos, transform.z, camera_offset);
}

using Time = std::chrono::high_resolution_clock::time_point;
using Milliseconds = std::chrono::milliseconds;

Time now() {
  return std::chrono::high_resolution_clock::now();
}

class Game {
  Ecs ecs_;
  Grid grid_;

  MarkerShaderProgram marker_shader_;
  GlyphShaderProgram glyph_shader_;
  FontMap font_map_;
  glm::vec2 camera_offset_ = glm::vec2(0.95f, 0.95f);

public:
  Error init() {
    return glyph_shader_.init() &&
           marker_shader_.init() &&
           font_map_.init("font/LeagueMono-Bold.ttf");
  }

  const Grid& grid() const { return grid_; }

  void set_grid(Grid grid) {
    for (auto [pos, tile] : grid) {
      GlyphRenderConfig rc(font_map_.get(tile.glyph), tile.fg_color,
                           tile.bg_color);
      rc.center();
      ecs().write_new_entity(Transform{pos, 0}, rc);
    }
    grid_ = std::move(grid);
  }

  Ecs& ecs() { return ecs_; }
  const Ecs& ecs() const { return ecs_; }

  const MarkerShaderProgram& marker_shader() const { return marker_shader_; }
  const GlyphShaderProgram& glyph_shader() const { return glyph_shader_; }

  FontMap& font_map() { return font_map_; }
  const FontMap& font_map() const { return font_map_; }

  const glm::vec2& camera_offset() const { return camera_offset_; }

  glm::vec3 to_graphical_pos(glm::vec2 pos, int z) const {
    return glm::vec3(pos.x * TILE_SIZE, pos.y * TILE_SIZE, z) -
           glm::vec3(camera_offset_, 0.f);
  }

  glm::vec3 to_graphical_pos(const Transform& transform) const {
    return to_graphical_pos(transform.pos, transform.z);
  }
};


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

std::vector<glm::ivec2> adjacent_steps() {
  return {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
}

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
    lines.push_back(concat_strings("HP: ", std::to_string(actor->stats.max_hp),
                                   "/", std::to_string(actor->stats.hp)));
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
    Stats stats{.hp = 10, .max_hp = 10, .strength = 5};
    player = game.ecs().write_new_entity(Transform{{5.f, 5.f}, -1},
                                         GridPos{{5, 5}},
                                         rc, Actor{"player", stats});
  }

  EntityId spider;
  {
    GlyphRenderConfig rc(game.font_map().get('s'),
                         glm::vec4(0.f, 0.2f, 0.6f, 1.f));
    rc.center();
    Stats stats{.hp = 10, .max_hp = 10, .strength = 5};
    spider = game.ecs().write_new_entity(Transform{{4.f, 4.f}, -1},
                                         GridPos{{4, 4}},
                                         rc, Actor{"spider", stats});
  }

  std::unique_ptr<Action> current_action;
  std::vector<Path> walkable_positions;
  std::vector<PossibleAttack> attackable_positions;

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

    if (current_action && current_action->finished()) current_action.reset();

    if (!current_action && walkable_positions.empty()) {
      const GridPos& player_pos = game.ecs().read_or_panic<GridPos>(player);
      walkable_positions = expand_walkable(game, player_pos.pos, 5);
      attackable_positions = expand_attacks(game.ecs(), player_pos.pos,
                                            walkable_positions);
    }

    if (mouse_down && !current_action) {
      auto it = std::find_if(attackable_positions.begin(),
                             attackable_positions.end(),
                             [mouse_grid_pos](const PossibleAttack& atk) {
                                 return atk.defender_pos == mouse_grid_pos;
                             });
      if (it != attackable_positions.end()) {
        current_action = mele_action(game.ecs(), player, it->defender, it->path);
      }
    }

    if (mouse_down && !current_action) {
      auto it = std::find_if(walkable_positions.begin(),
                             walkable_positions.end(),
                             [mouse_grid_pos](const Path& p) {
                                 return glm::ivec2(p.back()) == mouse_grid_pos;
                             });
      if (it != walkable_positions.end()) {
        current_action = move_action(player, std::move(*it));
      }
    }

    if (current_action) {
      walkable_positions.clear();
      attackable_positions.clear();
    }
    mouse_down = false;

    if (current_action) {
      current_action->run(game.ecs(), dt);
    }

    gl::clear();

    for (const auto& [_, transform, render_config] :
         game.ecs().read_all<Transform, GlyphRenderConfig>()) {
      game.glyph_shader().render_glyph(game.to_graphical_pos(transform),
                                       TILE_SIZE, render_config);
    }

    for (const Path& path : walkable_positions)
      game.marker_shader().render_marker(
          game.to_graphical_pos(path.back(), 0),
          TILE_SIZE, glm::vec4(0.1f, 0.2f, 0.4f, 0.5f));

    game.marker_shader().render_marker(
        game.to_graphical_pos(mouse_grid_pos, 0),
        TILE_SIZE, glm::vec4(0.1f, 0.3f, 0.6f, .5f));

    if (auto [id, found] = actor_at(game.ecs(), mouse_grid_pos); found) {
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
