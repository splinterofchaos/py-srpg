#include "user_input.h"

#include <SDL2/SDL.h>

void UserInput::reset(const Game& game) {
  // The selector graphically represents what tile the mouse is hovering
  // over. This might be a bit round-a-bout, but find where to place the
  // selector on screen by finding its position on the grid. Later, we
  // convert it to a grid-a-fied screen position.
  int mouse_x, mouse_y;
  SDL_GetMouseState(&mouse_x, &mouse_y);
  glm::vec2 mouse_screen_pos(float(mouse_x * 2) / WINDOW_WIDTH - 1,
                             float(-mouse_y * 2) / WINDOW_HEIGHT + 1);
  mouse_pos_f = (mouse_screen_pos + TILE_SIZE/2) / TILE_SIZE +
    game.camera_offset() / TILE_SIZE;
  mouse_pos = glm::floor(mouse_pos_f);

  left_click = false;
  right_click = false;
  keys_pressed.clear();
}
