#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_ttf.h>
#include <GL/glu.h>

#include <cstdlib>
#include <iostream>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "ecs.h"
#include "font.h"
#include "glpp.h"
#include "graphics.h"
#include "shaders.h"
#include "math.h"

constexpr int WINDOW_HEIGHT = 640;
constexpr int WINDOW_WIDTH = 480;

constexpr float TILE_SIZE = 0.1f;

struct Transform {
  glm::ivec2 pos;
  int z;
};

glm::vec3 to_graphical_pos(glm::ivec2 pos, int z) {
  return glm::vec3(pos.x * TILE_SIZE, pos.y * TILE_SIZE, z);
}

glm::vec3 to_graphical_pos(const Transform& transform) {
  return to_graphical_pos(transform.pos, transform.z);
}

glm::ivec3 to_grid_pos(const glm::vec3& p) {
  return {(int)std::ceil((p.x + TILE_SIZE/2) / TILE_SIZE),
          (int)std::ceil(p.y / TILE_SIZE),
          0};
}

using Ecs = EntityComponentSystem<Transform, GlyphRenderConfig>;

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

  {  // create player
    GlyphRenderConfig rc(font_map.get('@'), glm::vec4(.9f, .6f, .1f, 1.f));
    ecs.write_new_entity(Transform{{5, 5}, -1}, rc);
  }

  bool keep_going = true;
  SDL_Event e;
  while (keep_going) {
    while (SDL_PollEvent(&e) != 0) {
      switch (e.type) {
        case SDL_QUIT: keep_going = false;
        case SDL_KEYDOWN: {
          switch (e.key.keysym.sym) {
            case 'q': keep_going = false;
          }
        }
      }
    }

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
