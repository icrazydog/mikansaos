#include "window.hpp"

#include "logger.hpp"
#include "font.hpp"


namespace {

  const int kCloseButtonWidth = 16;
  const int kCloseButtonHeight = 14;
  // const char close_button[kCloseButtonHeight][kCloseButtonWidth + 1] = {
  //   "...............@",
  //   ".:::::::::::::$@",
  //   ".:::::::::::::$@",
  //   ".:::@@::::@@::$@",
  //   ".::::@@::@@:::$@",
  //   ".:::::@@@@::::$@",
  //   ".::::::@@:::::$@",
  //   ".:::::@@@@::::$@",
  //   ".::::@@::@@:::$@",
  //   ".:::@@::::@@::$@",
  //   ".:::::::::::::$@",
  //   ".:::::::::::::$@",
  //   ".$$$$$$$$$$$$$$@",
  //   "@@@@@@@@@@@@@@@@",
  // };
    const char close_button[kCloseButtonHeight][kCloseButtonWidth + 1] = {
    "................",
    ".@@@........@@@.",
    "..@@@......@@@..",
    "...@@@....@@@...",
    "....@@@..@@@....",
    ".....@@@@@@.....",
    "......@@@@......",
    ".....@@@@@@.....",
    "....@@@..@@@....",
    "...@@@....@@@...",
    "..@@@......@@@..",
    ".@@@........@@@.",
    "................",
    "................",
  };

  void DrawTextbox(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size,
                   const PixelColor& background,
                   const PixelColor& borderTop,
                   const PixelColor& borderRight,
                   const PixelColor& borderBottom,
                   const PixelColor& borderLeft){

    auto fill_rect =
    [&writer](Vector2D<int> pos, Vector2D<int> size, PixelColor c) {
      FillRectangle(writer, pos, size, c);
    };

  // fill main box
  fill_rect(pos + Vector2D<int>{1, 1}, size - Vector2D<int>{2, 2}, background);

  // draw border lines
  fill_rect(pos,                            {size.x, 1}, borderTop );
  fill_rect(pos,                            {1, size.y}, borderLeft);
  fill_rect(pos + Vector2D<int>{0, size.y}, {size.x, 1}, borderBottom);
  fill_rect(pos + Vector2D<int>{size.x, 0}, {1, size.y}, borderRight);
  }


}

uint8_t globalTransparent = 0xff;

Window::Window(int width, int height, PixelFormat shadow_format) 
    : _width{width}, _height{height}{
  _data.resize(height);
  for(int y=0; y<height; y++){
    _data[y].resize(width);
  }

  FrameBufferConfig config{};
  config.frame_buffer = nullptr;
  config.horizontal_resolution = width;
  config.vertical_resolution = height;
  config.pixel_format = shadow_format;

  if (auto err = _shadow_buffer.Initialize(config)) {
    Log(kError, err, "failed to initialize shadow buffer: %s at %s:%d\n",
        err.Name());
  }
}

void Window::DrawTo(FrameBuffer& dst, Vector2D<int> pos, const Rectangle<int>& area, bool transparent){
  Rectangle<int> window_area{pos, Size()};
  Rectangle<int> intersection_area = window_area & area;

  if(intersection_area.size.x==0||intersection_area.size.y==0){
    return;
  }
  
  if(!_transparent_color && !transparent){
    dst.Copy(intersection_area.pos, _shadow_buffer,
        {intersection_area.pos - pos, intersection_area.size});
  }else{
    const PixelColor* tc = nullptr;
    if(_transparent_color){
      tc = &_transparent_color.value();
    }
    auto& writer = dst.Writer();
    auto alphablend = [](const PixelColor& fgColor,const PixelColor& bgColor,uint8_t alpha){
      return fgColor * (alpha*1.0/255) + bgColor*(1- alpha*1.0/255);
    };

    auto delta_pos =  intersection_area.pos - pos;
    for(int y = std::max(0,0-intersection_area.pos.y); 
        y < std::min(intersection_area.size.y,writer.Height() - intersection_area.pos.y); y++){
      for(int x = std::max(0,0-intersection_area.pos.x); 
          x < std::min(intersection_area.size.x, writer.Width() - intersection_area.pos.x); x++){

        const auto c = At(Vector2D<int>{x + delta_pos.x,y + delta_pos.y});
        if(tc==nullptr || c != *tc){
          auto pixelColor = c;
          if(transparent){
            auto bgc = writer.GetPixel(intersection_area.pos + Vector2D<int>{x, y});
            pixelColor = alphablend(c,bgc,globalTransparent);
          }
          writer.Write(intersection_area.pos + Vector2D<int>{x, y}, pixelColor);
        }
      }
    }
  }
}

void Window::SetTransparentColor(std::optional<PixelColor> c){
  _transparent_color = c;
}


 Window::WindowWriter* Window::Writer(){
  return &_writer;
}

const PixelColor& Window::At(Vector2D<int> pos) const{
  return _data[pos.y][pos.x];
}

