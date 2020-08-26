#include "game.h"

bool Turn::over() const {
  return !waiting && (did_pass || (did_move && did_action));
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
    ecs().write_new_entity(Transform{pos, 0}, std::vector{rc});
  }
  grid_ = std::move(grid);
}

std::pair<EntityId, bool> actor_at(const Ecs& ecs, glm::ivec2 pos) {
  for (const auto [id, epos, unused_actor] :
       ecs.read_all<GridPos, Actor>())
    if (epos.pos == pos) return {id, true};
  return {EntityId(), false};
}

