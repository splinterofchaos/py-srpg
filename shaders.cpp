#include "shaders.h"

#include "math.h"

static GLuint rectangle_vbo(glm::vec3 dimensions = glm::vec3(1.f, 1.f, 0),
                            glm::vec2 tex_lower_left = glm::vec2(0.f, 0.f),
                            glm::vec2 tex_size = glm::vec2(1.f, 1.f)) {
  Vertex vertecies[] = {
    {dimensions / 2.f,          tex_lower_left + just_x(tex_size)},
    {flip_x(dimensions / 2.f),  tex_lower_left},
    {-dimensions / 2.f,         tex_lower_left + just_y(tex_size)},
    {flip_x(-dimensions) / 2.f, tex_lower_left + tex_size}
  };
  GLuint vbo = gl::genBuffer();
  gl::bindBuffer(GL_ARRAY_BUFFER, vbo);
  gl::bufferData(GL_ARRAY_BUFFER, vertecies, GL_STATIC_DRAW);

  return vbo;
}

void GlyphRenderConfig::center() {
  glm::vec2 current_center = (top_left + bottom_right) / 2.f;
  glm::vec2 offset = glm::vec2(0.5f, 0.5f) - current_center;
  top_left += offset;
  bottom_right += offset;
}

Error GlyphShaderProgram::init() {
  vbo_ = rectangle_vbo();

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
    uniform sampler2D tex;
    uniform vec4 bg_color;
    uniform vec4 fg_color;
    uniform vec2 top_left;
    uniform vec2 bottom_right;
    in vec2 TexCoord;
    out vec4 FragColor;
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

  program_.add_shader(verts);
  program_.add_shader(frag);
  if (Error e = program_.link(); !e.ok) return e;

  return program_.attribute_location("vertex_pos", vertex_pos_attr_) &&
         program_.attribute_location("tex_coord", tex_coord_attr_) &&
         program_.uniform_location("tex", texture_uniform_) &&
         program_.uniform_location("transform", transform_uniform_) &&
         program_.uniform_location("fg_color", fg_color_uniform_) &&
         program_.uniform_location("bg_color", bg_color_uniform_) &&
         program_.uniform_location("top_left", bottom_left_uniform_) &&
         program_.uniform_location("bottom_right", top_right_uniform_);
}

void GlyphShaderProgram::render_glyph(glm::vec3 pos, float size,
                                      const GlyphRenderConfig& render_config) {
  program_.use();

  glBindTexture(GL_TEXTURE_2D, render_config.texture);

  gl::bindBuffer(GL_ARRAY_BUFFER, vbo_);
  gl::uniform(texture_uniform_, render_config.texture);
  gl::uniform4v(fg_color_uniform_, 1, glm::value_ptr(render_config.fg_color));
  gl::uniform4v(bg_color_uniform_, 1, glm::value_ptr(render_config.bg_color));
  gl::uniform2v(bottom_left_uniform_, 1,
                glm::value_ptr(render_config.top_left));
  gl::uniform2v(top_right_uniform_, 1,
                glm::value_ptr(render_config.bottom_right));

  gl::enableVertexAttribArray(vertex_pos_attr_);
  gl::vertexAttribPointer<float>(vertex_pos_attr_, 2, GL_FALSE, &Vertex::pos);

  gl::enableVertexAttribArray(tex_coord_attr_);
  gl::vertexAttribPointer<float>(tex_coord_attr_, 2, GL_FALSE,
                                 &Vertex::tex_coord);

  glUniformMatrix4fv(transform_uniform_, 1, GL_FALSE,
                     glm::value_ptr(transformation(pos, 0, size)));

  gl::drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

Error MarkerShaderProgram::init() {
  vbo_ = rectangle_vbo();

  Shader verts(Shader::Type::VERTEX);
  verts.add_source(R"(
    #version 140
    in vec3 vertex_pos;
    uniform mat4 transform;

    void main() {
      gl_Position = transform * vec4(vertex_pos, 1);
    }
  )");
  if (Error e = verts.compile(); !e.ok) return e;

  Shader frag(Shader::Type::FRAGMENT);
  frag.add_source(R"(
    #version 140
    uniform vec4 color;

    out vec4 FragColor;

    void main() {
      FragColor = color;
    }
  )");
  if (Error e = frag.compile(); !e.ok) return e;

  program_.add_shader(verts);
  program_.add_shader(frag);
  if (Error e = program_.link(); !e.ok) return e;

  return program_.attribute_location("vertex_pos", vertex_pos_attr_) &&
         program_.uniform_location("transform", transform_uniform_) &&
         program_.uniform_location("color", color_uniform_);
}

void MarkerShaderProgram::render_marker(glm::vec3 pos, float size,
                                        glm::vec4 color) {
  program_.use();

  gl::bindBuffer(GL_ARRAY_BUFFER, vbo_);

  gl::uniform4v(color_uniform_, 1, glm::value_ptr(color));

  gl::enableVertexAttribArray(vertex_pos_attr_);
  gl::vertexAttribPointer<float>(vertex_pos_attr_, 2, GL_FALSE, &Vertex::pos);

  glUniformMatrix4fv(transform_uniform_, 1, GL_FALSE,
                     glm::value_ptr(transformation(pos, 0, size)));

  gl::drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
