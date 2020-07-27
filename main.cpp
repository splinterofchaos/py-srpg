#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>

#include <cstdlib>
#include <iostream>

#include "graphics.h"

constexpr int WINDOW_HEIGHT = 640;
constexpr int WINDOW_WIDTH = 480;

void print_shader_log(GLuint shader) {
	//Make sure name is shader
	if(!glIsShader(shader)) {
    std::cerr << "PrintShaderLog: No shader with id " << shader << std::endl;
    return;
  }
  //Shader log length
  int info_log_len = 0;
  int max_len = info_log_len;
  
  //Get info string length
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_len);
  
  //Allocate string
  char* infoLog = new char[max_len];
  
  //Get info log
  glGetShaderInfoLog(shader, max_len, &info_log_len, infoLog);
  if(info_log_len) std::cout << infoLog << std::endl;

  //Deallocate string
  delete[] infoLog;
}

void print_program_log(GLuint program) {
	//Make sure name is program
	if(!glIsProgram(program)) {
    std::cerr << "print_program_log: No program with id " << program << std::endl;
    return;
  }
  //Shader log length
  int info_log_len = 0;
  int max_len = info_log_len;
  
  //Get info string length
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_len);
  
  char* infoLog = new char[max_len];
  glGetProgramInfoLog(program, max_len, &info_log_len, infoLog);
  if(info_log_len) std::cout << infoLog << std::endl;

  //Deallocate string
  delete[] infoLog;
}
  

Error run() {
  Graphics gfx;
  if (Error e = gfx.init(WINDOW_WIDTH, WINDOW_HEIGHT); !e.ok) return e;

  GLuint program_id = glCreateProgram();

  Shader verts(Shader::Type::VERTEX);
  verts.add_source(
		"#version 140\n"
    "in vec2 vertex_pos;"
    "void main() {"
      "gl_Position = vec4(vertex_pos, 0, 1);"
    "}"
  );

  if (Error e = verts.compile(); !e.ok) return e;

  glAttachShader(program_id, verts.id());

  Shader frag(Shader::Type::FRAGMENT);
  frag.add_source(
    "#version 140\n"
    "out vec4 frag_pos;"
    "void main() {"
      "frag_pos = vec4(0.5);"
    "}"
  );

  if (Error e = frag.compile(); !e.ok) return e;

  glAttachShader(program_id, frag.id());
  glLinkProgram(program_id);

  GLint program_linked;
  glGetProgramiv(program_id, GL_LINK_STATUS, &program_linked);
  if (program_linked != GL_TRUE) {
    std::cerr << "Error linking GL program " << program_id << ":\n";
    print_program_log(program_id);
    std::exit(1);
  }

  auto gl_vertext_pos = glGetAttribLocation(program_id, "vertex_pos");
  if (gl_vertext_pos == -1) {
    std::cerr << "vertex_pos is not a valid glsl program var." << std::endl;
    std::exit(1);
  }

  //Initialize clear color
  glClearColor(0.f, 0.f, 0.f, 1.f);

  //VBO data
  GLfloat vertex_data[] = {
    -0.5f, -0.5f,
    0.5f, -0.5f,
    0.5f,  0.5f,
    -0.5f,  0.5f
  };

  //IBO data
  GLuint index_data[] = {0, 1, 2, 3};
 
 
  GLuint vertext_buf_obj;
  glGenBuffers(1, &vertext_buf_obj);
  glBindBuffer(GL_ARRAY_BUFFER, vertext_buf_obj);
  glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(GLfloat), vertex_data, GL_STATIC_DRAW);

  //Create IBO
  GLuint index_buf_obj;
  glGenBuffers(1, &index_buf_obj);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf_obj);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4 * sizeof(GLuint), index_data, GL_STATIC_DRAW);

  bool keep_going = true;
  SDL_Event e;
  while (keep_going) {
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) keep_going = false;
    }

    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program_id);

    glEnableVertexAttribArray(gl_vertext_pos);
    glBindBuffer(GL_ARRAY_BUFFER, vertext_buf_obj);
    glVertexAttribPointer(gl_vertext_pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf_obj);
    glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, nullptr);

    glDisableVertexAttribArray(gl_vertext_pos);

    glUseProgram(0);

    gfx.swap_buffers();
  }

  glDeleteProgram(program_id);

  return Error();
}

int main() {
  if (Error e = run(); !e.ok) {
    std::cerr << e.reason << std::endl;
    return 1;
  }
  return 0;
}
