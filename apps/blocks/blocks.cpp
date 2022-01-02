#include <array>
#include <bitset>
#include <cmath>
#include <cstdlib>

#include "../syscall.h"

using namespace std;

const int kNumBlocksX = 10, kNumBlocksY = 5;
const int kBlockWidth = 20, kBlockHeight = 10;
const int kBarWidth = 60, kBarHeight = 5, kBallRadius = 5;
const int kGapWidth = 30, kGapHeight = 30, kGapBar = 80, kBarFloat = 10;

const int kCanvasWidth = kNumBlocksX * kBlockWidth + 2 * kGapWidth;
const int kCanvasHeight = kGapHeight + kNumBlocksY * kBlockHeight
                          + kGapBar + kBarHeight + kBarFloat;
const int kBarY = kCanvasHeight - kBarFloat - kBarHeight;

//fps
const int kFrameRate = 60; 
//pixels per second
const int kBarSpeed = kCanvasWidth / 2;
const int kBallSpeed = kBarSpeed;

array<bitset<kNumBlocksX>, kNumBlocksY> blocks;

void DrawBlocks(uint64_t layer_id) {
  for (int by = 0; by < kNumBlocksY; ++by) {
    const int y = kWindowTitleHeight+kWindowMargin + kGapHeight + by * kBlockHeight;
    const uint32_t color = 0xff << (by % 3) * 8;

    for (int bx = 0; bx < kNumBlocksX; ++bx) {
      if (blocks[by][bx]) {
        const int x = kWindowMargin + kGapWidth + bx * kBlockWidth;
        const uint32_t c = color | (0xff << ((bx + by) % 3) * 8);
        SyscallWinFillRectangle(layer_id, x, y, kBlockWidth, kBlockHeight, c);
      }
    }
  }
}

void DrawBar(uint64_t layer_id, int bar_x) {
  SyscallWinFillRectangle(layer_id,
                          kWindowMargin + bar_x, kWindowTitleHeight+kWindowMargin + kBarY,
                          kBarWidth, kBarHeight, 0xffffff);
}

void DrawBall(uint64_t layer_id, int x, int y) {
  SyscallWinFillRectangle(layer_id,
                          kWindowMargin + x - kBallRadius, kWindowTitleHeight+kWindowMargin + y - kBallRadius,
                          2 * kBallRadius, 2 * kBallRadius, 0x007f00);
  SyscallWinFillRectangle(layer_id,
                          kWindowMargin + x - kBallRadius/2, kWindowTitleHeight+kWindowMargin + y - kBallRadius/2,
                          kBallRadius, kBallRadius, 0x00ff00);
}

template <class T>
T LimitRange(const T& x, const T& min, const T& max) {
  if (x < min) {
    return min;
  } else if (x > max) {
    return max;
  }
  return x;
}

