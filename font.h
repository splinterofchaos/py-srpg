#pragma once

#include <string_view>
#include <unordered_map>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <glm/vec2.hpp>

#include "glpp.h"
#include "util.h"

struct Glyph {
  GLuint texture;
  glm::vec2 top_left;
  glm::vec2 bottom_right;
};

class FontMap {
  FT_Library freetype_;
  FT_Face face_;

  std::unordered_map<char, Glyph> char_to_texture_;

public:
  Error init(const char* const ttf_file);

  Error get_safe(char c, Glyph** glyph);
  const Glyph& get(char c);
};
