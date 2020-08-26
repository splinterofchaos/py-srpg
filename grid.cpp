#include "grid.h"

#include <limits>
#include <unordered_set>

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

Grid arena_grid(glm::ivec2 dimensions, const Tile& wall, const Tile& floor) {
  Grid grid;
  for (int x = 0; x < dimensions.x; ++x) {
    for (int y = 0; y < dimensions.y; ++y) {
      bool on_edge = x == 0 || x == dimensions.x - 1 ||
                     y == 0 || y == dimensions.y - 1;
      grid[{x, y}] = on_edge ? wall : floor;
    }
  }
  return grid;
}

std::vector<glm::ivec2> adjacent_steps() {
  return {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
}

std::vector<glm::ivec2> adjacent_positions(glm::ivec2 p) {
  std::vector<glm::ivec2> adj = adjacent_steps();
  for (glm::ivec2& v : adj) v += p;
  return adj;
}
