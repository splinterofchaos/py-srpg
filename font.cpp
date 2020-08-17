
#include <string_view>

#include <SDL2/SDL.h>

#include "font.h"

static constexpr unsigned int PIXEL_SIZE = 64;
static constexpr float PIXEL_SIZE_F = 64.f;

// The freetype library is designed such that if you want to render a glyph
// at an x y coord, the tail of letters like "j" may go lower than y. That
// makes rendering withing an xy coord and st bounds difficult. This is a
// best guess at the number of pixels up from the bottom of a tile all values
// freetype returns are at.
static constexpr int BASELINE = 10;

static Error freetype_error(std::string_view action, int error) {
  return Error(action, ": ", std::to_string(error));
}

// Returns a vec2 representing the coordinates on a tile as a ratio of that
// tile's height and width. Note that y is in relation to the baseline, where
// positive is up and negative is down, but needs to be converted such that
// zero is the top and one is the bottom.
static glm::vec2 coord_to_glyph_pos(int x, int y) {
  return {
    glm::clamp(float(x) / PIXEL_SIZE_F, 0.f, 1.f),
    glm::clamp((PIXEL_SIZE_F - (y + BASELINE)) / PIXEL_SIZE_F, 0.f, 1.f)};
}

Error FontMap::init(const char* const ttf_file) {
  if (auto error = FT_Init_FreeType(&freetype_)) return freetype_error("initializing freetype", error);
  if (auto error = FT_New_Face(freetype_, ttf_file, 0, &face_))
    return freetype_error(concat_strings("Loading TTF file '", ttf_file, "'"),
                          error);

  float h_dpi, v_dpi;
  SDL_GetDisplayDPI(0, nullptr, &h_dpi, &v_dpi);
  //if (auto error = FT_Set_Char_Size(face_, 0, 32*64, h_dpi, v_dpi))
  //  freetype_error("setting freetype char size", error);
  if (auto error = FT_Set_Pixel_Sizes(face_, 0, PIXEL_SIZE))
    return freetype_error("Setting pixel sizes", error);

  return Error();
}

Error FontMap::get_safe(char c, Glyph** glyph) {
  auto it = char_to_texture_.find(c);
  if (it == char_to_texture_.end()) {
    if (auto error = FT_Load_Char(face_, c, FT_LOAD_RENDER)) {
      return freetype_error(concat_strings("FT_Load_Char for glyph (",
                                           std::to_string(c), ")"),
                            error);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLuint texture = gl::genTexture();
    gl::bindTexture(GL_TEXTURE_2D, texture);
    gl::texImage2D(GL_TEXTURE_2D, 0, GL_RED, face_->glyph->bitmap.width,
                 face_->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE,
                 face_->glyph->bitmap.buffer);
    gl::texParameter(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::texParameter(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::texParameter(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::texParameter(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    bool unused;
    std::tie(it, unused) = char_to_texture_.emplace(
        c,
        Glyph{texture,
              coord_to_glyph_pos(face_->glyph->bitmap_left,
                                 face_->glyph->bitmap_top),
              coord_to_glyph_pos(face_->glyph->bitmap_left +
                                 face_->glyph->bitmap.width,
                                 face_->glyph->bitmap_top -
                                 face_->glyph->bitmap.rows)});
  }

  *glyph = &it->second;
  return Error();
}

const Glyph& FontMap::get(char c) {
  Glyph* glyph;
  get_safe(c, &glyph);
  return *glyph;
}
