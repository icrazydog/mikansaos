#pragma once

#include "graphics.hpp"
#include "layer.hpp"

const int kMouseCursorWidth = 15;
const int kMouseCursorHeight = 24;
const PixelColor kMouseTransparentColor{0, 0, 1};

void DrawMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position);

class MouseCursor{
  public:
    MouseCursor(unsigned int layer_id);

    void OnInterrupt(uint8_t buttons, int8_t displacement_x, int8_t displacement_y);

    unsigned int LayerID() const { return _layer_id; }
    void SetPosition(Vector2D<int> position);
    Vector2D<int> Position() const { return _position; }
  private:
  unsigned int _layer_id;
  Vector2D<int> _position{};

  unsigned int _drag_layer_id{0};
  uint8_t _previous_buttons{0};
  Layer* _previous_mouse_down_layer = nullptr;
};

void InitializeMouse();
