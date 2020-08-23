#pragma once

#include <glm/vec2.hpp>

#include "components.h"
#include "font.h"
#include "grid.h"
#include "shaders.h"

constexpr inline float TILE_SIZE = 0.1f;

class Game {
  Ecs ecs_;
  Grid grid_;

  MarkerShaderProgram marker_shader_;
  GlyphShaderProgram glyph_shader_;
  FontMap font_map_;
  glm::vec2 camera_offset_ = glm::vec2(0.95f, 0.95f);

public:
  Error init();
  const Grid& grid() const { return grid_; }

  void set_grid(Grid grid);

  Ecs& ecs() { return ecs_; }
  const Ecs& ecs() const { return ecs_; }

  const MarkerShaderProgram& marker_shader() const { return marker_shader_; }
  const GlyphShaderProgram& glyph_shader() const { return glyph_shader_; }

  FontMap& font_map() { return font_map_; }
  const FontMap& font_map() const { return font_map_; }

  const glm::vec2& camera_offset() const { return camera_offset_; }

  glm::vec3 to_graphical_pos(glm::vec2 pos, int z) const {
    return glm::vec3(pos.x * TILE_SIZE, pos.y * TILE_SIZE, z) -
           glm::vec3(camera_offset_, 0.f);
  }

  glm::vec3 to_graphical_pos(const Transform& transform) const {
    return to_graphical_pos(transform.pos, transform.z);
  }
};


