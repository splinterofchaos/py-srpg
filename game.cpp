#include "game.h"

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
    ecs().write_new_entity(Transform{pos, 0}, rc);
  }
  grid_ = std::move(grid);
}

