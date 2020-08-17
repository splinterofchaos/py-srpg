#include "shaders.h"

Error text_shader(GlProgram& tex_shader_program) {
  Shader verts(Shader::Type::VERTEX);
  verts.add_source(R"(
		#version 140
    in vec3 vertex_pos;
    in vec2 tex_coord;
    uniform mat4 transform;
    out vec2 TexCoord;
    void main() {
      gl_Position = transform * vec4(vertex_pos, 1);
      TexCoord = tex_coord;
    }
  )");
  if (Error e = verts.compile(); !e.ok) return e;

  Shader frag(Shader::Type::FRAGMENT);
  frag.add_source(R"(
    #version 140
    in vec2 TexCoord;
    in vec4 Bg_color;
    in vec4 Fg_color;
    out vec4 FragColor;
    uniform sampler2D tex;
    uniform vec4 bg_color;
    uniform vec4 fg_color;
    void main() {
      FragColor = mix(bg_color, fg_color, texture(tex, TexCoord).r);
    }
  )");
  if (Error e = frag.compile(); !e.ok) return e;

  tex_shader_program.add_shader(verts);
  tex_shader_program.add_shader(frag);
  return tex_shader_program.link();
}
