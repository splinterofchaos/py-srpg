#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>

#include "util.h"

// This is a little POD for organizing SDL and OpenGL code.
class Graphics {
  SDL_Window* win_ = nullptr;
  int width_;
  int height_;

  SDL_GLContext gl_context_ = nullptr;

public:
  Graphics() { }

  Error init(int width, int height);

  void swap_buffers();

  ~Graphics();
};
