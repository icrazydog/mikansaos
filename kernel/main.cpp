/**
 * @file main.cpp
 *
 * kernel
 */

#include <cstdint>
#include <cstddef>
#include <deque>
#include <limits>

#include "frame_buffer_config.hpp"
#include "logger.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "mouse.hpp"
#include "pci.hpp"
#include "usb/xhci/xhci.hpp"
#include "interrupt.hpp"
#include "asmfunc.h"
#include "memory_map.hpp"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "window.hpp"
#include "layer.hpp"
#include "timer.hpp"
#include "message.hpp"
#include "acpi.hpp"
#include "keyboard.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "fat.hpp"
#include "syscall.hpp"
#include "uefi.h"



/*
//defined in <array>
void* operator new(size_t size, void* buf) noexcept{
  return buf;
}
*/

// void operator delete(void* obj) noexcept{
// }


int printk(const char* format, ...){
  if(console==nullptr){
    return 1;
  }

  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  if(IsLAPICTimerInitialized()){
    StartLAPICTimer();
    console->PutString(s);
    auto elapsed = LAPICTimerElapsed();
    StopLAPICTimer();

    sprintf(s, "[%9d]", elapsed);
    console->PutString(s);
  }else{
    console->PutString(s);
  }
  return result;
} 

std::shared_ptr<ToplevelWindow> main_window;
unsigned int main_window_layer_id;
void InitializeMainWindow() {
  main_window = std::make_shared<ToplevelWindow>(
      160, 68, screen_frame_buffer_config.pixel_format,
      "Hello Window");
  // DrawWindow(*main_window->Writer(), "Hello Window");
  WriteString(*main_window->Writer(), {18, 28}, "MikanSAOS world!", {0, 0, 0});

  main_window_layer_id = layer_manager->NewLayer()
      .SetWindow(main_window)
      .SetDraggable(true)
      .SetTransparentable(true)
      .Move({300, 100})
      .ID();

  layer_manager->SetIndex(main_window_layer_id, std::numeric_limits<int>::max());
}

std::shared_ptr<ToplevelWindow> text_window;
unsigned int text_window_layer_id;
void InitializeTextWindow() {
  const int win_w = 160;
  const int win_h = 54;

  text_window = std::make_shared<ToplevelWindow>(
      win_w, win_h, screen_frame_buffer_config.pixel_format,
      "Text Box Test");
  // DrawWindow(*text_window->Writer(), "Text Box Test");
  // DrawTextbox(*text_window->Writer(), {2, 26}, {win_w - 2*2, win_h - 26 - 2});
  DrawTextbox(*text_window->InnerWriter(), {0, 0}, text_window->InnerSize());

  text_window_layer_id = layer_manager->NewLayer()
    .SetWindow(text_window)
    .SetDraggable(true)
    .SetTransparentable(true)
    .Move({500, 100})
    .ID();

  layer_manager->SetIndex(text_window_layer_id, std::numeric_limits<int>::max());
}

std::shared_ptr<ToplevelWindow> task_b_window;
unsigned int task_b_window_layer_id;
void InitializeTaskBWindow() {
  task_b_window = std::make_shared<ToplevelWindow>(
      160, 52, screen_frame_buffer_config.pixel_format,
      "TaskB Window");
  // DrawWindow(*task_b_window->Writer(), "TaskB Window");

  task_b_window_layer_id = layer_manager->NewLayer()
    .SetWindow(task_b_window)
    .SetDraggable(true)
    .SetTransparentable(true)
    .Move({100, 100})
    .ID();

  layer_manager->SetIndex(task_b_window_layer_id, std::numeric_limits<int>::max());
}



