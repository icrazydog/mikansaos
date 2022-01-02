#include "frame_buffer.hpp"

#include <cstring>

namespace{
  int BytesPerPixel(PixelFormat format){
    switch (format) {
      case kPixelRGBResv8BitPerColor: return 4;
      case kPixelBGRResv8BitPerColor: return 4;
    }
    return -1;
  }

    Vector2D<int> FrameBufferSize(const FrameBufferConfig& config) {
    return {static_cast<int>(config.horizontal_resolution),
            static_cast<int>(config.vertical_resolution)};
  }

  uint8_t* FrameAddrAt(Vector2D<int> pos, const FrameBufferConfig& config) {
    return config.frame_buffer + BytesPerPixel(config.pixel_format) *
      (config.pixels_per_scan_line * pos.y + pos.x);
  }
}

Error FrameBuffer::Initialize(const FrameBufferConfig& config){
  _config = config;
  const auto bytes_per_pixel = BytesPerPixel(_config.pixel_format);
  if (bytes_per_pixel <= 0) {
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  if (_config.frame_buffer) {
    _buffer.resize(0);
  } else {
    _buffer.resize(
        bytes_per_pixel
        * _config.horizontal_resolution * _config.vertical_resolution);
    _config.frame_buffer = _buffer.data();
    _config.pixels_per_scan_line = _config.horizontal_resolution;
  }

  switch (_config.pixel_format){
    case kPixelRGBResv8BitPerColor:
      _writer = std::make_unique<RGBResv8BitPerColorPixelWriter>(_config);
      break;
    case kPixelBGRResv8BitPerColor:
      _writer = std::make_unique<BGRResv8BitPerColorPixelWriter>(_config);
      break;
    default:
      return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  return MAKE_ERROR(Error::kSuccess);
}

Error FrameBuffer::Copy(Vector2D<int> dst_pos, const FrameBuffer& src, 
    const Rectangle<int>& src_area){
  if (_config.pixel_format != src._config.pixel_format) {
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  const auto bytes_per_pixel = BytesPerPixel(_config.pixel_format);
  if (bytes_per_pixel <= 0) {
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  const Rectangle<int> src_area_shifted{dst_pos, src_area.size};
  const Rectangle<int> src_outline{dst_pos - src_area.pos, FrameBufferSize(src._config)};
  const Rectangle<int> dst_outline{{0, 0}, FrameBufferSize(_config)};
  const auto copy_area = dst_outline & src_outline & src_area_shifted;
  const auto src_start_pos = copy_area.pos - src_outline.pos;

  uint8_t* dst_buf = FrameAddrAt(copy_area.pos, _config);
  const uint8_t* src_buf = FrameAddrAt(src_start_pos, src._config);
  for (int dy = 0; dy < copy_area.size.y; dy++) {
    memcpy(dst_buf, src_buf, bytes_per_pixel * copy_area.size.x);
    dst_buf += bytes_per_pixel * _config.pixels_per_scan_line;
    src_buf += bytes_per_pixel * src._config.pixels_per_scan_line;
  }

  return MAKE_ERROR(Error::kSuccess);
}

void FrameBuffer::Move(Vector2D<int> dst_pos, const Rectangle<int>& src){
  const auto bytes_per_pixel = BytesPerPixel(_config.pixel_format);
  const auto bytes_per_scan_line =  bytes_per_pixel * _config.pixels_per_scan_line;
  
  if (dst_pos.y < src.pos.y) { 
      //move up
    uint8_t* dst_buf = FrameAddrAt(dst_pos, _config);
    const uint8_t* src_buf = FrameAddrAt(src.pos, _config);

    for (int y = 0; y < src.size.y; ++y) {
      memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
      dst_buf += bytes_per_scan_line;
      src_buf += bytes_per_scan_line;
    }
  }else{
    //move down
    uint8_t* dst_buf = FrameAddrAt(dst_pos + Vector2D<int>{0, src.size.y - 1}, _config);
    const uint8_t* src_buf = FrameAddrAt(src.pos + Vector2D<int>{0, src.size.y - 1}, _config);

    for (int y = 0; y < src.size.y; ++y) {
      memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
      dst_buf -= bytes_per_scan_line;
      src_buf -= bytes_per_scan_line;
    }
  }
}



