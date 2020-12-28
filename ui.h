#pragma once

#include <glm/vec2.hpp>
#include <functional>
#include <string_view>

#include "constants.h"
#include "include/ecs.h"
#include "include/math.h"
#include "font.h"

inline constexpr float MENU_WIDTH = 10.0f;
inline constexpr float DIALOGUE_WIDTH = 20.f;

class Game;  // Foward declaration as TextBoxPopup references Game.

struct Text {
  glm::vec2 upper_left;
  glm::vec2 lower_right;
  std::string text;
  std::vector<EntityId> text_entities;

  std::function<void()> on_click;

  // Often when constructing text, we won't know where the text is
  // positioned yet and will construct the characters later.
  template<typename...String>
  explicit Text(String... strings)
      : text(concat_strings(std::move(strings)...)) { }

  Text(std::string text, std::function<void()> on_click)
    : text(std::move(text)), on_click(std::move(on_click)) { }
};

float text_width(FontMap& font_map, std::string_view text);

class TextBoxPopup {
 protected:
  EntityPool text_pool_;

  std::vector<Text> text_;

  // Even if the window background will strictly be one entity, the pool
  // abstracts needing to decide whether to create a new entity or reuse
  // the old.
  EntityPool window_background_pool_;

  Game& game;

  bool active_;

  float width_;

public:
  enum OnClickResponse {
    DESTROY_ME,
    KEEP_OPEN
  };

  TextBoxPopup(Game& game, float width=MENU_WIDTH);

  bool active() const { return active_; }

  // Deactivates the entities in this popup.
  void clear();

  // Erases all entities associated with this popup.
  void destroy();

  ~TextBoxPopup() { destroy(); }

  // Run if the player left clicks anywhere on the screen.
  virtual OnClickResponse on_left_click(glm::vec2 mouse_pos) {
    return DESTROY_ME;
  }

  template<typename...String>
  void add_text(String...strings) {
    text_.emplace_back(std::move(strings)...);
  }

  void add_text_with_onclick(std::string text, std::function<void()> onclick) {
    text_.emplace_back(std::move(text), std::move(onclick));
  }

  // Builds the window background adjacent to `pos`, defaulting to the right,
  // but switching to the left if its edges would go off screen.
  void build_text_box_next_to(glm::vec2 pos);

  // Builds the window background and text given the window's dimensions.
  void build_text_box_at(glm::vec2 upper_left);
};

class SelectionBox : public TextBoxPopup {
 public:
  using TextBoxPopup::TextBoxPopup;

  OnClickResponse on_left_click(glm::vec2 mouse_pos) override {
    for (const Text& text : text_) {
      if (in_between(mouse_pos, text.upper_left, text.lower_right)) {
        text.on_click();
        break;
      }
    }
    return DESTROY_ME;
  }
};

class DialogueBox : public TextBoxPopup {
 public:

  using TextBoxPopup::TextBoxPopup;

  OnClickResponse on_left_click(glm::vec2 mouse_pos) override {
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
};