void TaskB(uint64_t task_id, int64_t data) {
  printk("TaskB: task_id=%d, data=%d\n", task_id, data);
  char str[128];
  int count = 0;

  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  __asm__("sti");

  while (true) {
    ++count;
    sprintf(str, "%010d", count);
    // FillRectangle(*task_b_window->Writer(), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
    // WriteString(*task_b_window->Writer(), {24, 28}, str, {0, 0, 0});
    // layer_manager->Draw(task_b_window_layer_id);
    FillRectangle(*task_b_window->InnerWriter(), {20, 4}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
    WriteString(*task_b_window->InnerWriter(), {20, 4}, str, {0, 0, 0});

    // SwitchContext(&task_a_ctx, &task_b_ctx);

    if(count == 10000){
        task.Sleep();
    }

    Message msg{Message::kLayerOps, task_id};
    msg.arg.layer.layer_id = task_b_window_layer_id;
    msg.arg.layer.op = LayerOperation::Draw;
    __asm__("cli");
    task_manager->SendMessage(1, msg);
    __asm__("sti");

    while(true){
      __asm__("cli");
      auto msg = task.ReceiveMessage();
      if(!msg){
        task.Sleep();
        __asm__("sti");
        continue;
      }

      if(msg->type == Message::kLayerOpsFinish){
        break;
      }
    }

  }
}

int text_window_index;

void DrawTextCursor(bool visible) {
  const auto color = visible ? kTextBoxTextColor : kTextBoxBGColor;
  // const auto pos = Vector2D<int>{8 + 8*text_window_index, 26 + 3};
  const auto pos = Vector2D<int>{6 + 8*text_window_index, 3};
  FillRectangle(*text_window->InnerWriter(), pos, {2, 18}, color);
}

void InputTextWindow(char c) {
  if (c == 0) {
    return;
  }

  auto pos = []() { return Vector2D<int>{4 + 8*text_window_index, 4}; };

  const int max_chars = (text_window->InnerSize().x- 8) / 8;
  if (c == '\b' && text_window_index > 0) {
    DrawTextCursor(false);
    text_window_index--;
    FillRectangle(*text_window->InnerWriter(), pos(), {8, 16}, kTextBoxBGColor);
    DrawTextCursor(true);
  } else if (c >= ' ' && text_window_index < max_chars) {
    DrawTextCursor(false);
    WriteAscii(*text_window->InnerWriter(), pos(), c, kTextBoxTextColor);
    text_window_index++;
    DrawTextCursor(true);
  }

  layer_manager->Draw(text_window_layer_id);
}




void TaskWallclock(uint64_t task_id, int64_t data) {
  __asm__("cli");
  Task& task = task_manager->CurrentTask();

  const int kFrameWidth = ScreenSize().x;
  const int kFrameHeight = ScreenSize().y;

  auto date_window = std::make_shared<Window>(
      8 * 10, 16, screen_frame_buffer_config.pixel_format);
  const auto date_window_layer_id = layer_manager->NewLayer()
    .SetWindow(date_window)
    .SetDraggable(false)
    .Move({kFrameWidth-80-20, kFrameHeight - kTaskBarHeight/2 -8})
    .ID();

  auto clock_window = std::make_shared<Window>(
      48, 48, screen_frame_buffer_config.pixel_format);
  const auto clock_window_layer_id = layer_manager->NewLayer()
    .SetWindow(clock_window)
    .SetDraggable(false)
    .Move({kFrameWidth/2 -48/2, kFrameHeight - kInfoBallRadius -5 -48/2})
    .ID();
  
  layer_manager->SetIndex(date_window_layer_id, 2);
  layer_manager->SetIndex(clock_window_layer_id, 2);
  __asm__("sti");

  auto draw_current_time = [&]() {
    EFI_TIME t;

    int64_t err = uefi_rts->GetTime(&t, 0);
    if(err != 0){
      printk("get time fail: %ld\n",err);
      while(1){
        __asm__("hlt");
      }
    }
 
    FillRectangle(*date_window->Writer(),
                  {0, 0}, date_window->Size(), kTaskBarColor);

    FillRectangle(*clock_window->Writer(),
                  {0, 0}, clock_window->Size(), kMainLightColor);
 
    char s[64];
    sprintf(s, "%04d-%02d-%02d", t.Year, t.Month, t.Day);
    WriteString(*date_window->Writer(), {0, 0}, s, kMainTextColor);
    
    sprintf(s, "%01d%01d", t.Second/10,t.Second%10);
    // WriteString(*clock_window->Writer(), {8*5/2 - 3*8/2, 16}, s, kDesktopFGColor);
    auto [u32, bytes] = ConvertUTF8To32(s);
    WriteUnicode(*clock_window->Writer(), {48/2 - 38/2 -3, 48/2 - 38/2 -7}, u32, kMainLight2Color, 38, true);
    auto [u32_2, bytes_2] = ConvertUTF8To32(&s[1]);
    WriteUnicode(*clock_window->Writer(), {48/2 + 3, 48/2 - 38/2 -7}, u32_2, kMainLight2Color, 38, true);

    sprintf(s, "%02d:%02d", t.Hour, t.Minute);
    WriteString(*clock_window->Writer(), {48/2 -(8*5)/2, 48/2 - 8}, s, kDesktopFGColor);

    Message msg_date{Message::kLayerOps, task_id};
    msg_date.arg.layer.layer_id = date_window_layer_id;
    msg_date.arg.layer.op = LayerOperation::Draw;

    Message msg_clock{Message::kLayerOps, task_id};
    msg_clock.arg.layer.layer_id = clock_window_layer_id;
    msg_clock.arg.layer.op = LayerOperation::Draw;

    __asm__("cli");
    task_manager->SendMessage(1, msg_date);
    task_manager->SendMessage(1, msg_clock);
     __asm__("sti");
  };

  draw_current_time();
  timer_manager->AddTimer(
      Timer{timer_manager->CurrentTick(), 1, task_id});

  while (true) {
    __asm__("cli");
    auto msg = task.ReceiveMessage();
    if (!msg) {
      task.Sleep();
      __asm__("sti");
      continue;
    }
    __asm__("sti");

    if (msg->type == Message::kTimerTimeout) {
      draw_current_time();
      timer_manager->AddTimer(
          Timer{msg->arg.timer.timeout + kTimerFreq, 1, task_id});
    }
  }
}


std::deque<Message>* main_queue;

//init manage memory(stack)
const uint32_t kernel_main_stack_size = 1024*1024;
alignas(16) uint8_t kernel_main_stack[kernel_main_stack_size];
uint8_t* kernel_main_stack_base = kernel_main_stack + kernel_main_stack_size;

extern "C" void KernelMainNewStack(
    const FrameBufferConfig& frame_buffer_config_ref,
    const MemoryMap& memory_map_ref,
    const acpi::RSDP& acpi_table,
    void* volume_image,
    EFI_RUNTIME_SERVICES* rts){

  MemoryMap memory_map{memory_map_ref};
  uefi_rts = rts;


  InitializeGraphics(frame_buffer_config_ref);
  InitializeConsole();

  printk("welcome to mikanSAOS!\n");
  SetLogLevel(kWarn);
  // SetLogLevel(kDebug);

  Log(kDebug, "stack base: %p\n",kernel_main_stack_base);


  InitializeSegmentation();
  InitializePaging();
  InitializeMemoryManager(memory_map);
  InitializeTSS();
  
  //init queue for interrupt
  // std::array<Message, 32> main_queue_buf;
  // ArrayQueue<Message> main_queue{main_queue_buf};
  // ::main_queue = &main_queue;
  // ::main_queue = new std::deque<Message>();
  // InitializeInterrupt(main_queue);
  InitializeInterrupt();

  fat32::Initialize(volume_image);
  InitializeFont();
  InitializePCI();

  InitializeLayer();
  InitializeMainWindow();
  InitializeTextWindow();
  InitializeTaskBWindow();

  layer_manager->Draw({{0,0}, ScreenSize()});

  // active_layer->Activate(task_b_window_layer_id);

  Log(kDebug, "desktop initialzed\n");

  // //usb recevie event
  // Log(kInfo, "ProcessEvent starting\n");
  // while(1){
  //   if(auto err = ProcessEvent(xhc)){
  //     Log(kError, err, "Error while ProcessEvent: %s", err.Name());
  //   }
  // }

  acpi::Initialize(acpi_table);
  InitializeLAPICTimer();

  // timer_manager->AddTimer(Timer(200, 2));

  const int kTextboxCursorTimer = 1;
  const int kTimer05Sec = static_cast<int>(kTimerFreq * 0.5);
  // __asm__("cli");
  timer_manager->AddTimer(Timer{kTimer05Sec, kTextboxCursorTimer, 1});
  // __asm__("sti");
  bool textbox_cursor_visible = false;

  InitializeSyscall();

  InitializeTask();
  Task& main_task = task_manager->CurrentTask();
  // terminals = new std::map<uint64_t, Terminal*>;

  // for(int i=1;i<10;i++){
  //   task_manager->NewTask().InitContext(TaskIdle, i*111111).Wakeup();
  // }

  usb::xhci::Initialize();
  InitializeKeyboard();
  InitializeMouse();

  app_loads = new std::map<fat32::DirectoryEntry*, AppLoadInfo>;
  const uint64_t taskb_id = task_manager->NewTask().InitContext(TaskB, 45)
    .Wakeup()
    .ID();

  task_manager->NewTask()
      .InitContext(TaskTerminal, 0)
      .Wakeup();

  task_manager->NewTask()
    .InitContext(TaskWallclock, 0)
    .Wakeup();

  char conter_str[128];
  //process message queue
  while(true){ 
    __asm__("cli");
    const auto tick = timer_manager->CurrentTick();
    __asm__("sti");

    sprintf(conter_str, "%010lu", tick);
    // FillRectangle(*main_window->Writer(), {18, 44}, {8 * 10, 16}, kWindowBGColor);
    // WriteString(*main_window->Writer(), {18, 44}, conter_str, kWindowFGColor);
    FillRectangle(*main_window->InnerWriter(), {14, 19}, {8 * 10, 16}, kWindowBGColor);
    WriteString(*main_window->InnerWriter(), {14, 19}, conter_str, kWindowFGColor);
    layer_manager->Draw(main_window_layer_id);

    // __asm__("cli");
    // // if(!main_queue.HasFront()){
    // if(main_queue->size() == 0){
    //   __asm__("sti\n\thlt");
    //   continue;
    // }
    // Message msg = main_queue.Front();
    // Message msg = main_queue->front();
    // main_queue->pop_front();
    // __asm__("sti");

    __asm__("cli");
    auto msg = main_task.ReceiveMessage();
    if(!msg){
      // __asm__("sti\n\thlt");
      main_task.Sleep();
      __asm__("sti");

      continue;
    }
    
    __asm__("sti");

    switch (msg->type){
      case Message::kInterruptXHCI:
        usb::xhci::ProcessEvents();
        break;
      case Message::kInterruptLAPICTimer:
        printk("Timer interrupt\n");
        break;
      case Message::kTimerTimeout:
        // printk("Timer: timeout = %lu, value = %d\n",
        //     msg.arg.timer.timeout, msg.arg.timer.value);
        if (msg->arg.timer.value == kTextboxCursorTimer) {
          __asm__("cli");
          timer_manager->AddTimer(
              Timer{msg->arg.timer.timeout + kTimer05Sec, kTextboxCursorTimer, 1});
          __asm__("sti");
          textbox_cursor_visible = !textbox_cursor_visible;
          DrawTextCursor(textbox_cursor_visible);
          layer_manager->Draw(text_window_layer_id);

        // __asm__("cli");
        // task_manager->SendMessage(task_terminal_id, *msg);
        // __asm__("sti");
        }
        break;
      case Message::kKeyPush:
        {
          auto act = active_layer->GetActive(); 
          if (act == text_window_layer_id) {
            if (msg->arg.keyboard.press) {
              InputTextWindow(msg->arg.keyboard.ascii);
            }
          }else if (act == task_b_window_layer_id){
            if(msg->arg.keyboard.ascii == 's'){
              printk("sleep TaskB: %s\n", task_manager->Sleep(taskb_id).Name());
            }else if(msg->arg.keyboard.ascii == 'w'){
              printk("wakeup TaskB: %s\n", task_manager->Wakeup(taskb_id).Name());
            }
          }else if (msg->arg.keyboard.press &&
                    msg->arg.keyboard.keycode == 59) {
            /* F2 */
            task_manager->NewTask()
              .InitContext(TaskTerminal, 0)
              .Wakeup();
          }else{
            __asm__("cli");
            auto task_it = layer_task_map->find(act);
            __asm__("sti");
            if(task_it != layer_task_map->end()){
              __asm__("cli");
              task_manager->SendMessage(task_it->second,*msg);
              __asm__("sti");
            }else{
              printk("key push not handled: keycode %02x, ascii %02x\n",
                msg->arg.keyboard.keycode,
                msg->arg.keyboard.ascii);
            }
          }
        }
        break;
      case Message::kLayerOps:
        ProcessLayerMessage(*msg);
        __asm__("cli");
        task_manager->SendMessage(msg->src_task_id, Message{Message::kLayerOpsFinish});
        __asm__("sti");
        break;
      default:
        Log(kError, "Unknown message type: %d\n", msg->type);
        break;
    }

  }
}

extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}
