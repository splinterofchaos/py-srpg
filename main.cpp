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

glm::vec3 to_graphical_pos(glm::vec2 pos, int z, glm::vec3 camera_offset) {
  return glm::vec3(pos.x * TILE_SIZE, pos.y * TILE_SIZE, z) - camera_offset;
}

glm::vec3 to_graphical_pos(const Transform& transform, glm::vec3 camera_offset) {
  return to_graphical_pos(transform.pos, transform.z, camera_offset);
}

using Time = std::chrono::high_resolution_clock::time_point;
using Milliseconds = std::chrono::milliseconds;

Time now() {
  return std::chrono::high_resolution_clock::now();
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

std::vector<glm::ivec2> adjacent_steps() {
  return {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
}

std::vector<Path> expand_walkable(Grid grid, const Ecs& ecs,
                                  glm::ivec2 start, unsigned int max_dist) {
  std::unordered_set<glm::ivec2> taken_spaces;
  for (const auto [unused_id, pos, unused_actor] :
       ecs.read_all<GridPos, Actor>())
    taken_spaces.insert(pos.pos);
  
  std::deque<Path> edges = {{start}};
  std::unordered_set<glm::ivec2> visited;
  std::vector<Path> paths;
  while (!edges.empty()) {
    Path path = std::move(edges.front());
    edges.pop_front();

    if (!visited.insert(path.back()).second) continue;
    if (path.back() != start) paths.push_back(path);

    if (path.size() + 1 >= max_dist) continue;
    for (glm::vec step : adjacent_steps()) {
      glm::ivec2 next_pos = step + path.back();
      auto [tile, in_grid] = grid.get(next_pos);
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

// Print's an entity's description next to its location. The view of the entity
// itself shall be unobstructed and the text will always be on screen.
void render_entity_desc(const Ecs& ecs,
                        const MarkerShaderProgram& marker_shader,
                        const GlyphShaderProgram& glyph_shader,
                        FontMap& font_map,
                        glm::vec3 camera_offset,
                        EntityId id) {
  GridPos grid_pos = ecs.read_or_panic<GridPos>(id);

  const std::string* name;
  if (ecs.read(id, &name) == EcsError::NOT_FOUND) {
    static std::string not_found_name = "UNKNOWN";
    name = &not_found_name;
  }

  float w = 0.f;
  for (unsigned int i = 0; i < name->size(); ++i) {
    const Glyph& glyph = font_map.get(name->at(i));
    w += glyph.bottom_right.x;
    // Leave the same amount of trailing space as there is leading.
    if (i == 0) w += glyph.top_left.x;
  }
  w += name->size() * TILE_SIZE * 0.1f;

  glm::vec2 start = glm::vec2(grid_pos.pos) + glm::vec2(1.f, 0.f);
  glm::vec2 end = start + glm::vec2(w, 0.f);
  glm::vec2 center = (start + end) / 2.f;
  glm::vec3 graphical_center = to_graphical_pos(center, 0, camera_offset);

  // Check that it's not overflowing offscreen. Maybe move it to the other
  // side of the entity whose information we're printing.
  glm::vec3 graphical_end = to_graphical_pos(end, 0, camera_offset);
  if (graphical_end.x > 1.f) {
    glm::vec2 offset = 2.f * (glm::vec2(grid_pos.pos) - center); 
    graphical_center = to_graphical_pos(offset + center, 0, camera_offset);
    start += offset;

  }

  marker_shader.render_marker(
      graphical_center,
      TILE_SIZE, glm::vec4(0.f, 0.f, 0.f, .9f), glm::vec2(w, 1.f));

  float cursor = start.x + 0.5f;
  for (char c : *name) {
    glm::vec2 pos = glm::vec2(cursor, start.y);
    const Glyph& glyph = font_map.get(c);
    GlyphRenderConfig rc(glyph, glm::vec4(1.f));
    glyph_shader.render_glyph(to_graphical_pos(pos, 0.f, camera_offset),
                              TILE_SIZE, rc);

    cursor += glyph.bottom_right.x + TILE_SIZE * 0.1f;
  }
}

Error run() {
  Graphics gfx;
  if (Error e = gfx.init(WINDOW_WIDTH, WINDOW_HEIGHT); !e.ok) return e;

  GlyphShaderProgram glyph_shader;
  if (Error e = glyph_shader.init(); !e.ok) return e;;

  MarkerShaderProgram marker_shader;
  if (Error e = marker_shader.init(); !e.ok) return e;

  gl::clearColor(0.f, 0.f, 0.f, 1.f);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  glOrtho(-1, 1, -1, 1, -10, 10);

  FontMap font_map;
  if (Error e = font_map.init("font/LeagueMono-Bold.ttf"); !e.ok) return e;

  glm::vec3 camera_offset(0.95f, 0.95f, 0.f);

  GLuint vbo_elems[] = {0, 1, 2,
                        2, 3, 0};
  GLuint vbo_elems_id = gl::genBuffer();
  gl::bindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_elems_id);
  gl::bufferData(GL_ELEMENT_ARRAY_BUFFER, vbo_elems, GL_STATIC_DRAW);

  Ecs ecs;

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
  Grid grid = grid_from_string(STARTING_GRID, tile_types);
  for (auto [pos, tile] : grid) {
    GlyphRenderConfig rc(font_map.get(tile.glyph), tile.fg_color,
                         tile.bg_color);
    rc.center();
    ecs.write_new_entity(Transform{pos, 0}, rc);
  }

  EntityId player;
  {
    GlyphRenderConfig rc(font_map.get('@'), glm::vec4(.9f, .6f, .1f, 1.f));
    rc.center();
    player = ecs.write_new_entity(std::string("you"),
                                  Transform{{5.f, 5.f}, -1},
                                  GridPos{{5, 5}},
                                  rc, Actor{});
  }

  EntityId spider;
  {
    GlyphRenderConfig rc(font_map.get('s'), glm::vec4(0.f, 0.2f, 0.6f, 1.f));
    rc.center();
    spider = ecs.write_new_entity(std::string("spider"),
                                  Transform{{4.f, 4.f}, -1},
                                  GridPos{{4, 4}},
                                  rc, Actor{});
  }

  std::unique_ptr<Action> current_action;
  std::vector<Path> walkable_positions;

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
    glm::vec3 mouse_screen_pos(float(mouse_x * 2) / WINDOW_WIDTH - 1,
                               float(-mouse_y * 2) / WINDOW_HEIGHT + 1,
                               0.f);
    glm::ivec3 mouse_grid_pos =
      glm::ivec3((mouse_screen_pos + TILE_SIZE/2) / TILE_SIZE +
                 camera_offset / TILE_SIZE);

    if (current_action && current_action->finished()) current_action.reset();

    if (!current_action && walkable_positions.empty()) {
      const GridPos& player_pos = ecs.read_or_panic<GridPos>(player);
      walkable_positions = expand_walkable(grid, ecs, player_pos.pos, 5);
    }

    if (mouse_down && !current_action) {
      auto it = std::find_if(walkable_positions.begin(),
                             walkable_positions.end(),
                             [mouse_grid_pos](const Path& p) {
                                 return p.back() == glm::ivec2(mouse_grid_pos);
                             });
      if (it != walkable_positions.end()) {
        current_action = move_action(player, std::move(*it));
        walkable_positions.clear();
      }
    }
    mouse_down = false;

    if (current_action) {
      current_action->run(ecs, dt);
    }

    gl::clear();

    for (const auto& [_, transform, render_config] :
         ecs.read_all<Transform, GlyphRenderConfig>()) {
      glyph_shader.render_glyph(to_graphical_pos(transform, camera_offset),
                                TILE_SIZE, render_config);
    }

    for (const Path& path : walkable_positions)
      marker_shader.render_marker(
          to_graphical_pos(path.back(), 0, camera_offset),
          TILE_SIZE, glm::vec4(0.1f, 0.2f, 0.4f, 0.5f));

    marker_shader.render_marker(
        to_graphical_pos(mouse_grid_pos, 0, camera_offset),
        TILE_SIZE, glm::vec4(0.1f, 0.3f, 0.6f, .5f));

    if (auto [id, found] = actor_at(ecs, mouse_grid_pos); found) {
      render_entity_desc(ecs, marker_shader, glyph_shader, font_map,
                         camera_offset, id);
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