extern "C" void main(int argc, char** argv) {
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(kCanvasWidth + kWindowMargin*2, kCanvasHeight + kWindowTitleHeight+kWindowMargin*2, 10, 10, "blocks");
  if (err_openwin) {
    exit(err_openwin);
  }

  for (int y = 0; y < kNumBlocksY; ++y) {
    blocks[y].set();
  }

  const int kBallX = kCanvasWidth/2 - kBallRadius - 20;
  const int kBallY = kCanvasHeight - kBarFloat - kBarHeight - kBallRadius - 20;

  int bar_x = kCanvasWidth/2 - kBarWidth/2;
  int ball_x = kBallX, ball_y = kBallY;
  // -1: left, 1: right
  int move_dir = 0; 
  bool left_pressing = 0; 
  bool right_pressing = 0; 
  //degree clockwise start from x
  int ball_dir = 0; 
  int ball_dx = 0, ball_dy = 0;

  bool running = true;
  while(running){

    if(ball_dir!=0){
      //calculate next position
      ball_dx = round(kBallSpeed * cos(M_PI * ball_dir / 180) / kFrameRate);
      ball_dy = round(kBallSpeed * sin(M_PI * ball_dir / 180) / kFrameRate);
      ball_x += ball_dx;
      ball_y += ball_dy;
    }


    //clear canvas
    SyscallWinFillRectangle(layer_id | LAYER_NO_REDRAW,
                            kWindowMargin, kWindowTitleHeight+kWindowMargin, kCanvasWidth, kCanvasHeight, 0);

    DrawBlocks(layer_id | LAYER_NO_REDRAW);
    DrawBar(layer_id | LAYER_NO_REDRAW, bar_x);
    if (ball_y >= 0) {
      DrawBall(layer_id | LAYER_NO_REDRAW, ball_x, ball_y);
    }
    SyscallWinRedraw(layer_id);

    static unsigned long prev_timeout = 0;
    if (prev_timeout == 0) {
      const auto timeout = SyscallCreateTimer(TIMER_ONESHOT_REL, 1, 1000 / kFrameRate);
      prev_timeout = timeout.value;
    } else {
      prev_timeout += 1000 / kFrameRate;
      SyscallCreateTimer(TIMER_ONESHOT_ABS, 1, prev_timeout);
    }

    AppEvent events[1];
    while(running){
      SyscallReadEvent(events, 1);
      if (events[0].type == AppEvent::kTimerTimeout) {
        //draw new frame
        break;
      } else if (events[0].type == AppEvent::kQuit) {
        running = false;
        break;
      } else if (events[0].type == AppEvent::kKeyPush) {
        const auto keycode = events[0].arg.keypush.keycode;
        if (!events[0].arg.keypush.press) { 
          //key release
          if (keycode == 79) {
            //right arrow
            right_pressing = false; 
            move_dir = 1;
          } else if (keycode == 80) {
            left_pressing = false;
            //left arrow
            move_dir = -1;
          }

          if(left_pressing){
            move_dir = -1;
          }else if(right_pressing){
            move_dir = 1;
          }else{
            move_dir = 0;
          }
        } else {
          if (keycode == 79) {
            //right arrow
            right_pressing = true; 
            move_dir = 1;
          } else if (keycode == 80) {
            left_pressing = true;
            //left arrow
            move_dir = -1;
          } else if (keycode == 44) {
            //spacebar
            if (ball_dir == 0 && ball_y < 0) {
              //reset ball
              ball_x = kBallX; ball_y = kBallY;
            } else if (ball_dir == 0) {
              //start game
              ball_dir = 45;
            }
          }
          if (bar_x == 0 && move_dir < 0) {
            move_dir = 0;
          } else if (bar_x + kBarWidth == kCanvasWidth - 1 && move_dir > 0) {
            move_dir = 0;
          }
        }
      }
    }

    if(!running){
      break;
    }

    bar_x += move_dir * kBarSpeed / kFrameRate;
    bar_x = LimitRange(bar_x, 0, kCanvasWidth - kBarWidth - 1);

    if (ball_dir == 0) {
      //game over or not start
      continue;
    }

    int ball_x_nxt = ball_x + ball_dx;
    int ball_y_nxt = ball_y + ball_dy;
    if ((ball_dx < 0 && ball_x_nxt < kBallRadius) ||
        (ball_dx > 0 && kCanvasWidth - kBallRadius <= ball_x_nxt)) {
      //hit left or right
      ball_dir = 180 - ball_dir;
    }
    if (ball_dy < 0 && ball_y_nxt < kBallRadius) {
      //hit top
      ball_dir = -ball_dir;
      continue;
    } else if (bar_x <= ball_x_nxt && ball_x_nxt < bar_x + kBarWidth &&
               ball_dy > 0 && kBarY - kBallRadius <= ball_y_nxt) {
      //hit bar
      ball_dir = -ball_dir;
      continue;
    } else if (ball_dy > 0 && kCanvasHeight - kBallRadius <= ball_y_nxt) {
      //fall down
      ball_dir = 0;
      ball_y = -1;
      continue;
    }


    if (ball_x_nxt < kGapWidth ||
        kCanvasWidth - kGapWidth <= ball_x_nxt ||
        ball_y_nxt < kGapHeight ||
        kGapHeight + kNumBlocksY * kBlockHeight <= ball_y_nxt) {
      //not in black area
      continue;
    }

    const int index_x = (ball_x_nxt - kGapWidth) / kBlockWidth;
    const int index_y = (ball_y_nxt - kGapHeight) / kBlockHeight;
    if (!blocks[index_y].test(index_x)) {
      //no block
      continue;
    }

    //hit block
    blocks[index_y].reset(index_x);

    const int block_left = kGapWidth + index_x * kBlockWidth;
    const int block_right = kGapWidth + (index_x + 1) * kBlockWidth;
    const int block_top = kGapHeight + index_y * kBlockHeight;
    const int block_bottom = kGapHeight + (index_y + 1) * kBlockHeight;
    if ((ball_x < block_left && block_left <= ball_x_nxt) ||
        (block_right < ball_x && ball_x_nxt <= block_right)) {
      ball_dir = 180 - ball_dir;
    }
    if ((ball_y < block_top && block_top <= ball_y_nxt) ||
        (block_bottom < ball_y && ball_y_nxt <= block_bottom)) {
      ball_dir = -ball_dir;
    }
  }

  SyscallCloseWindow(layer_id);
  exit(0);
}