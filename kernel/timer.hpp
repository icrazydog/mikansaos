#include <cstdint>
#include <queue>

#include "message.hpp"


// void InitializeLAPICTimer(std::deque<Message>& msg_queue);
void InitializeLAPICTimer();
bool IsLAPICTimerInitialized();
void StartLAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();

class Timer{
  public:
    Timer(unsigned long timeout, int value, uint64_t task_id);
    unsigned long Timeout() const{ return _timeout; }
    int Value() const { return _value; }
    uint64_t TaskID() const { return _task_id; }

  private:
    unsigned long _timeout;
    int _value;
    uint64_t _task_id;
};

inline bool operator<(const Timer& lhs, const Timer& rhs){
  return lhs.Timeout() > rhs.Timeout();
}

class TimerManager {
  public:
    // TimerManager(std::deque<Message>& msg_queue);
    TimerManager();
    void AddTimer(const Timer& timer);

    bool Tick();
    unsigned long CurrentTick() const { return _tick; }

 private:
  volatile unsigned long _tick{0};
  std::priority_queue<Timer> _timers{};
  // std::deque<Message>& _msg_queue;
};

extern TimerManager* timer_manager;
extern unsigned long lapic_timer_freq;
const int kTimerFreq = 100;

const int kTaskTimerPeriod = static_cast<int>(kTimerFreq * 0.02);
// const int kTaskTimerPeriod = static_cast<int>(kTimerFreq * 1.0);
const int kTaskTimerValue = std::numeric_limits<int>::min();

// void LAPICTimerOnInterrupt();