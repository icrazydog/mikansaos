#include "timer.hpp"

#include "interrupt.hpp"
#include "acpi.hpp"
#include "task.hpp"

namespace {
  const uint32_t kCountMax = 0xffffffffu;
  static bool initialized = false;
  volatile uint32_t& lvt_timer = *reinterpret_cast<uint32_t*>(0xfee00320);
  volatile uint32_t& initial_count = *reinterpret_cast<uint32_t*>(0xfee00380);
  volatile uint32_t& current_count = *reinterpret_cast<uint32_t*>(0xfee00390);
  volatile uint32_t& divide_config = *reinterpret_cast<uint32_t*>(0xfee003e0);
}

// void InitializeLAPICTimer(std::deque<Message>& msg_queue){
//   timer_manager = new TimerManager(msg_queue);
void InitializeLAPICTimer(){
  timer_manager = new TimerManager();
  /*
  bits: 0,1,3
  000: Divide by 2
  001: Divide by 4
  010: Divide by 8
  011: Divide by 16
  100: Divide by 32
  101: Divide by 64
  110: Divide by 128
  111: Divide by 1
  */
  divide_config = 0b1011;

  /*bits: 18,17 
  00b One-shot mode
  01b Periodic mode
  bits 16: 0 enable 1 inhibit interrupt
  vector 32
  */
  lvt_timer = (0b01<<16) | 32;
  StartLAPICTimer();
  acpi::WaitMilliseconds(100);
  const auto elapsed = LAPICTimerElapsed();
  StopLAPICTimer();

  //hz
  lapic_timer_freq = static_cast<unsigned long>(elapsed) * 10;

  lvt_timer = (0b10<<16) | InterruptVector::kLAPICTimer;
  initialized = false;
  //0.1s
  initial_count = lapic_timer_freq / kTimerFreq;
}

bool IsLAPICTimerInitialized(){
  return initialized;
}
void StartLAPICTimer(){
  initial_count = kCountMax;
}

uint32_t LAPICTimerElapsed(){
  return kCountMax - current_count;
}

void StopLAPICTimer(){
   initial_count = 0;
}

Timer::Timer(unsigned long timeout, int value, uint64_t task_id)
    : _timeout{timeout}, _value{value}, _task_id{task_id}  {
}

// TimerManager::TimerManager(std::deque<Message>& msg_queue)
//     : _msg_queue{msg_queue}{
TimerManager::TimerManager(){
  _timers.push(Timer{std::numeric_limits<unsigned long>::max(), 0, 0});
}

void TimerManager::AddTimer(const Timer& timer) {
  _timers.push(timer);
}

bool TimerManager::Tick() {
  _tick++;

  bool task_timer_timeout = false;
  while(true){
    const auto& t = _timers.top();
    if(t.Timeout() > _tick){
      break;
    }

    if (t.Value() == kTaskTimerValue) {
      task_timer_timeout = true;
      _timers.pop();
      _timers.push(Timer{_tick + kTaskTimerPeriod, kTaskTimerValue, 1});
      continue;
    }

    Message m{Message::kTimerTimeout};
    m.arg.timer.timeout = t.Timeout();
    m.arg.timer.value = t.Value();
    // _msg_queue.push_back(m);
    task_manager->SendMessage(t.TaskID(), m);

    _timers.pop();
  }

  return task_timer_timeout;
}

TimerManager* timer_manager;
unsigned long lapic_timer_freq;

extern "C" void LAPICTimerOnInterrupt(const TaskContext& ctx_stack){
  const bool task_timer_timeout = timer_manager->Tick();
  NotifyEndOfInterrupt();

  if (task_timer_timeout) {
    // SwitchTask();
    task_manager->SwitchTask(ctx_stack);
  }
}