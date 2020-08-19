#include "grid.h"

Grid grid_from_string(std::string_view grid_s,
                      const std::unordered_map<char, Tile>& tile_types) {
  // Due to a slight oddity, our Y-axis has "up" as positive and down as
  // negative, but reading a string would have us make down positive so we'd
  // get an upside down grid. Start at the top and work down.
  std::size_t height_minus_one =
    std::count(grid_s.begin(), grid_s.end(), '\n');
  glm::ivec2 pos = {0, height_minus_one};
  Grid grid;
  for (char c : grid_s) {
    if (c == '\n') {
      pos = {0, pos.y - 1};
      continue;
    }

    auto it = tile_types.find(c);
    if (it != tile_types.end()) grid[pos] = it->second;

    ++pos.x;
  }

  return grid;
}
