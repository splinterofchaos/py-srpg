#pragma once

#include <string_view>
#include <unordered_map>

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
