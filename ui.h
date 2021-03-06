#pragma once

#include <glm/vec2.hpp>
#include <functional>
#include <string_view>

#include "constants.h"
#include "include/ecs.h"
#include "include/math.h"
#include "include/timer.h"
#include "font.h"

inline constexpr float MENU_WIDTH = 10.0f;
inline constexpr float DIALOGUE_WIDTH = 10.f;

class Game;  // Foward declaration as TextBoxPopup references Game.

struct Text {
  // Each char in this text will be tied to an entity, though spaces will keep
  // the default, NOT_AN_ID value in that field.
  struct Char { char c; EntityId id; };

  glm::vec2 upper_left;
  glm::vec2 lower_right;
  std::vector<Char> char_entities;

  std::function<void()> on_click;

  // Often when constructing text, we won't know where the text is
  // positioned yet and will construct the characters later.
  template<typename...String>
  explicit Text(String... strings) {
    for (char c : concat_strings(std::move(strings)...)) {
      char_entities.push_back({c, EntityId()});
    }
  }

  Text(std::string text, std::function<void()> _on_click)
    : Text(std::move(text)) {
    on_click = std::move(_on_click);
  }
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

  float width_;  // Used while constructing the text.

  glm::vec2 center_;

  // Used by DialogueBox to deactivate all text after the build process.
  virtual void after_build_box() { }

public:
  enum OnClickResponse {
    DESTROY_ME,
    KEEP_OPEN
  };

  TextBoxPopup(Game& game, float width=MENU_WIDTH);

  bool active() const { return active_; }

  glm::vec2 center() const { return center_; }

  // Deactivates the entities in this popup.
  void clear();

  // Erases all entities associated with this popup.
  void destroy();

  ~TextBoxPopup() { destroy(); }

  virtual void update(std::chrono::milliseconds dt) { }

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

// The dialogue box differs a little from the others in that it prints each
// character one-by-one instead of putting them all on screen at once.
class DialogueBox : public TextBoxPopup {
  // This watch manages the delay between printing characters.
  StopWatch typing_watch_;

  // The number of Text objects where all their entities are active.
  unsigned int text_activated_ = 0;
  // The number of entities in the current Text object we're activating already
  // done.
  unsigned int entities_activated_ = 0;

  void activate_next();
  bool finished_activating() const;

 public:
  using TextBoxPopup::TextBoxPopup;

  OnClickResponse on_left_click(glm::vec2 mouse_pos) override;

  void after_build_box() override;

  void update(std::chrono::milliseconds dt) override;
};
