#pragma once

#include <vector>
#include <memory>

#include "error.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"

class FrameBuffer {
 public:
  Error Initialize(const FrameBufferConfig& config);
  Error Copy(Vector2D<int> dst_pos, const FrameBuffer& src, const Rectangle<int>& src_area);
  void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

  FrameBufferWriter& Writer() { return *_writer; }
  const FrameBufferConfig& Config() const { return _config; }

 private:
  FrameBufferConfig _config{};
  std::vector<uint8_t> _buffer{};
  std::unique_ptr<FrameBufferWriter> _writer{};
};
