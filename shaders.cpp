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
    out vec4 FragColor;
    uniform sampler2D tex;
    uniform vec4 bg_color;
    uniform vec4 fg_color;
    uniform vec2 top_left;
    uniform vec2 bottom_right;
    in vec2 TexCoord;
    void main() {
      float a = 0;
      if (all(greaterThan(TexCoord, top_left)) && 
          all(lessThan(TexCoord, bottom_right))) {
        a = texture(tex, smoothstep(top_left, bottom_right, TexCoord)).r;
      }
      FragColor = mix(bg_color, fg_color, a);
    }
  )");
  if (Error e = frag.compile(); !e.ok) return e;

  tex_shader_program.add_shader(verts);
  tex_shader_program.add_shader(frag);
  return tex_shader_program.link();
}
