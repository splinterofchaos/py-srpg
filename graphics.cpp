#include <sstream>

#include "graphics.h"

Error sdl_error(const char* const action) {
  std::ostringstream oss;
  oss << "SDL error while " << action << ": " << SDL_GetError();
  return Error(oss.str());
}

Error Graphics::init(int width, int height) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) return sdl_error("initializing");

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);  // 4?
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
 
  width_ = width;
  height_ = height;
  win_ = SDL_CreateWindow(
      "SRPG", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width_,
      height_, SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if (win_ == nullptr) return sdl_error("creating window");

  gl_context_ = SDL_GL_CreateContext(win_);
  if (gl_context_ == nullptr) sdl_error("creating OpenGL context");
  
  if (GLenum glew_error = glewInit(); glew_error != GLEW_OK) {
    return Error(concat_strings("error initializing GLEW: ",
                                (char*)glewGetErrorString(glew_error)));
  }

  return Error();
}

void Graphics::swap_buffers() {
  SDL_GL_SwapWindow(win_);
}

Graphics::~Graphics() {
  SDL_GL_DeleteContext(gl_context_);
  SDL_DestroyWindow(win_);
  SDL_Quit();
}
