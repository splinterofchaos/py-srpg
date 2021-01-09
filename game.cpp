#include "game.h"

bool Turn::over() const {
  return did_pass || (did_move && did_action);
}

Error Game::init() {
  return glyph_shader_.init() &&
         marker_shader_.init() &&
         font_map_.init("font/LeagueMono-Bold.ttf") &&
         text_font_map_.init("font/LeagueMono-Regular.ttf");
}

void Game::set_grid(Grid grid) {
  for (auto [pos, tile] : grid) {
    GlyphRenderConfig rc(font_map_.get(tile.glyph), tile.fg_color,
                         tile.bg_color);
    rc.center();
    ecs().write_new_entity(Transform{pos, Transform::GRID}, std::vector{rc});
  }
  grid_ = std::move(grid);
}

unsigned int Game::gen_script_vars_and_id() {
  // TODO: care about integer overflow.
  unsigned int id = 1;
  if (!script_vars_.empty()) id = std::prev(script_vars_.end())->first + 1;
  script_vars_.emplace(id, Vars());
  return id;
}

void Game::add_independent_script(Script script) {
  independent_scripts_.emplace_back(gen_script_vars_and_id(), std::move(script));
}

void Game::add_ordered_script(Script script) {
  ordered_scripts_.emplace_back(gen_script_vars_and_id(), std::move(script));
}

void Game::execute_independent_scripts() {
  for (ScriptEngine& engine : independent_scripts_) {
    current_script_id_ = engine.id();
    ScriptResult r = engine.run(*this);
    if (r.code == ScriptResult::EXIT) script_vars_.erase(engine.id());
  }
  std::erase_if(independent_scripts_, std::mem_fn(&ScriptEngine::finished));
}

void Game::execute_ordered_scripts() {
  while (!ordered_scripts_.empty()) {
    auto script = std::prev(ordered_scripts_.end());
    current_script_id_ = script->id();
    ScriptResult r = script->run(*this);
    if (r.code != ScriptResult::EXIT) break;
    script_vars_.erase(script->id());
    ordered_scripts_.erase(script);
  }
}

Vars* Game::get_vars() {
  auto it = script_vars_.find(current_script_id_);
  if (it == script_vars_.end()) return nullptr;
  return &it->second;
}

void Game::lerp_camera_toward(glm::ivec2 pos, float rate_per_ms,
                              std::chrono::milliseconds ms) {
  glm::vec2 real_pos = glm::vec2(pos.x * TILE_SIZE, pos.y * TILE_SIZE);
  camera_offset_ = glm::mix(camera_offset_, real_pos,
                            rate_per_ms * ms.count());
}

void Game::set_camera_target(glm::vec2 pos) {
  if (pos == camera_target_) return;
  camera_target_ = pos;
  camera_center_watch_.reset();
  camera_center_watch_.start();
  camera_initial_offset_ = camera_offset_ / TILE_SIZE;
}

void Game::smooth_camera_towards_target(std::chrono::milliseconds ms) {
  if (camera_center_watch_.finished()) return;
  camera_center_watch_.consume(ms);
  float t = camera_center_watch_.ratio_consumed();
  camera_offset_ = glm::mix(glm::vec2(camera_initial_offset_),
                            glm::vec2(camera_target_),
                            glm::smoothstep(0.f, 1.f, t));
  camera_offset_ *= TILE_SIZE;
}

std::pair<EntityId, bool> actor_at(const Ecs& ecs, glm::ivec2 pos) {
  for (const auto [id, epos, unused_actor] :
       ecs.read_all<GridPos, Actor>())
    if (epos.pos == pos) return {id, true};
  return {EntityId(), false};
}

