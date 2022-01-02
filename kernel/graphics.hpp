#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include "frame_buffer_config.hpp"

struct PixelColor{
  uint8_t r, g, b;
};

inline bool operator==(const PixelColor& lhs, const PixelColor& rhs) {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

inline bool operator!=(const PixelColor& lhs, const PixelColor& rhs) {
  return !(lhs == rhs);
}

inline PixelColor operator* (const PixelColor& lhs, const double rhs) {
  PixelColor tmp;
  tmp.r = lhs.r * rhs;
  tmp.g = lhs.g * rhs;
  tmp.b = lhs.b * rhs;
  return tmp;
}

inline PixelColor operator+ (const PixelColor& lhs, const PixelColor& rhs) {
  PixelColor tmp;
  tmp.r = lhs.r + rhs.r;
  tmp.g = lhs.g + rhs.g;
  tmp.b = lhs.b + rhs.b;
  return tmp;
}


constexpr PixelColor ToColor(uint32_t c) {
  return {
    static_cast<uint8_t>((c >> 16) & 0xff),
    static_cast<uint8_t>((c >> 8) & 0xff),
    static_cast<uint8_t>(c & 0xff)
  };
}

/**
 * draw 1 pixel
 * @retval 0 success
 * @retval others fail
 */
/*int WritePixel(const FrameBufferConfig& config,int x, int y, const PixelColor& c){
 const int pixel_position = config.pixels_per_scan_line * y + x;
  if(config.pixel_format == kPixelRGBResv8BitPerColor){
    uint8_t* p = &config.frame_buffer[4 * pixel_position];
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
  }else if(config.pixel_format == kPixelBGRResv8BitPerColor){
    uint8_t* p = &config.frame_buffer[4 * pixel_position];
    p[0] = c.b;
    p[1] = c.g;
    p[2] = c.r;  
  }else{
    return -1;
  }
  return 0;
}
*/

template <typename T>
struct Vector2D{
  T x, y;

  template <typename U>
  Vector2D<T>& operator +=(const Vector2D<U>& rhs){
    x += rhs.x;
    y += rhs.y;
    return *this;
  }

  template <typename U>
  Vector2D<T>& operator -=(const Vector2D<U>& rhs){
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }

  template <typename U>
  Vector2D<T> operator -(const Vector2D<U>& rhs) const {
    auto tmp = *this;
    tmp -= rhs;
    return tmp;
  }
};

template <typename T, typename U>
auto operator +(const Vector2D<T>& lhs, const Vector2D<U>& rhs)
    -> Vector2D<decltype(lhs.x + rhs.x)> {
  return {lhs.x + rhs.x, lhs.y + rhs.y};
}

template <typename T>
Vector2D<T> ElementMax(const Vector2D<T>& lhs, const Vector2D<T>& rhs) {
  return {std::max(lhs.x, rhs.x), std::max(lhs.y, rhs.y)};
}

template <typename T>
Vector2D<T> ElementMin(const Vector2D<T>& lhs, const Vector2D<T>& rhs) {
  return {std::min(lhs.x, rhs.x), std::min(lhs.y, rhs.y)};
}

template <typename T>
struct Rectangle {
  Vector2D<T> pos, size;
};

template<typename T, typename U>
Rectangle<T> operator&(const Rectangle<T>& lhs, const Rectangle<U>& rhs){
  const auto lhs_end_pos = lhs.pos + lhs.size;
  const auto rhs_end_pos = rhs.pos + rhs.size;

  if(lhs_end_pos.x < rhs.pos.x ||
     lhs.pos.x > rhs_end_pos.x ||
     lhs_end_pos.y < rhs.pos.y ||
     lhs.pos.y > rhs_end_pos.y){
    return {{0,0},{0,0}};
  }

  auto new_pos = ElementMax(lhs.pos, rhs.pos);
  auto new_size = ElementMin(lhs_end_pos, rhs_end_pos) - new_pos;
  return {new_pos, new_size};
  
}

class PixelWriter{
  public:
    virtual ~PixelWriter() = default;
    virtual void Write(Vector2D<int> pos, const PixelColor& c) = 0;
    virtual int Width() = 0;
    virtual int Height() = 0;
    virtual PixelColor GetPixel(Vector2D<int> pos) = 0;
};

class FrameBufferWriter : public PixelWriter{
  public:
    FrameBufferWriter(const FrameBufferConfig& config) : _config{config}{}
    virtual ~FrameBufferWriter() = default;

    virtual int Width() override { return _config.horizontal_resolution; }
    virtual int Height() override { return _config.vertical_resolution; }
  protected:
    uint8_t* PixelAt(Vector2D<int> pos){
      return _config.frame_buffer + 4 * (_config.pixels_per_scan_line * pos.y + pos.x);
    }

  private:
    const FrameBufferConfig& _config;
};


class RGBResv8BitPerColorPixelWriter : public FrameBufferWriter {
  public:
    using FrameBufferWriter::FrameBufferWriter;

    virtual void Write(Vector2D<int> pos, const PixelColor& c) override;
    virtual PixelColor GetPixel(Vector2D<int> pos) override;
};


class BGRResv8BitPerColorPixelWriter : public FrameBufferWriter {
  public:
    using FrameBufferWriter::FrameBufferWriter;

    virtual void Write(Vector2D<int> pos, const PixelColor& c) override;
    virtual PixelColor GetPixel(Vector2D<int> pos) override;
};



void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c);

void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c);

void DrawCircle(PixelWriter& writer, const Vector2D<int>& pos,
                   int radius,int lineWidth, const PixelColor& c);

void FillCircle(PixelWriter& writer, const Vector2D<int>& pos,
                   const int radius, const PixelColor& c);

void FillPolygon(PixelWriter& writer, const std::vector<Vector2D<int>>& vertexs, const PixelColor& c);

const PixelColor kDesktopBGColor{0, 162, 232};
const PixelColor kDesktopFGColor{255, 255, 255};

const PixelColor kTaskBarColor{255, 255, 255};
const PixelColor kMainLightColor{255, 173, 119};
const PixelColor kMainLight2Color{255, 140, 64};
const PixelColor kMainTextColor{143, 143, 182};

const PixelColor kWindowTitleColor{255, 255, 255};
const PixelColor kWindowTitleBlurColor{195, 195, 195};
const PixelColor kWindowCloseColor{237, 28, 36};
const PixelColor kWindowTitleTextColor{255, 255, 255};
const PixelColor kWindowBGColor{229, 229, 229};
const PixelColor kWindowFGColor{0, 0, 0};

const PixelColor kTextBoxBGColor{255, 255, 255};
const PixelColor kTextBoxBorderTopColor = ToColor(0x848484);
const PixelColor kTextBoxBorderLeftColor = ToColor(0x848484);
const PixelColor kTextBoxBorderBottomColor = ToColor(0xc6c6c6);
const PixelColor kTextBoxBorderRightColor = ToColor(0xc6c6c6);
const PixelColor kTextBoxTextColor{0, 0, 0};

const PixelColor kTerminalBGColor{0, 0, 0};
const PixelColor kTerminalFGColor = ToColor(0x67be67);

const int kTaskBarHeight = 35; 
const int kInfoBallRadius = 35;

void DrawDesktop(PixelWriter& writer);
void DrawTaskBar(PixelWriter& pixel_writer);
void DrawDesktopButton(PixelWriter& pixel_writer);

extern FrameBufferConfig screen_frame_buffer_config;
extern PixelWriter* screen_pixel_writer;
Vector2D<int> ScreenSize();

void InitializeGraphics(const FrameBufferConfig& screen_config);
