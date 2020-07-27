#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>

#include <string>
#include <vector>

#include "util.h"

class Shader {
public:
  enum class Type : GLenum {
    VERTEX = GL_VERTEX_SHADER,
    FRAGMENT = GL_FRAGMENT_SHADER
  };

private:
  std::vector<std::string> sources_;
  GLuint id_;
  Type type_;

public:
  Shader(Type type);
  Shader(const Shader&) = delete;

  void add_source(std::string src);
  Error compile();

  std::string log() const;

  GLuint id() const { return id_; }
};

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
