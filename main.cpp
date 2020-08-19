#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_ttf.h>
#include <GL/glu.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "action.h"
#include "ecs.h"
#include "components.h"
#include "font.h"
#include "glpp.h"
#include "graphics.h"
#include "math.h"
#include "shaders.h"
#include "timer.h"

constexpr int WINDOW_HEIGHT = 640;
constexpr int WINDOW_WIDTH = 480;

constexpr float TILE_SIZE = 0.1f;

glm::vec3 to_graphical_pos(glm::vec2 pos, int z) {
  return glm::vec3(pos.x * TILE_SIZE, pos.y * TILE_SIZE, z);
}

glm::vec3 to_graphical_pos(const Transform& transform) {
  return to_graphical_pos(transform.pos, transform.z);
}

using Time = std::chrono::high_resolution_clock::time_point;
using Milliseconds = std::chrono::milliseconds;

Time now() {
  return std::chrono::high_resolution_clock::now();
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

  // Create the tiles. Note that this MUST happen first so they are drawn
  // first. Eventually, we might want to consider z-sorting.
  for (int x = 0; x < 20; ++x) for (int y = 0; y < 20; ++y) {
    GlyphRenderConfig rc(font_map.get('a' + x),
                         glm::vec4(.5f, .5f, .5f, 1.f),
                         glm::vec4(.2f, .2f, .2f, 1.f));
    ecs.write_new_entity(Transform{{x, y}, 0}, rc);
  }

  EntityId player;
  {
    GlyphRenderConfig rc(font_map.get('@'), glm::vec4(.9f, .6f, .1f, 1.f));
    player = ecs.write_new_entity(Transform{{5.f, 5.f}, -1},
                                  GridPos{{5, 5}},
                                  rc);
  }

  std::unique_ptr<Action> current_action;

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

    if (mouse_down && !current_action) {
      current_action = move_action(player, mouse_grid_pos);
    }
    mouse_down = false;

    if (current_action) {
      current_action->run(ecs, dt);
    }

    gl::clear();

    for (const auto& [_, transform, render_config] :
         ecs.read_all<Transform, GlyphRenderConfig>()) {
      glyph_shader.render_glyph(to_graphical_pos(transform) - camera_offset,
                                TILE_SIZE, render_config);
    }

    marker_shader.render_marker(
        to_graphical_pos(mouse_grid_pos, 0) - camera_offset,
        TILE_SIZE, glm::vec4(0.1f, 0.3f, 0.6f, .5f));

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
