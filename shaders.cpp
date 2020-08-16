#include "shaders.h"

Error simple_texture_shader(GlProgram& tex_shader_program) {
  Shader verts(Shader::Type::VERTEX);
  verts.add_source(R"(
		#version 140
    in vec3 vertex_pos;
    in vec2 tex_coord;
    out vec2 TexCoord;
    void main() {
      gl_Position = vec4(vertex_pos, 1);
      TexCoord = tex_coord;
    }
  )");
  if (Error e = verts.compile(); !e.ok) return e;

  Shader frag(Shader::Type::FRAGMENT);
  frag.add_source(R"(
    #version 140
    in vec2 TexCoord;
    out vec4 FragColor;
    uniform sampler2D tex;
    void main() {
      FragColor = vec4(1, 1, 1, texture(tex, TexCoord).r);
    }
  )");
  if (Error e = frag.compile(); !e.ok) return e;

  tex_shader_program.add_shader(verts);
  tex_shader_program.add_shader(frag);
  return tex_shader_program.link();
}
