/**
 * @file mouse.cpp
 */

#include "mouse.hpp"

#include <memory>

#include "timer.hpp"
#include "logger.hpp"
#include "usb/classdriver/mouse.hpp"
#include "task.hpp"

namespace{
  const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1]  {
    "@              ",
    "@@             ",
    "@.@            ",
    "@..@           ",
    "@...@          ",
    "@....@         ",
    "@.....@        ",
    "@......@       ",
    "@.......@      ",
    "@........@     ",
    "@.........@    ",
    "@..........@   ",
    "@...........@  ",
    "@............@ ",
    "@......@@@@@@@@",
    "@......@       ",
    "@....@@.@      ",
    "@...@ @.@      ",
    "@..@   @.@     ",
    "@.@    @.@     ",
    "@@      @.@    ",
    "@       @.@    ",
    "         @.@   ",
    "         @@@   ",
  };


  std::tuple<Layer*, uint64_t> FindActiveLayerTask() {
    const auto act = active_layer->GetActive();
    if (!act) {
      return { nullptr, 0 };
    }
    const auto layer = layer_manager->FindLayer(act);
    if (!layer) {
      return { nullptr, 0 };
    }

    const auto task_it = layer_task_map->find(act);
    if (task_it == layer_task_map->end()) {
      return { layer, 0 };
    }

    return { layer, task_it->second };
  }

  void SendMouseMessage(Vector2D<int> newpos, Vector2D<int> posdiff,
                        uint8_t buttons, uint8_t previous_buttons) {
    
    const auto [ layer, task_id ] = FindActiveLayerTask();
    if (!layer || !task_id) {
      return;
    }

    const auto relpos = newpos - layer->GetPosition();
    if (posdiff.x != 0 || posdiff.y != 0) {
      Message msg{Message::kMouseMove};
      msg.arg.mouse_move.x = relpos.x;
      msg.arg.mouse_move.y = relpos.y;
      msg.arg.mouse_move.dx = posdiff.x;
      msg.arg.mouse_move.dy = posdiff.y;
      msg.arg.mouse_move.buttons = buttons;
      task_manager->SendMessage(task_id, msg);
    }

    if (previous_buttons != buttons) {
      const auto diff = previous_buttons ^ buttons;
      for (int i = 0; i < 8; i++) {
        if ((diff >> i) & 1) {
          Message msg{Message::kMouseButton};
          msg.arg.mouse_button.x = relpos.x;
          msg.arg.mouse_button.y = relpos.y;
          msg.arg.mouse_button.press = (buttons >> i) & 1;
          msg.arg.mouse_button.button = i;
          task_manager->SendMessage(task_id, msg);
        }
      }
    }
  }

  void SendCloseMessage() {
    const auto [layer, task_id] = FindActiveLayerTask();
    if (!layer || !task_id) {
      return;
    }

    Message msg{Message::kWindowClose};
    msg.arg.window_close.layer_id = layer->ID();
    task_manager->SendMessage(task_id, msg);
  }
} //namespace
  
void DrawMouseCursor(PixelWriter * pixel_writer, Vector2D<int> position){
  //draw mouse cursor
  for(int dy = 0; dy< kMouseCursorHeight; dy++){
    for(int dx = 0; dx < kMouseCursorWidth; dx++){
      if(mouse_cursor_shape[dy][dx] == '@'){
        pixel_writer->Write(Vector2D<int>{position.x + dx, position.y + dy}, {0, 0, 0});
      }else if(mouse_cursor_shape[dy][dx] == '.'){
        pixel_writer->Write(Vector2D<int>{position.x + dx, position.y + dy}, {255, 255, 255});
      } else {
        pixel_writer->Write(Vector2D<int>{position.x + dx, position.y + dy}, kMouseTransparentColor);
      }
    }
  }
}



MouseCursor::MouseCursor(unsigned int layer_id) : _layer_id{layer_id} {
}

void MouseCursor::SetPosition(Vector2D<int> position) {
  _position = position;
  layer_manager->Move(_layer_id, _position);
}


void MouseCursor::OnInterrupt(uint8_t buttons, int8_t displacement_x, int8_t displacement_y){

  if(layer_manager){

    const auto oldpos = _position;
    auto newpos = _position + Vector2D<int>{displacement_x , displacement_y};
    newpos = ElementMin(newpos, ScreenSize() +  Vector2D<int>{-1, -1});
    _position = ElementMax(newpos,  Vector2D<int>{0 , 0});
    
    const auto posdiff = _position - oldpos;

    if(IsLAPICTimerInitialized()){
      StartLAPICTimer();
      layer_manager->Move(_layer_id, _position);
      // layer_manager->Draw();
      auto elapsed = LAPICTimerElapsed();
      StopLAPICTimer();
      Log(kDebug, "MouseObserver: elapsed = %u\n", elapsed);
    }else{
      layer_manager->Move(_layer_id, _position);
    }

    unsigned int close_layer_id = 0;

    const bool previous_left_pressed = (_previous_buttons & 0x01);
    const bool left_pressed = (buttons & 0x01);
    if (!previous_left_pressed && left_pressed) {
      //left button down
      auto layer = layer_manager->FindLayerByPosition(_position, _layer_id);
      if (layer  && layer->IsDraggable()) {
        const auto pos_layer = _position - layer->GetPosition();
        switch(layer->GetWindow()->GetWindowRegion(pos_layer)){
          case WindowRegion::kTitleBar:
          _drag_layer_id = layer->ID();
          break;
        case WindowRegion::kCloseButton:
          close_layer_id = layer->ID();
          break;
        default:
          break;
        }
        active_layer->Activate(layer->ID());
      }else{
        active_layer->Activate(0);
      }
      _previous_mouse_down_layer = layer;
    }else if(previous_left_pressed && left_pressed) {
      //on drag
      if (_drag_layer_id > 0) {
        layer_manager->MoveRelative(_drag_layer_id, posdiff);
      }
    }else if(previous_left_pressed && !left_pressed) {
      //left button up
      if(_drag_layer_id!=0){
        //drag end
        _drag_layer_id = 0;
      }else{
        auto layer = layer_manager->FindLayerByPosition(_position, _layer_id);
        if(layer!=nullptr && _previous_mouse_down_layer == layer)
        layer->OnClick(1);
      }
      _previous_mouse_down_layer = nullptr;
    }

    if (_drag_layer_id == 0) {
      if (close_layer_id == 0) {
        SendMouseMessage(newpos, posdiff, buttons, _previous_buttons);
      }else{
        SendCloseMessage();
      }
    }

    _previous_buttons = buttons;
  }
}


void InitializeMouse(){
  auto mouse_window = std::make_shared<Window>(
    kMouseCursorWidth, kMouseCursorHeight, screen_frame_buffer_config.pixel_format);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  DrawMouseCursor(mouse_window->Writer(), {0, 0});
  const auto screen_size = ScreenSize();
  Vector2D<int> mouse_position = {screen_size.x - screen_size.x/3, screen_size.y/2};



  auto mouse_layer_id = layer_manager->NewLayer()
      .SetWindow(mouse_window)
      .Move(mouse_position)
      .ID();
  
  auto mouse = std::make_shared<MouseCursor>(mouse_layer_id);
  mouse->SetPosition(mouse_position);
  layer_manager->SetIndex(mouse_layer_id,  std::numeric_limits<int>::max());


  usb::HIDMouseDriver::default_observer = 
      [mouse](uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
        mouse->OnInterrupt(buttons, displacement_x, displacement_y);
      };

  active_layer->SetMouseLayer(mouse_layer_id);
}