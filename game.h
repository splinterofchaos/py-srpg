#pragma once

#include <chrono>
#include <list>

#include <glm/vec2.hpp>

#include "constants.h"
#include "components.h"
#include "decision.h"
#include "font.h"
#include "grid.h"
#include "shaders.h"
#include "timer.h"
#include "ui.h"

// Forward decl because Script relies on Game.
class Script;
class ScriptEngine;

struct Turn {
  bool did_move = false;
  bool did_action = false;
  bool did_pass = false;
  EntityId actor;

  void reset() {
    did_pass = did_action = did_move = false;
  }

  bool over() const;
};

class Game {
  Ecs ecs_;
  Grid grid_;
  Turn turn_;  // represents the current entitie's turn.
  Decision decision_;

  MarkerShaderProgram marker_shader_;
  GlyphShaderProgram glyph_shader_;
  FontMap font_map_;       // Normal font for in-game entities.
  FontMap text_font_map_;  // Font for text rendering.

  glm::vec2 camera_offset_ = glm::vec2(0.95f, 0.95f);
  glm::vec2 camera_initial_offset_ = glm::vec2(0.f, 0.f);
  glm::vec2 camera_target_ = glm::vec2(0.f, 0.f);
  StopWatch camera_center_watch_ = std::chrono::milliseconds(1000);

  // The time passed in this frame.
  std::chrono::milliseconds dt_;

  std::unique_ptr<TextBoxPopup> popup_box_;

  std::vector<ScriptEngine> independent_scripts_;
  std::list<ScriptEngine> ordered_scripts_;

public:
  Error init();
  const Grid& grid() const { return grid_; }

  void set_grid(Grid grid);

  Decision& decision() { return decision_; }
  const Decision& decision() const { return decision_; }

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

  std::unique_ptr<TextBoxPopup>& popup_box() { return popup_box_; }
  const std::unique_ptr<TextBoxPopup>& popup_box() const {
    return popup_box_;
  }

  bool ready_to_decide() {
    return decision().type == Decision::DECIDING;
  }

  std::chrono::milliseconds dt() const { return dt_; }
  template<typename Duration>
  void set_dt(Duration d) {
    dt_ = std::chrono::duration_cast<std::chrono::milliseconds>(d);
  }

  void add_independent_script(Script script);
  void add_ordered_script(Script script);

  bool have_ordered_scripts() const { return !ordered_scripts_.empty(); }

  void execute_independent_scripts();
  void execute_ordered_scripts();

  glm::vec2& camera_offset() { return camera_offset_; }
  const glm::vec2& camera_offset() const { return camera_offset_; }

  void lerp_camera_toward(glm::ivec2 pos, float rate_per_ms,
                          std::chrono::milliseconds ms);

  void smooth_camera_towards_target(std::chrono::milliseconds ms);

  glm::vec2 to_camera_pos(glm::ivec2 pos) const {
    return glm::vec2(pos.x * TILE_SIZE, pos.y * TILE_SIZE);
  }

  void set_camera_target(glm::vec2 pos);

  glm::vec3 to_graphical_pos(glm::vec2 pos, Transform::ZLayer z) const {
    return glm::vec3(pos.x * TILE_SIZE, pos.y * TILE_SIZE,
                     1.f + z * Transform::OFFSET_PER_LAYER) -
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
