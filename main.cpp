#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>

#include <cstdlib>
#include <iostream>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "glpp.h"
#include "graphics.h"
#include "shaders.h"

constexpr int WINDOW_HEIGHT = 640;
constexpr int WINDOW_WIDTH = 480;

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

  //Initialize clear color
  gl::clearColor(0.f, 0.f, 0.f, 1.f);

  GLuint tex;
  if (Error e = load_bmp_texture("art/goblin.bmp", tex); !e.ok) return e;

  struct Vertex {
    glm::vec2 pos;
    glm::vec2 tex_coord;
  };

  //VBO data
  Vertex vertecies[] = {
    // Position    TexCoords
    {{-0.5f, -0.5f},  {1.0f, 1.0f}},
    {{ 0.5f, -0.5f},  {0.0f, 1.0f}},
    {{ 0.5f,  0.5f},  {0.0f, 0.0f}},
    {{-0.5f,  0.5f},  {1.0f, 0.0f}}
  };

  GLuint vbo = gl::genBuffer();
  gl::bindBuffer(GL_ARRAY_BUFFER, vbo);
  gl::bufferData(GL_ARRAY_BUFFER, vertecies, GL_STATIC_DRAW);

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
    std::cerr << e.reason << std::endl;
    return 1;
  }
  return 0;
}
