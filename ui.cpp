#include "game.h"
#include "ui.h"

constexpr float MENU_WIDTH = 10.0f;

float text_width(FontMap& font_map, std::string_view text) {
  auto get_width =
    [&font_map](char c) { return font_map.get(c).bottom_right.x; };
  float w = reduce_by(text, 0.0f, get_width);
  // There will be some trailing space at the start. Make it the same at the
  // end.
  if (text.size()) w += font_map.get(text[0]).top_left.x;
  // Add spacing between the letters.
  w += text.size() * TILE_SIZE * 0.1f;
  return w;
}

TextBoxPopup::TextBoxPopup(Game& game) : game(game), active_(false) { }

void TextBoxPopup::clear() {
  text_pool_.deactivate_pool(game.ecs());
  window_background_pool_.deactivate_pool(game.ecs());
  text_.clear();
  active_ = false;
}

void TextBoxPopup::destroy() {
  text_pool_.destroy_pool(game.ecs());
  window_background_pool_.destroy_pool(game.ecs());
  active_ = false;
}

void TextBoxPopup::create_text_box(EntityId id) {
  active_ = true;

  float h = text_.size();

  GridPos pos = game.ecs().read_or_panic<GridPos>(id);

  glm::vec2 start = glm::vec2(pos.pos) + glm::vec2(1.f, 0.f);
  if (start.x + MENU_WIDTH > game.bottom_right_screen_tile().x)
    start.x -= 2.f + MENU_WIDTH;
  start.y = std::max(start.y + 0.5f, game.bottom_right_screen_tile().y + h);

  glm::vec2 end = start + glm::vec2(MENU_WIDTH, -h);
  glm::vec2 center = (start + end) / 2.f;

  window_background_pool_.create_new(
      game.ecs(),
      Transform{center, Transform::WINDOW_BACKGROUND},
      Marker(glm::vec4(0.f, 0.2f, 0.f, .9f), glm::vec2(MENU_WIDTH, h)));

  for (unsigned int i = 0; i < text_.size(); ++i) {
    text_[i].upper_left = start + glm::vec2(0.0f, -float(i) + 0.5f);

    float cursor = 0.5f;
    for (char c : text_[i].text) {
      if (c == ' ') {
        cursor += 1;
        continue;
      }

      glm::vec2 pos = text_[i].upper_left +
                      glm::vec2(cursor, -1.f);
      const Glyph& glyph = game.text_font_map().get(c);
      GlyphRenderConfig rc(glyph, glm::vec4(1.f));
      EntityId id = text_pool_.create_new(
          game.ecs(), Transform{pos, Transform::WINDOW_TEXT},
          std::vector<GlyphRenderConfig>{rc});

      text_[i].text_entities.push_back(id);

      cursor += glyph.bottom_right.x + TILE_SIZE * 0.1f;
    }

    text_[i].lower_right = text_[i].upper_left +
                           glm::vec2(MENU_WIDTH, -1.0f);
  }
}
