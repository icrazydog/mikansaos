/**
 * @file font.cpp
 *
 * draw font
 */

#include "font.hpp"

#include "fat.hpp"

//font_a
/*const uint8_t kFontA[16] = {
  0b00000000, //
  0b00011000, //    **
  0b00011000, //    **
  0b00011000, //    **
  0b00011000, //    **
  0b00100100, //   *  *
  0b00100100, //   *  *
  0b00100100, //   *  *
  0b00100100, //   *  *
  0b01111110, //  ******
  0b01000010, //  *    *
  0b01000010, //  *    *
  0b01000010, //  *    *
  0b11100111, // ***  ***
  0b00000000, //
  0b00000000, //

};
*/

extern const uint8_t _binary_hankaku_bin_start;
extern const uint8_t _binary_hankaku_bin_end;
extern const uint8_t _binary_hankaku_bin_size;


namespace {
  const uint8_t* GetFont(char c){
    auto index = 16 * static_cast<unsigned int>(c);
    if(index >= reinterpret_cast<uintptr_t>(&_binary_hankaku_bin_size)){
      return nullptr;  
    }
    return &_binary_hankaku_bin_start + index;
  }

  FT_Library ft_library;
  std::vector<uint8_t>* nihongo_buf;
  std::vector<uint8_t>* nihongo_bold_buf;

  Error RenderUnicode(char32_t c, FT_Face face) {
    const auto glyph_index = FT_Get_Char_Index(face, c);
    if(glyph_index == 0){
      return MAKE_ERROR(Error::kFreeTypeError);
    }

    if(int err = FT_Load_Glyph(face, glyph_index,
        FT_LOAD_RENDER | FT_LOAD_TARGET_MONO)){
      return MAKE_ERROR(Error::kFreeTypeError);
    }
    return MAKE_ERROR(Error::kSuccess);
  }
} //namespace



void WriteAscii(PixelWriter& writer, Vector2D<int> pos, char c,const PixelColor& color){
  const uint8_t* font = GetFont(c);
  for(int dy = 0; dy<16; dy++){
    for(int dx = 0; dx<8 ; dx++){
      if((font[dy] << dx) & 0x80u){
        writer.Write(pos + Vector2D<int>{dx, dy}, color);
      }
    }
  }
}

void WriteString(PixelWriter& writer, Vector2D<int> pos, const char* s, const PixelColor& color){
  // for(int i=0; s[i]!='\0'; i++){
  //   WriteAscii(writer, pos + Vector2D<int>{8 * i, 0}, s[i], color);
  // }
  int pos_x = 0;
  while (*s) {
    const auto [u32, bytes] = ConvertUTF8To32(s);
    WriteUnicode(writer, pos + Vector2D<int>{8 * pos_x, 0}, u32, color);
    s += bytes;
    pos_x += IsHankaku(u32) ? 1 : 2;
  }
}

///1110 xxxx 10xx xxxx 10xx xxxx
///110x xxxx 10xx xxxx
///10xx xxxx
///0xxx xxxx
//U+0800 - U+FFFF  asia
int CountUTF8Size(uint8_t c){
  if (c < 0x80) {
    return 1;
  }else if(0xc0 <= c && c < 0xe0){
    return 2;
  }else if(0xe0 <= c && c < 0xf0){
    return 3;
  }else if(0xf0 <= c && c < 0xf8){
    return 4;
  }
  return 0;
}

