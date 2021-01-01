#pragma once

#include <glm/vec2.hpp>
#include <unordered_set>

#include "game.h"

struct UserInput {
  glm::vec2 mouse_pos_f;
  glm::ivec2 mouse_pos;
  bool left_click;
  bool right_click;
  // Inclusion in this set means that a key is pressed.
  std::unordered_set<char> keys_pressed;

  bool pressed(char c) { return keys_pressed.contains(c); }
  void press(char c) { keys_pressed.insert(c); }

  void reset(const Game& game);
};

