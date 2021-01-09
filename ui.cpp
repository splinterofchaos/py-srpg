#include "ui.h"

#include <cctype>
#include <numeric>

#include "game.h"

// How much space to put between lines in a text block as a ratio of the tile
// size.
constexpr float LINE_SPACING = TEXT_SCALE;
// Amount of space between letters.
constexpr float LETTER_SPACING = TEXT_SCALE * 0.1f;
constexpr float SPACE_SIZE = LETTER_SPACING * 4.f;

constexpr std::chrono::milliseconds TYPING_DELAY(50);
constexpr std::chrono::milliseconds TYPING_COMMA_DELAY(100);
constexpr std::chrono::milliseconds TYPING_PERIOD_DELAY(250);

float text_width(FontMap& font_map, std::string_view text) {
  auto get_width =
    [&font_map](char c) { return font_map.get(c).bottom_right.x; };
  float w = reduce_by(text, 0.0f, get_width);
  // There will be some trailing space at the start. Make it the same at the
  // end.
  if (text.size()) w += font_map.get(text[0]).top_left.x;
  // Add spacing between the letters.
  w += text.size() * TEXT_SCALE * 0.1f;
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

  // TODO: In testing, if the upper_left argument is <-6, 3.5>, the actual
  // upper-left corner will be rendered at around <-5.4, 3.95>, based on the
  // text scale at the time of this writing. The math in this function appears
  // consistent, but consistently off.

  for (unsigned int i = 0; i < text_.size(); ++i) {
    text_[i].upper_left = upper_left +
                          glm::vec2(0.0f, -line * LINE_SPACING + 0.8f);

    float cursor = 0.0f;
    auto end = std::end(text_[i].char_entities);
    // Print each space-separated word one by one, breaking for new lines.
    int text_line = 0;
    // How far down from the upper left of this text box to print.
    float offset_y = 0.f;
    for (auto it = std::begin(text_[i].char_entities); it < end; ++it) {
      auto space = std::find_if(
          it, end, [](const Text::Char& ch) { return ch.c == ' '; });

      // Check if we need to break now.
      float estimated_width = std::accumulate(
          it, space, 0.0f,
          [&](float x, Text::Char ch) {
            return x + game.text_font_map().get(ch.c).bottom_right.x;
          }) * TEXT_SCALE + LETTER_SPACING * std::distance(it, space);
      if (cursor + estimated_width > width_) {
        cursor = 0;
        ++line;
        ++text_line;
      }

      offset_y = -(text_line + 1.f + TEXT_SCALE) * LINE_SPACING;

      for (; it != space; ++it) {
        glm::vec2 pos =
          text_[i].upper_left +
          glm::vec2(cursor + TEXT_SCALE/2, offset_y);
        const Glyph& glyph = game.text_font_map().get(it->c);
        GlyphRenderConfig rc(glyph, glm::vec4(1.f));
        rc.offset_scale = TEXT_SCALE;
        it->id = text_pool_.create_new(
            game.ecs(), Transform{pos, Transform::WINDOW_TEXT},
            std::vector<GlyphRenderConfig>{rc});

        cursor += glyph.bottom_right.x * TEXT_SCALE + LETTER_SPACING;
      }

      cursor += SPACE_SIZE;
    }

    line += 1;

    text_[i].lower_right = text_[i].upper_left + glm::vec2(width_, offset_y);
  }

  center_ = glm::vec2(upper_left.x + width_ / 2.0f,
                      upper_left.y - line * LINE_SPACING / 2.f);

  window_background_pool_.create_new(
      game.ecs(),
      Transform{center_, Transform::WINDOW_BACKGROUND},
      Marker(glm::vec4(0.f, 0.2f, 0.f, .9f), {width_, line * LINE_SPACING}));

  after_build_box();
}

void DialogueBox::after_build_box() {
  for (Text& txt : text_) {
    for (Text::Char ch : txt.char_entities) {
      game.ecs().deactivate(ch.id);
    }
  }

  typing_watch_.set_duration(std::chrono::milliseconds(60));
  typing_watch_.start();
}

bool DialogueBox::finished_activating() const {
  return text_activated_ >= text_.size();
}

void DialogueBox::activate_next() {
  Text::Char ch{' ', EntityId()};
  while (ch.c == ' ') {
    ch = text_[text_activated_].char_entities[entities_activated_++];
  }
  game.ecs().activate(ch.id);

  std::chrono::milliseconds delay = TYPING_DELAY;
  if (ch.c == ',')
    delay = TYPING_COMMA_DELAY;
  else if (ch.c == '.' || ch.c == ';' || ch.c == '!' || ch.c == '?')
    delay = TYPING_PERIOD_DELAY;
  typing_watch_.set_duration(delay);

  if (entities_activated_ == text_[text_activated_].char_entities.size()) {
    ++text_activated_;
    entities_activated_ = 0;
  }
}

TextBoxPopup::OnClickResponse DialogueBox::on_left_click(glm::vec2 mouse_pos) {
  // Make sure all the text is printed before checking specific dialogue boxes.
  if (!finished_activating()) {
    do activate_next(); while (!finished_activating());
    return KEEP_OPEN;
  }

  bool has_on_click = false;
  bool did_on_click = false;
  for (const Text& text : text_) {
    has_on_click |= bool(text.on_click);
    if (in_between(mouse_pos, text.upper_left, text.lower_right)) {
      if (text.on_click) {
        text.on_click();
        did_on_click = true;
      }
      break;
    }
  }

  return !has_on_click || did_on_click ? DESTROY_ME : KEEP_OPEN;
}

void DialogueBox::update(std::chrono::milliseconds dt) {
  if (finished_activating()) return;

  typing_watch_.consume(dt);
  if (!typing_watch_.finished()) return;

  typing_watch_.reset();
  typing_watch_.start();

  activate_next();
}
