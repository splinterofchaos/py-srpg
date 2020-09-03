#pragma once

#include <chrono>

#include <glm/vec2.hpp>

#include "components.h"
#include "font.h"
#include "grid.h"
#include "shaders.h"

constexpr inline float TILE_SIZE = 0.1f;

struct Turn {
  bool did_move = false;
  bool did_action = false;
  bool did_pass = false;
  EntityId actor;

  // True if we're waiting for some action to complete.
  bool waiting = false;

  void reset() {
    waiting = did_pass = did_action = did_move = false;
  }

  bool over() const;
};

class Game {
  Ecs ecs_;
  Grid grid_;
  Turn turn_;  // represents the current entitie's turn.

  MarkerShaderProgram marker_shader_;
  GlyphShaderProgram glyph_shader_;
  FontMap font_map_;       // Normal font for in-game entities.
  FontMap text_font_map_;  // Font for text rendering.

  glm::vec2 camera_offset_ = glm::vec2(0.95f, 0.95f);

public:
  Error init();
  const Grid& grid() const { return grid_; }

  void set_grid(Grid grid);

  Ecs& ecs() { return ecs_; }
  const Ecs& ecs() const { return ecs_; }

  Turn& turn() { return turn_; }
  const Turn& turn() const { return turn_; }

  const MarkerShaderProgram& marker_shader() const { return marker_shader_; }
  const GlyphShaderProgram& glyph_shader() const { return glyph_shader_; }

  FontMap& font_map() { return font_map_; }
  const FontMap& font_map() const { return font_map_; }

  FontMap& text_font_map() { return text_font_map_; }
  const FontMap& text_font_map() const { return text_font_map_; }

  glm::vec2& camera_offset() { return camera_offset_; }
  const glm::vec2& camera_offset() const { return camera_offset_; }

  void lerp_camera_toward(glm::ivec2 pos, float rate_per_ms,
                          std::chrono::milliseconds ms);

  glm::vec3 to_graphical_pos(glm::vec2 pos, int z) const {
    return glm::vec3(pos.x * TILE_SIZE, pos.y * TILE_SIZE, z) -
           glm::vec3(camera_offset_, 0.f);
  }

  glm::vec3 to_graphical_pos(const Transform& transform) const {
    return to_graphical_pos(transform.pos, transform.z);
  }

  glm::ivec2 top_left_screen_tile() const {
    // If to_graphical_pos(p) == <-1, 1>, then
    // pos.x * TILE_SIZE - camera_offset_.x = -1
    // solve for pos.x:
    int x = glm::ceil((camera_offset_.x - 1.f) / TILE_SIZE);
    // For y, it's: pos.y * TILE_SIZE - camera_offset_.y = 1
    int y = glm::floor((camera_offset_.y + 1.f) / TILE_SIZE);
    return {x, y};
  }

  glm::ivec2 bottom_right_screen_tile() const {
    int x = glm::floor((camera_offset_.x + 1.f) / TILE_SIZE);
    int y = glm::ceil((camera_offset_.y - 1.f) / TILE_SIZE);
    return {x, y};
  }
};

std::pair<EntityId, bool> actor_at(const Ecs& ecs, glm::ivec2 pos);