void Window::Write(Vector2D<int> pos, PixelColor c){
  _data[pos.y][pos.x] = c;
  _shadow_buffer.Writer().Write(pos, c);
}

int Window::Width() const{
  return _width;
}
int Window::Height() const{
  return _height;
}

Vector2D<int> Window::Size() const{
  return {_width, _height};
}

void Window::Move(Vector2D<int> dst_pos, const Rectangle<int>& src){
  _shadow_buffer.Move(dst_pos, src);
}

WindowRegion Window::GetWindowRegion(Vector2D<int> pos) {
  return WindowRegion::kOther;
}

ToplevelWindow::ToplevelWindow(int width, int height, PixelFormat shadow_format,
                const std::string& title)
    : Window{width, height, shadow_format}, _title{title}{
  DrawWindow(*Writer(), _title.c_str());
}

void ToplevelWindow::Activate() {
  Window::Activate();
  DrawWindowTitle(*Writer(), _title.c_str(), true);
}
void ToplevelWindow::Deactivate() {
  Window::Deactivate();
  DrawWindowTitle(*Writer(), _title.c_str(), false);

}

WindowRegion ToplevelWindow::GetWindowRegion(Vector2D<int> pos) {
  if(pos.x < 2 || Width() - 2 <= pos.x ||
      pos.y < 2 || Height() - 2 <= pos.y){
    return WindowRegion::kBorder;
  }else if (pos.y < kTopLeftMargin.y){
    if(Width() - 5 - kCloseButtonWidth <= pos.x && pos.x < Width() - 5 &&
        5 <= pos.y && pos.y < 5 + kCloseButtonHeight){
      return WindowRegion::kCloseButton;
    }
    return WindowRegion::kTitleBar;
  }
  return WindowRegion::kOther;
}

Vector2D<int> ToplevelWindow::InnerSize() const{
  return Size() - kTopLeftMargin - kBottomRightMargin;
}

void DrawWindow(PixelWriter& writer, const char* title) {
  //constant expression
  // auto fill_rect = [&writer](Vector2D<int> pos, Vector2D<int> size, uint32_t c) {
  //   FillRectangle(writer, pos, size, ToColor(c));
  // };
  // fill_rect({0, 0},         {win_w, 1},             0xc6c6c6);
  // fill_rect({1, 1},         {win_w - 2, 1},         0xffffff);
  // fill_rect({0, 0},         {1, win_h},             0xc6c6c6);
  // fill_rect({1, 1},         {1, win_h - 2},         0xffffff);
  // fill_rect({win_w - 2, 1}, {1, win_h - 2},         0x848484);
  // fill_rect({win_w - 1, 0}, {1, win_h},             0x000000);
  // fill_rect({2, 2},         {win_w - 4, win_h - 4}, 0xc6c6c6);
  // fill_rect({3, 3},         {win_w - 6, 18},        0x000084);
  // fill_rect({1, win_h - 2}, {win_w - 2, 1},         0x848484);
  // fill_rect({0, win_h - 1}, {win_w, 1},             0x000000);
  


  auto fill_rect = [&writer](Vector2D<int> pos, Vector2D<int> size, PixelColor c) {
    FillRectangle(writer, pos, size, c);
  };
  const auto win_w = writer.Width();
  const auto win_h = writer.Height();

  fill_rect({0, 25},   {win_w, win_h-25}, kWindowBGColor);

  DrawWindowTitle(writer, title, false);
}


void DrawTextbox(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size) {
  DrawTextbox(writer, pos, size, kTextBoxBGColor, 
      kTextBoxBorderTopColor,
      kTextBoxBorderRightColor,
      kTextBoxBorderBottomColor,
      kTextBoxBorderLeftColor);
}

void DrawTerminal(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size){
  DrawTextbox(writer, pos, size, 
      kTerminalBGColor, 
      kTextBoxBorderTopColor,
      kTextBoxBorderRightColor,
      kTextBoxBorderBottomColor,
      kTextBoxBorderLeftColor);
}

void DrawWindowTitle(PixelWriter& writer, const char* title, bool active){
  const auto win_w = writer.Width();
  auto fill_rect = [&writer](Vector2D<int> pos, Vector2D<int> size, PixelColor c) {
    FillRectangle(writer, pos, size, c);
  };

  PixelColor bgcolor;
  if (active) {
    bgcolor = kWindowTitleColor;
  }else{
    bgcolor = kWindowTitleBlurColor;
  }

  fill_rect({0, 0},   {win_w, 25}, bgcolor);


  WriteString(writer, {18, 4}, title, kWindowFGColor);

  for (int y = 0; y < kCloseButtonHeight; ++y) {
    for (int x = 0; x < kCloseButtonWidth; ++x) {
      if (close_button[y][x] == '@') {
        writer.Write({win_w - 5 - kCloseButtonWidth + x, 5 + y}, kWindowCloseColor);
      }
      
    }
  }
}