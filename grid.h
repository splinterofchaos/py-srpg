#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace std {
  template<> struct hash<glm::ivec2> {
    std::size_t operator()(const glm::ivec2& v) const noexcept {
      return hash<int>{}(v.x) ^ (hash<int>{}(v.y) << 1);
    }
  };
}

struct Tile {
  bool walkable = false;
  char glyph = ' ';
  glm::vec4 fg_color;
  glm::vec4 bg_color;
};

struct Grid {
  std::unordered_map<glm::ivec2, Tile> data_;

  // Returned by get or operator[] if a lookup is done of a tile and we don't
  // contain this tile.
  Tile dummy_notreal;

public:
  bool has(glm::ivec2 pos) { return data_.contains(pos); }

  std::pair<Tile&, bool> get(glm::ivec2 pos) {
    auto it = data_.find(pos);
    if (it != data_.end()) return {it->second, true};
    return {dummy_notreal, false};
  }

  std::pair<const Tile&, bool> get(glm::ivec2 pos) const {
    auto it = data_.find(pos);
    if (it != data_.end()) return {it->second, true};
    return {dummy_notreal, false};
  }

  Tile& at(glm::ivec2 pos) { return get(pos).first; }
  const Tile& at(glm::ivec2 pos) const { return get(pos).first; }

  Tile& operator[](glm::ivec2 pos) {
    return data_.emplace(pos, dummy_notreal).first->second;
  }

  auto begin() { return data_.begin(); }
  auto begin() const { return data_.begin(); }
  auto end() { return data_.end(); }
  auto end() const { return data_.end(); }
};

Grid grid_from_string(std::string_view grid_s,
                      const std::unordered_map<char, Tile>& tile_types);

Grid arena_grid(glm::ivec2 dimensions, const Tile& wall, const Tile& floor);

std::vector<glm::ivec2> adjacent_steps();
std::vector<glm::ivec2> adjacent_positions(glm::ivec2);
