#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "font.h"
#include "graphics.h"
#include "util.h"

// The vertex type used for sending data to the graphics card.
struct Vertex {
  glm::vec2 pos;
  glm::vec2 tex_coord;
};

// The glyph shader program below uses the configuration here to understand how
// to render it.
struct GlyphRenderConfig {
  GLuint texture = 0;
  glm::vec2 top_left, bottom_right;
  glm::vec4 fg_color, bg_color;

  GlyphRenderConfig() = default;
  GlyphRenderConfig(const Glyph& glyph, glm::vec4 fg_color,
                    glm::vec4 bg_color = glm::vec4())
      : fg_color(fg_color), bg_color(bg_color) {
    texture = glyph.texture;
    top_left = glyph.top_left;
    bottom_right = glyph.bottom_right;
  }

  // By default, the glyph will be drawn proportionate to its space on a tile
  // which is not representative of the width of the tile so it will bias to
  // the left. center() corrects it for tile-based rendering.
  void center();
};

// This shader program handles its own initialization and rendering of glyphs.
// The user is expected to set up the coordinate system and know the size of
// each tile.
class GlyphShaderProgram {
  GlProgram program_;

  GLuint vbo_ = 0;

  GLint vertex_pos_attr_ = -1;
  GLint tex_coord_attr_ = -1;
  GLint transform_uniform_ = -1;
  GLint texture_uniform_ = -1;
  GLint bg_color_uniform_ = -1;
  GLint fg_color_uniform_ = -1;
  GLint bottom_left_uniform_ = -1;
  GLint top_right_uniform_ = -1;

public:
  Error init();
  void render_glyph(glm::vec3 pos, float size,
                    const GlyphRenderConfig& config);
};

// Markers are drawn over most other elements and can denote tiles to which an
// actor can move, attack, or the cursor's position.
class MarkerShaderProgram {
  GlProgram program_;
  GLuint vbo_ = 0;

  GLint vertex_pos_attr_ = -1;
  GLint transform_uniform_ = -1;
  GLint color_uniform_ = -1;

public:
  Error init();

  void render_marker(glm::vec3 pos, float size, glm::vec4 color);
};
