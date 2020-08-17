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

#include "font.h"
#include "glpp.h"
#include "graphics.h"
#include "shaders.h"
#include "math.h"

constexpr int WINDOW_HEIGHT = 640;
constexpr int WINDOW_WIDTH = 480;

constexpr float TILE_SIZE = 0.1f;

template<typename Vec> Vec just_x(Vec v) { v.y = 0; return v; }
template<typename Vec> Vec just_y(Vec v) { v.x = 0; return v; }
template<typename Vec> Vec flip_x(Vec v) { v.x = -v.x; return v; }
template<typename Vec> Vec flip_y(Vec v) { v.y = -v.y; return v; }

// The vertex type used for sending data to the graphics card.
struct Vertex {
  glm::vec2 pos;
  glm::vec2 tex_coord;
};

GLuint rectangle_vbo(glm::vec3 dimensions = glm::vec3(1.f, 1.f, 0),
                     glm::vec2 tex_lower_left = glm::vec2(0.f, 0.f),
                     glm::vec2 tex_size = glm::vec2(1.f, 1.f)) {
  Vertex vertecies[] = {
    {dimensions / 2.f,          tex_lower_left + just_x(tex_size)},
    {flip_x(dimensions / 2.f),  tex_lower_left},
    {-dimensions / 2.f,         tex_lower_left + just_y(tex_size)},
    {flip_x(-dimensions) / 2.f, tex_lower_left + tex_size}
  };
  GLuint vbo = gl::genBuffer();
  gl::bindBuffer(GL_ARRAY_BUFFER, vbo);
  gl::bufferData(GL_ARRAY_BUFFER, vertecies, GL_STATIC_DRAW);

  return vbo;
}

struct GlProgramBindings {
  const GlProgram& program;

  GLint vertex_pos_attr = -1;
  GLint tex_coord_attr = -1;
  GLint transform_uniform = -1;
  GLint texture_uniform = -1;
  GLint bg_color_uniform = -1;
  GLint fg_color_uniform = -1;
  GLint bottom_left_uniform = -1;
  GLint top_right_uniform = -1;

  Error init_vertex_pos_attr() {
    return program.attribute_location("vertex_pos", vertex_pos_attr);
  }

  Error init_tex_coord_attr() {
    return program.attribute_location("tex_coord", tex_coord_attr);
  }

  Error init_texture_uniform() {
    return program.uniform_location("tex", texture_uniform);
  }

  Error init_transform_uniform() {
    return program.uniform_location("transform", transform_uniform);
  }

  Error init_fg_color_uniform() {
    return program.uniform_location("fg_color", fg_color_uniform);
  }

  Error init_bg_color_uniform() {
    return program.uniform_location("bg_color", bg_color_uniform);
  }

  Error init_bottom_left_uniform() {
    return program.uniform_location("top_left", bottom_left_uniform);
  }

  Error init_top_right_uniform() {
    return program.uniform_location("bottom_right", top_right_uniform);
  }
};

struct RenderConfig {
  GLuint vbo = 0;
  GLuint texture = 0;
  glm::vec2 top_left, bottom_right;
  glm::vec4 fg_color, bg_color;
};

void render_object(glm::vec3 pos, const GlProgramBindings& program_bindings,
                   const RenderConfig& render_config) {
  program_bindings.program.use();

  glBindTexture(GL_TEXTURE_2D, render_config.texture);

  gl::bindBuffer(GL_ARRAY_BUFFER, render_config.vbo);
  gl::uniform(program_bindings.texture_uniform, render_config.texture);
  gl::uniform4v(program_bindings.fg_color_uniform, 1,
              glm::value_ptr(render_config.fg_color));
  gl::uniform4v(program_bindings.bg_color_uniform, 1,
              glm::value_ptr(render_config.bg_color));
  gl::uniform2v(program_bindings.bottom_left_uniform, 1,
              glm::value_ptr(render_config.top_left));
  gl::uniform2v(program_bindings.top_right_uniform, 1,
              glm::value_ptr(render_config.bottom_right));

  gl::enableVertexAttribArray(program_bindings.vertex_pos_attr);
  gl::vertexAttribPointer<float>(program_bindings.vertex_pos_attr, 2, GL_FALSE,
                                 &Vertex::pos);

  gl::enableVertexAttribArray(program_bindings.tex_coord_attr);
  gl::vertexAttribPointer<float>(program_bindings.tex_coord_attr, 2, GL_FALSE,
                                 &Vertex::tex_coord);

  glUniformMatrix4fv(program_bindings.transform_uniform, 1, GL_FALSE,
                     glm::value_ptr(transformation(pos, 0, TILE_SIZE)));

  gl::drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

Error run() {
  Graphics gfx;
  if (Error e = gfx.init(WINDOW_WIDTH, WINDOW_HEIGHT); !e.ok) return e;

  GlProgram tex_shader_program;
  if (Error e = text_shader(tex_shader_program); !e.ok) return e;

  GlProgramBindings program_bindings{.program = tex_shader_program};
  if (Error e =
      program_bindings.init_vertex_pos_attr() &&
      program_bindings.init_tex_coord_attr() &&
      program_bindings.init_texture_uniform() &&
      program_bindings.init_transform_uniform() &&
      program_bindings.init_fg_color_uniform() &&
      program_bindings.init_bg_color_uniform() &&
      program_bindings.init_bottom_left_uniform() &&
      program_bindings.init_top_right_uniform();
      !e.ok) return e;

  gl::clearColor(0.f, 0.f, 0.f, 1.f);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  RenderConfig render_config;
  render_config.vbo = rectangle_vbo();
  render_config.fg_color = glm::vec4(.5f, .5f, .5f, 1.f);
  render_config.bg_color = glm::vec4(.2f, .2f, .2f, 1.f);

  FontMap font_map;
  if (Error e = font_map.init("font/LeagueMono-Bold.ttf"); !e.ok) return e;

  GLuint vbo_elems[] = {0, 1, 2,
                        2, 3, 0};
  GLuint vbo_elems_id = gl::genBuffer();
  gl::bindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_elems_id);
  gl::bufferData(GL_ELEMENT_ARRAY_BUFFER, vbo_elems, GL_STATIC_DRAW);
  bool keep_going = true;
  SDL_Event e;
  while (keep_going) {
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) keep_going = false;
    }

    gl::clear();

    for (int x = 0; x < 20; ++x)
      for (int y = 0; y < 20; ++y) {
        Glyph* glyph;
        if (Error e = font_map.get_safe('a' + x, &glyph); !e.ok) return e;

        render_config.texture = glyph->texture;
        render_config.top_left = glyph->top_left;
        render_config.bottom_right = glyph->bottom_right;

        render_object(
            glm::vec3(-1.f + TILE_SIZE/2 + x * TILE_SIZE,
                      -1.f + TILE_SIZE/2 + y * TILE_SIZE, 0),
            program_bindings,
            render_config);
      }

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
