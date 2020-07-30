#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>

#include <cstdlib>
#include <iostream>

#include "graphics.h"
#include "vec.h"


constexpr int WINDOW_HEIGHT = 640;
constexpr int WINDOW_WIDTH = 480;

Error run() {
  Graphics gfx;
  if (Error e = gfx.init(WINDOW_WIDTH, WINDOW_HEIGHT); !e.ok) return e;


  Shader verts(Shader::Type::VERTEX);
  verts.add_source(
		"#version 140\n"
    "in vec2 vertex_pos;"
    "in vec2 tex_coord;"
    "out vec2 TexCoord;"
    "void main() {"
      "gl_Position = vec4(vertex_pos, 0, 1);"
      "TexCoord = tex_coord;"
    "}"
  );

  if (Error e = verts.compile(); !e.ok) return e;

  Shader frag(Shader::Type::FRAGMENT);
  frag.add_source(
    "#version 140\n"
    "in vec2 TexCoord;"
    "in vec2 tex_coord;"
    "out vec4 FragColor;"
    "uniform sampler2D tex;"
    "void main() {"
      "FragColor = texture(tex, TexCoord);"
    "}"
  );

  if (Error e = frag.compile(); !e.ok) return e;

  GlProgram gl_program;
  gl_program.add_shader(verts);
  gl_program.add_shader(frag);
  if (Error e = gl_program.link(); !e.ok) return e;

  auto gl_vertext_pos = gl_program.attribute_location("vertex_pos");
  if (gl_vertext_pos == -1) return Error("vertex_pos is not a valid var.");
  auto gl_tex_coord = gl_program.attribute_location("tex_coord");
  if (gl_tex_coord == -1) return Error("tex_coord is not a valid var.");

  //Initialize clear color
  glClearColor(0.f, 0.f, 0.f, 1.f);

  SDL_Surface* floor_surface = SDL_LoadBMP("art/goblin.bmp");
  if (floor_surface == nullptr) {
    return Error(concat_strings("Failed to load art/floor.bmp: ",
                                SDL_GetError()));
  }

  GLuint floor_texture;
  glGenTextures(1, &floor_texture);
  glBindTexture(GL_TEXTURE_2D, floor_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, floor_surface->w,
               floor_surface->h, 0, GL_RGB, GL_UNSIGNED_BYTE,
               floor_surface->pixels);
  if (auto e = glGetError(); e != GL_NO_ERROR) {
    return Error(concat_strings(
        "I'm too lazy to figure out if there's a function which maps this GL "
        "error number to a string so here's a number: ", std::to_string(e)));
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  GLfloat bad_color[] = {.8f, 0.5f, 0.8f, 1.f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bad_color);

  SDL_FreeSurface(floor_surface);

  //GLuint vao;
  //glGenVertexArrays(1, &vao);
  //glBindVertexArray(vao);

  struct Vertex {
    Vec<GLfloat, 2> pos;
    Vec<GLfloat, 2> tex_coord;
  };

  //VBO data
  Vertex vertecies[] = {
    // Position    TexCoords
    {{-0.5f, -0.5f},  {1.0f, 1.0f}},
    {{ 0.5f, -0.5f},  {0.0f, 1.0f}},
    {{ 0.5f,  0.5f},  {0.0f, 0.0f}},
    {{-0.5f,  0.5f},  {1.0f, 0.0f}}
  };

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertecies), vertecies, GL_STATIC_DRAW);

  //IBO data
  GLuint vbo_elems[] = {0, 1, 2,
                        2, 3, 0};

  GLuint vbo_elems_id;
  glGenBuffers(1, &vbo_elems_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_elems_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(vbo_elems), vbo_elems, GL_STATIC_DRAW);

  bool keep_going = true;
  SDL_Event e;
  while (keep_going) {
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) keep_going = false;
    }

    glClear(GL_COLOR_BUFFER_BIT);

    // This must happen before gl_program.use().
    auto uni = gl_program.uniform_location("tex");
    if (uni == -1) return Error("tex is not a valid uniform location.");
    glUniform1i(uni, floor_texture);

    gl_program.use();


    glEnableVertexAttribArray(gl_vertext_pos);
    glVertexAttribPointer(gl_vertext_pos, 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), (void*)offsetof(Vertex, pos));

    glEnableVertexAttribArray(gl_tex_coord);
    glVertexAttribPointer(gl_tex_coord, 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), (void*)offsetof(Vertex, tex_coord));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glUseProgram(0);

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
