#pragma once

#include <cstdint>
#include <ft2build.h>

#include "graphics.hpp"
#include "error.hpp"

#include FT_FREETYPE_H

void WriteAscii(PixelWriter& writer, Vector2D<int> pos, char c, const PixelColor& color);
void WriteString(PixelWriter& writer, Vector2D<int> pos, const char* s, const PixelColor& color);

int CountUTF8Size(uint8_t c);
std::pair<char32_t, int> ConvertUTF8To32(const char* u8);
bool IsHankaku(char32_t c);
WithError<FT_Face> NewFTFace(int size = 16, bool bold = false);
Error WriteUnicode(PixelWriter& writer, Vector2D<int> pos,
                  char32_t c, const PixelColor& color,int size = 16, bool bold = false);


void InitializeFont();