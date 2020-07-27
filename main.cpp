#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>

#include <cstdlib>
#include <iostream>

constexpr unsigned int WINDOW_HEIGHT = 640;
constexpr unsigned int WINDOW_WIDTH = 480;

void panic_sdl_error(const char* const action) {
  std::cerr << "SDL error while " << action << ": " << SDL_GetError();
  std::exit(1);
}

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

int main() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) panic_sdl_error("initializing");

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);  // 4?
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
 
  SDL_Window* win = SDL_CreateWindow(
      "SRPG", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH,
      WINDOW_HEIGHT, SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if (win == nullptr) panic_sdl_error("creating window");

  auto gl_context = SDL_GL_CreateContext(win);
  if (gl_context == nullptr) panic_sdl_error("creating OpenGL context");

  GLenum glew_error = glewInit();
  if (glew_error != GLEW_OK) {
    std::cerr << "error initializing GLEW: " << glewGetErrorString(glew_error)
              << std::endl;
    std::exit(1);
  }

  GLuint program_id = glCreateProgram();

  GLuint vshader = glCreateShader(GL_VERTEX_SHADER);

  const GLchar* vshader_source[] = {
		"#version 140\n"
    "in vec2 vertex_pos;"
    "void main() {"
      "gl_Position = vec4(vertex_pos, 0, 1);"
    "}"
  };

  glShaderSource(vshader, 1, vshader_source, nullptr);
  glCompileShader(vshader);

  GLint vshader_compiled = GL_FALSE;
  glGetShaderiv(vshader, GL_COMPILE_STATUS, &vshader_compiled);
  if (vshader_compiled != GL_TRUE) {
    std::cerr << "Unabled to compile vertext shader " << vshader << ":\n";
    print_shader_log(vshader);
    std::exit(1);
  }

  glAttachShader(program_id, vshader);

  GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);

  const GLchar* frag_shader_source[] =
  {
    "#version 140\n"
    "out vec4 frag_pos;"
    "void main() {"
      "frag_pos = vec4(0.5);"
    "}"
  };

  glShaderSource(frag_shader, 1, frag_shader_source, nullptr);
  glCompileShader(frag_shader);

  GLint frag_shader_compiled = GL_FALSE;
  glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &frag_shader_compiled);
  if (frag_shader_compiled != GL_TRUE) {
    std::cerr << "Unable to compile fragment shader " << frag_shader << ":\n";
    print_shader_log(frag_shader);
    std::exit(1);
  }

  glAttachShader(program_id, frag_shader);
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

    SDL_GL_SwapWindow(win);
  }

  print_program_log(program_id);
  print_shader_log(frag_shader);

  glDeleteProgram(program_id);
  SDL_DestroyWindow(win);
  SDL_Quit();
}
