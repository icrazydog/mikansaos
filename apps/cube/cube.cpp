#include <cstdlib>
#include <array>
#include <cmath>
#include <algorithm>

#include "../syscall.h"

using namespace std;

template <class T>
struct Vector3D {
  T x, y, z;
};

template <class T>
struct Vector2D {
  T x, y;
};

void DrawObj(uint64_t layer_id);
void DrawSurface(uint64_t layer_id, int sur);
bool Sleep(unsigned long ms);

const int kScale = 100, kMargin = 10;
const int kCanvasSize = 2 * kScale + kMargin;
const array<Vector3D<int>, 8> kCube{{
  { 1,  1,  1}, { 1,  1, -1}, { 1, -1,  1}, { 1, -1, -1},
  {-1,  1,  1}, {-1,  1, -1}, {-1, -1,  1}, {-1, -1, -1}
}};
const array<array<int, 4>, 6> kSurface{{
 {0,4,6,2}, {1,3,7,5}, {0,2,3,1}, {0,1,5,4}, {4,5,7,6}, {6,7,3,2}
}};
const array<uint32_t, kSurface.size()> kColor{
  0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff
};

array<Vector3D<double>, kCube.size()> vert;
array<double, kSurface.size()> centerz4;
array<Vector2D<int>, kCube.size()> scr;



extern "C" void main(int argc, char** argv) {
  auto [layer_id, err_openwin]
      = SyscallOpenWindow(kCanvasSize + kWindowMargin*2, kCanvasSize + kWindowTitleHeight + kWindowMargin*2, 10, 10, "cube");
  if(err_openwin){
    exit(err_openwin);
  }

  //degree
  int thx = 0, thy = 0, thz = 0;
  const double to_rad = 3.14159265358979323 / 0x8000;
  while (true) {
    //rotate axis
    thx = (thx + 182) & 0xffff;
    thy = (thy + 273) & 0xffff;
    thz = (thz + 364) & 0xffff;
    const double xp = cos(thx * to_rad), xa = sin(thx * to_rad);
    const double yp = cos(thy * to_rad), ya = sin(thy * to_rad);
    const double zp = cos(thz * to_rad), za = sin(thz * to_rad);
    for (int i = 0; i < kCube.size(); i++) {
      const auto cv = kCube[i];
      //sin(a+b) = sin(a)cos(b)+cos(a)sin(b)
      //cos(a+b) = cos(a)cos(b)-sin(a)sin(b)
      //rotate x
      const double zt = kScale*cv.z * xp + kScale*cv.y * xa; 
      const double yt = kScale*cv.y * xp - kScale*cv.z * xa;
      //rotate y
      const double xt = kScale*cv.x * yp + zt          * ya;
      vert[i].z       = zt          * yp - kScale*cv.x * ya;
      //rotate z
      vert[i].x       = xt          * zp - yt          * za;
      vert[i].y       = yt          * zp + xt          * za;
    }

    //4*z of center coordinate of surface
    for (int sur = 0; sur < kSurface.size(); sur++) {
      centerz4[sur] = 0;
      for (int i = 0; i < kSurface[sur].size(); i++) {
        centerz4[sur] += vert[kSurface[sur][i]].z;
      }
    }

    //clear screen
    SyscallWinFillRectangle(layer_id | LAYER_NO_REDRAW,
                            kWindowMargin, kWindowTitleHeight+kWindowMargin, kCanvasSize, kCanvasSize, 0);
    DrawObj(layer_id | LAYER_NO_REDRAW);
    SyscallWinRedraw(layer_id);
    if (Sleep(50)) {
      break;
    }
  }

  SyscallCloseWindow(layer_id);
  exit(0);
}


void DrawObj(uint64_t layer_id) {
  //perspective projection vert to scr
  for (int i = 0; i < kCube.size(); i++) {
    const double t = kScale / (vert[i].z + 3*kScale);
    scr[i].x = (vert[i].x * t) + kCanvasSize/2;
    scr[i].y = (vert[i].y * t) + kCanvasSize/2;
  }

  while(true){
    //draw from inside
    double* const zmax = max_element(centerz4.begin(), centerz4.end());
    if (*zmax == numeric_limits<double>::lowest()) {
      break;
    }
    const int sur = zmax - centerz4.begin();
    centerz4[sur] = numeric_limits<double>::lowest();

    //draw surface which normal vector to scr
    const auto v0 = vert[kSurface[sur][0]],
               v1 = vert[kSurface[sur][1]],
               v2 = vert[kSurface[sur][2]];
    // v0 -> v1
    const auto e0x = v1.x - v0.x, e0y = v1.y - v0.y; 
    // v1 -> v2
    const auto e1x = v2.x - v1.x, e1y = v2.y - v1.y;
    //cross product 
    int surface_normal_vecor_z = e0x * e1y - e0y * e1x;
    if (surface_normal_vecor_z <= 0) {
      DrawSurface(layer_id, sur);
    }
  }
}

void DrawSurface(uint64_t layer_id, int sur) {
  const auto& surface = kSurface[sur]; 
  int ymin = kCanvasSize, ymax = 0;
  int y2x_down_side[kCanvasSize], y2x_up_side[kCanvasSize]; 
  for (int i = 0; i < surface.size(); i++) {
    const auto p0 = scr[surface[i]], p1 = scr[surface[(i+1)%4]];
    ymin = min(ymin, p0.y);
    ymax = max(ymax, p0.y);
    if (p0.y == p1.y) {
      continue;
    }

    //make y0 < y1
    int* y2x;
    int x0, y0, y1, dx;
    if (p0.y < p1.y) {
      y2x = y2x_down_side;
      x0 = p0.x; y0 = p0.y; y1 = p1.y; dx = p1.x - p0.x;
    } else {
      y2x = y2x_up_side;
      x0 = p1.x; y0 = p1.y; y1 = p0.y; dx = p0.x - p1.x;
    }

    const double m = static_cast<double>(dx) / (y1 - y0);
    const auto roundish = dx >= 0 ? static_cast<double(*)(double)>(floor)
                                  : static_cast<double(*)(double)>(ceil);
    for (int y = y0; y <= y1; y++) {
      y2x[y] = roundish(m * (y - y0) + x0);
    }
  }

  for (int y = ymin; y <= ymax; y++) {
    int p0x = min(y2x_up_side[y], y2x_down_side[y]);
    int p1x = max(y2x_up_side[y], y2x_down_side[y]);
    SyscallWinFillRectangle(layer_id, kWindowMargin + p0x, kWindowTitleHeight + kWindowMargin + y, p1x - p0x + 1, 1, kColor[sur]);
  }
}

bool Sleep(unsigned long ms) {
  static unsigned long prev_timeout = 0;
  if (prev_timeout == 0) {
    const auto timeout = SyscallCreateTimer(TIMER_ONESHOT_REL, 1, ms);
    prev_timeout = timeout.value;
  } else {
    prev_timeout += ms;
    SyscallCreateTimer(TIMER_ONESHOT_ABS, 1, prev_timeout);
  }

  AppEvent events[1];
  for (;;) {
    SyscallReadEvent(events, 1);
    if (events[0].type == AppEvent::kTimerTimeout) {
      return false;
    } else if (events[0].type == AppEvent::kQuit) {
      return true;
    }
  }
}
