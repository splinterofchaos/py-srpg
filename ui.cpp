#include "ui.h"

#include <numeric>

#include "game.h"

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

TextBoxPopup::TextBoxPopup(Game& game, float width)
  : game(game), active_(false), width_(width) { }

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

void TextBoxPopup::build_text_box_next_to(glm::vec2 pos) {
  active_ = true;

  const float h = text_.size();

  glm::vec2 start = pos + glm::vec2(1.f, 0.f);
  if (start.x + MENU_WIDTH > game.bottom_right_screen_tile().x)
    start.x -= 2.f + width_;
  start.y = std::max(start.y + 0.5f, game.bottom_right_screen_tile().y + h);
  build_text_box_at(start);
}

void TextBoxPopup::build_text_box_at(glm::vec2 upper_left) {
  // Keep this int signed! Allows us to use terse (-line + 0.5f) syntax.
  int line = 0;
  for (unsigned int i = 0; i < text_.size(); ++i) {
    text_[i].upper_left = upper_left + glm::vec2(0.0f, -line + 0.5f);

    float cursor = 0.0f;
    auto end = std::end(text_[i].text);
    // Print each space-separated word one by one, breaking for new lines.
    int text_line = 0;
    for (auto it = std::begin(text_[i].text); it < end; ++it) {
      auto space = std::find(it, end, ' ');

      // Check if we need to break now.
      float estimated_width = std::accumulate(
          it, space, 0.0f,
          [&](float x, char c) {
            return x + game.text_font_map().get(c).bottom_right.x;
          });
      if (cursor + estimated_width > width_) {
        cursor = 0;
        ++line;
        ++text_line;
      }

      for (; it != space; ++it) {
        glm::vec2 pos = text_[i].upper_left +
                        glm::vec2(cursor + 0.5f, -text_line - 1.f);
        const Glyph& glyph = game.text_font_map().get(*it);
        GlyphRenderConfig rc(glyph, glm::vec4(1.f));
        EntityId id = text_pool_.create_new(
            game.ecs(), Transform{pos, Transform::WINDOW_TEXT},
            std::vector<GlyphRenderConfig>{rc});

        text_[i].text_entities.push_back(id);

        cursor += glyph.bottom_right.x + TILE_SIZE * 0.1f;
      }

      cursor += 0.5f;
    }

    line += 1;

    text_[i].lower_right = text_[i].upper_left +
                           glm::vec2(width_, -(text_line + 1));
  }

  center_ = glm::vec2(upper_left.x + width_ / 2.0f,
                      upper_left.y - line / 2.f);

  window_background_pool_.create_new(
      game.ecs(),
      Transform{center_, Transform::WINDOW_BACKGROUND},
      Marker(glm::vec4(0.f, 0.2f, 0.f, .9f), {width_, line}));

}
