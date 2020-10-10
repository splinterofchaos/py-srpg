#pragma once

#include <glm/vec2.hpp>
#include <functional>

#include "include/ecs.h"
#include "font.h"

struct Text {
  glm::vec2 upper_left;
  std::string text;
  std::vector<EntityId> text_entities;

  std::function<void()> on_click;

  // Often when constructing text, we won't know where the text is
  // positioned yet and will construct the characters later.
  template<typename...String>
  explicit Text(String... strings)
      : text(concat_strings(std::move(strings)...)) { }
};
