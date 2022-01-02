#pragma once

#include <cstdint>

enum class LayerOperation{
  Move, MoveRelative, Draw, DrawArea
};


struct Message{
  enum Type{
    kEmpty,
    kInterruptXHCI,
    kInterruptLAPICTimer,
    kTimerTimeout,
    kKeyPush,
    kLayerOps,
    kLayerOpsFinish,
    kMouseMove,
    kMouseButton,
    kWindowActive,
    kPipe,
    kWindowClose,
  } type;

  uint64_t src_task_id;

  union {
    struct{
      unsigned long timeout;
      int value;
    } timer;

    struct{
      uint8_t modifier;
      uint8_t keycode;
      char ascii;
      int press;
    } keyboard;

    struct{
      LayerOperation op;
      unsigned int layer_id;
      int x, y;
      int w, h;
    } layer;

    struct{
      int x, y;
      int dx, dy;
      uint8_t buttons;
    } mouse_move;

    struct{
      int x, y;
      // 1: press, 0: release
      int press; 
      int button;
    } mouse_button;

    struct {
      // 1: activate, 0: deactivate
      int activate; 
    } window_active;

    struct{
      char data[16];
      uint8_t len;
    } pipe;
    
    struct {
      unsigned int layer_id;
    } window_close;
  } arg;
};