std::pair<char32_t, int> ConvertUTF8To32(const char* u8){
switch(CountUTF8Size(u8[0])){
  case 1:
    return {static_cast<char32_t>(u8[0]), 1};
  case 2:
    return {
      (static_cast<char32_t>(u8[0]) & 0b0001'1111) << 6 |
      (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 0,
      2
    };
  case 3:
    return {
      (static_cast<char32_t>(u8[0]) & 0b0000'1111) << 12 |
      (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 6 |
      (static_cast<char32_t>(u8[2]) & 0b0011'1111) << 0,
      3
    };
  case 4:
    return {
      (static_cast<char32_t>(u8[0]) & 0b0000'0111) << 18 |
      (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 12 |
      (static_cast<char32_t>(u8[2]) & 0b0011'1111) << 6 |
      (static_cast<char32_t>(u8[3]) & 0b0011'1111) << 0,
      4
    };
  default:
    return { 0, 0 };
  }
}

bool IsHankaku(char32_t c){
  return c <= 0x7f;
}

WithError<FT_Face> NewFTFace(int size, bool bold){
  FT_Face face;
  if(bold){
    if(int err = FT_New_Memory_Face(ft_library,
        nihongo_bold_buf->data(), nihongo_bold_buf->size(), 0, &face)) {
      return { face, MAKE_ERROR(Error::kFreeTypeError) };
    }
  }else{
    if(int err = FT_New_Memory_Face(ft_library,
        nihongo_buf->data(), nihongo_buf->size(), 0, &face)) {
      return { face, MAKE_ERROR(Error::kFreeTypeError) };
    }
  }

  if(int err = FT_Set_Pixel_Sizes(face, 0, size)){
    return { face, MAKE_ERROR(Error::kFreeTypeError) };
  }

  return { face, MAKE_ERROR(Error::kSuccess) };
}

Error WriteUnicode(PixelWriter& writer, Vector2D<int> pos,
    char32_t c, const PixelColor& color,int size, bool bold){
  if(c <= 0x7f && !bold){
    WriteAscii(writer, pos, c, color);
    return MAKE_ERROR(Error::kSuccess);
  }

  auto [face, err] = NewFTFace(size, bold);
  if(err){
    WriteAscii(writer, pos, '?', color);
    WriteAscii(writer, pos + Vector2D<int>{8, 0}, '?', color);
    return err;
  }

  if(auto err = RenderUnicode(c, face)){
    FT_Done_Face(face);
    WriteAscii(writer, pos, '?', color);
    WriteAscii(writer, pos + Vector2D<int>{8, 0}, '?', color);
    return err;
  }

  FT_Bitmap& bitmap = face->glyph->bitmap;


  int baseline = (face->height + face->descender) *
      face->size->metrics.y_ppem / face->units_per_EM;

  if(bold){
    baseline = (face->ascender) *
        face->size->metrics.y_ppem / face->units_per_EM;
  }


  // char s[200];
  // sprintf(s, "%d , %d , %d , %d , %d | %d , %d", 
  //   (face->ascender) * face->size->metrics.y_ppem / face->units_per_EM,
  //   (face->descender) * face->size->metrics.y_ppem / face->units_per_EM,
  //   (face->height) * face->size->metrics.y_ppem / face->units_per_EM,
  //   face->glyph->bitmap_top,
  //   bitmap.rows,
  //   (face->max_advance_width) * face->size->metrics.y_ppem / face->units_per_EM,
  //   face->glyph->bitmap_left
  // );
  // WriteString(*screen_pixel_writer, {500, 0}, s, {0,0,0});

  const auto glyph_topleft = pos + Vector2D<int>{
      face->glyph->bitmap_left, baseline - face->glyph->bitmap_top};

  for(int dy = 0; dy < bitmap.rows; dy++){
    unsigned char* q = &bitmap.buffer[bitmap.pitch * dy];
    if (bitmap.pitch < 0) {
      q += -bitmap.pitch * bitmap.rows;
    }
    for (int dx = 0; dx < bitmap.width; dx++) {
      const bool b = q[dx >> 3] & (0x80 >> (dx & 0x7));
      if (b) {
        writer.Write(glyph_topleft + Vector2D<int>{dx, dy}, color);
      }
    }
  }

  FT_Done_Face(face);
  return MAKE_ERROR(Error::kSuccess);
}

void InitializeFont(){
    if (int err = FT_Init_FreeType(&ft_library)) {
    exit(1);
  }

  auto [entry, pos_slash] = fat32::FindFile("/nihongor.ttf");
  if (entry == nullptr || pos_slash) {
    exit(1);
  }

  const size_t size = entry->file_size;
  nihongo_buf = new std::vector<uint8_t>(size);
  if (LoadFile(nihongo_buf->data(), size, *entry) != size) {
    delete nihongo_buf;
    exit(1);
  }

  auto [entry2, pos_slash2] = fat32::FindFile("/nihongob.ttf");
  if (entry2 == nullptr || pos_slash2) {
    exit(1);
  }

  const size_t size2 = entry2->file_size;
  nihongo_bold_buf = new std::vector<uint8_t>(size2);
  if (LoadFile(nihongo_bold_buf->data(), size2, *entry2) != size2) {
    delete nihongo_bold_buf;
    exit(1);
  }
}