#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../syscall.h"

static const int kWidth = 200, kHeight = 130;

bool IsInside(int x, int y) {
  return kWindowMargin <= x && x < kWindowMargin + kWidth && kWindowTitleHeight + kWindowMargin <= y && y < kWindowTitleHeight + kWindowMargin + kHeight;
}

extern "C" void main(int argc, char** argv) {
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(kWidth + kWindowMargin*2, kHeight + kWindowTitleHeight + kWindowMargin*2, 10, 10, "paint");
  if (err_openwin) {
    exit(err_openwin);
  }

  AppEvent events[1];
  bool press = false;
  while (true) {
    auto [ n, err ] = SyscallReadEvent(events, 1);
    if (err) {
      printf("ReadEvent failed: %s\n", strerror(err));
      break;
    }
    if (events[0].type == AppEvent::kQuit) {
      break;
    } else if (events[0].type == AppEvent::kMouseMove) {
      auto& arg = events[0].arg.mouse_move;
      const auto prev_x = arg.x - arg.dx, prev_y = arg.y - arg.dy;
      if (press && IsInside(prev_x, prev_y) && IsInside(arg.x, arg.y)) {
        SyscallWinDrawLine(layer_id, prev_x, prev_y, arg.x, arg.y, 0x000000);
      }
    } else if (events[0].type == AppEvent::kMouseButton) {
      auto& arg = events[0].arg.mouse_button;
      if (arg.button == 0) {
        press = arg.press;
        if(press && IsInside(arg.x, arg.y)){
          SyscallWinFillRectangle(layer_id, arg.x, arg.y, 1, 1, 0x000000);
        }
      }
    } else {
      printf("unknown event: type = %d\n", events[0].type);
    }
  }

  SyscallCloseWindow(layer_id);
  exit(0);
}
