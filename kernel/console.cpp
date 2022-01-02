/**
 * @file console.cpp
 *
 * draw console utils
 */

#include "console.hpp"

#include <cstring>
#include "font.hpp"
#include "layer.hpp"
#include "task.hpp"

Console::Console(const PixelColor& fg_color, const PixelColor& bg_color)
  : _writer{nullptr}, _window{}, _fg_color{fg_color}, _bg_color{bg_color},
    _buffer{}, _cursor_row{0}, _cursor_column{0},  _layer_id{0}{
}

void Console::PutString(const char* s){
  while(*s){
    if(*s == '\n'){
      NewLine();
    }else if(_cursor_column < kColumns -1){
      WriteAscii(*_writer, Vector2D<int>{8 * _cursor_column, 16 * _cursor_row}, *s, _fg_color);
      _buffer[_cursor_row][_cursor_column] = *s;
      _cursor_column++;
    }
    s++;
  }


  if(layer_manager){
    // if(task_manager->CurrentTask().ID()!=1){
    //   Message msg = MakeLayerMessage(
    //           1, _layer_id,
    //           LayerOperation::Draw,{});  
    //   __asm__("cli");
    //   task_manager->SendMessage(1, msg);
    //   __asm__("sti");
    // }else{
      layer_manager->Draw(_layer_id);
    // }
  }
}

void Console::SetWriter(PixelWriter* writer){
  if(_writer == writer){
    return;
  }
  _writer = writer;
  _window.reset();
  Refresh();
}

void Console::SetWindow(const std::shared_ptr<Window>& window){
  if(_window == window ){
    return;
  }
  _window = window;
  _writer = window->Writer();
  Refresh();
}

void Console::SetLayerID(unsigned int layer_id) {
  _layer_id = layer_id;
}

unsigned int Console::LayerID() const {
  return _layer_id;
}


void Console::NewLine(){
  _cursor_column = 0;
  if(_cursor_row < kRows - 1){
    _cursor_row++;
  }else{

    if(_window){
      Rectangle<int> move_src{{0, 16}, {8 * kColumns, 16 * (kRows - 1)}};
      _window->Move({0, 0}, move_src);
      FillRectangle(*_writer, {0, 16 *(kRows -1)},{8 * kColumns, 16}, _bg_color);
    }else{
      FillRectangle(*_writer, {0, 0},{8 * kColumns, 16 * kRows}, _bg_color);
    }

    for(int row = 0; row < kRows - 1; row++){
      memcpy(_buffer[row], _buffer[row+1], kColumns + 1);
      WriteString(*_writer,Vector2D<int>{0 ,16 * row}, _buffer[row], _fg_color);
    }
    memset(_buffer[kRows - 1], 0, kColumns + 1);

  }
}

void Console::Refresh() {
  FillRectangle(*_writer, {0, 0}, {8 * kColumns, 16 * kRows}, _bg_color);
  for (int row = 0; row < kRows; ++row) {
    WriteString(*_writer, Vector2D<int>{0, 16 * row}, _buffer[row], _fg_color);
  }
}

namespace {
  char console_buf[sizeof(Console)];
}
Console* console;

void InitializeConsole(){
  console = new(console_buf) Console{kDesktopFGColor, kDesktopBGColor};
  console->SetWriter(screen_pixel_writer);

    /*
  for(int x=0; x<frame_buffer_config.horizontal_resolution; x++){
    for(int y=0; y<frame_buffer_config.vertical_resolution; y++){
      pixel_writer->Write(x, y,{255, 255, 255});
    }
  }
  */

  /*
  for(int x=0; x<200; x++){
    for(int y=0; y<100; y++){
      pixel_writer->Write(x, y,{102, 255, 255});
    }
  }  }  SetLogLevel(kWarn);, 0, 66,"Hello, world!!",{0, 0, 255});

  char buf[128];
  sprintf(buf, "1+2 = %d", 1+2);
  WriteString(*pixel_writer, 0, 82, buf, {0, 0, 0});
  */

   /*
  for(int i = 0; i < 27; i++){
    printk("printk: %d\n", i);
  }
  */
}