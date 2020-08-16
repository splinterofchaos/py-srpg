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

#include "glpp.h"
#include "graphics.h"
#include "shaders.h"

constexpr int WINDOW_HEIGHT = 640;
constexpr int WINDOW_WIDTH = 480;

template<typename Vec> Vec just_x(Vec v) { v.y = 0; return v; }
template<typename Vec> Vec just_y(Vec v) { v.x = 0; return v; }
template<typename Vec> Vec flip_x(Vec v) { v.x = -v.x; return v; }
template<typename Vec> Vec flip_y(Vec v) { v.y = -v.y; return v; }

// The vertex type used for sending data to the graphics card.
struct Vertex {
  glm::vec2 pos;
  glm::vec2 tex_coord;
};

GLuint rectangle_vbo(glm::vec3 dimensions = glm::vec3(1.f, 1.f, 0.f),
                     glm::vec2 tex_lower_left = glm::vec2(0.f, 0.f),
                     glm::vec2 tex_size = glm::vec2(1.f, 1.f)) {
  Vertex vertecies[] = {
    {dimensions / 2.f,          tex_lower_left},
    {flip_x(dimensions / 2.f),  tex_lower_left + just_x(tex_size)},
    {-dimensions / 2.f,         tex_lower_left + tex_size},
    {flip_x(-dimensions) / 2.f, tex_lower_left + just_y(tex_size)}
  };
  GLuint vbo = gl::genBuffer();
  gl::bindBuffer(GL_ARRAY_BUFFER, vbo);
  gl::bufferData(GL_ARRAY_BUFFER, vertecies, GL_STATIC_DRAW);

  return vbo;
}


Error run() {
  Graphics gfx;
  if (Error e = gfx.init(WINDOW_WIDTH, WINDOW_HEIGHT); !e.ok) return e;

  GlProgram tex_shader_program;
  if (Error e = simple_texture_shader(tex_shader_program); !e.ok) return e;

  GLint gl_vertex_pos, gl_tex_coord;
  if (Error e =
      tex_shader_program.attribute_location("vertex_pos", gl_vertex_pos) &&
      tex_shader_program.attribute_location("tex_coord", gl_tex_coord);
      !e.ok) return e;

  gl::clearColor(0.f, 0.f, 1.f, 1.f);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  FT_Library freetype;
  if (auto error = FT_Init_FreeType(&freetype)) {
    return Error("Initializing freetype: ", std::to_string(error));
  }

  FT_Face face;
  if (auto error = FT_New_Face(freetype, "font/LeagueMono-Bold.ttf", 0, &face))
  {
    return Error("Loading font: ", std::to_string(error));
  }

  float h_dpi, v_dpi;
  SDL_GetDisplayDPI(0, nullptr, &h_dpi, &v_dpi);
  if (auto error = FT_Set_Char_Size(face, 0, 32*64, h_dpi, v_dpi)) {
    return Error("Setting char size: ", std::to_string(error));
  }
  if (auto error = FT_Set_Pixel_Sizes(face, 0, 32)) {
    return Error("Setting pixel sizes: ", std::to_string(error));
  }

  auto glyph_index = FT_Get_Char_Index(face, 'A');
  if (auto error = FT_Load_Glyph(face, glyph_index, 0)) {
    return Error("Loading char index: ", std::to_string(error));
  }

  if (auto error = FT_Load_Char(face, 'A', FT_LOAD_RENDER)) {
    return Error("Loading glyph: ", std::to_string(error));
  }

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  GLuint tex = gl::genTexture();
  gl::bindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width,
               face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE,
               face->glyph->bitmap.buffer);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  GLuint vbo = rectangle_vbo();

  //IBO data
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

    // This must happen before gl_program.use().
    auto uni = tex_shader_program.uniform_location("tex");
    if (uni == -1) return Error("tex is not a valid uniform location.");
    gl::uniform(uni, tex);
    tex_shader_program.use();

    gl::bindBuffer(GL_ARRAY_BUFFER, vbo);

    gl::enableVertexAttribArray(gl_vertex_pos);
    gl::vertexAttribPointer<float>(gl_vertex_pos, 2, GL_FALSE, &Vertex::pos);

    gl::enableVertexAttribArray(gl_tex_coord);
    gl::vertexAttribPointer<float>(gl_tex_coord, 2, GL_FALSE, &Vertex::tex_coord);

    gl::drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    gl::useProgram(0);

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
