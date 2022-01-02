#pragma once 

#include <cstdint>
#include <array>
#include <vector>
#include <memory>
#include <deque>
#include <optional>
#include <map>

#include "error.hpp"
#include "message.hpp"
#include "fat.hpp"

struct TaskContext {
  uint64_t cr3, rip, rflags, reserved1; // offset 0x00
  uint64_t cs, ss, fs, gs; // offset 0x20
  uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp; // offset 0x40
  uint64_t r8, r9, r10, r11, r12, r13, r14, r15; // offset 0x80
  std::array<uint8_t, 512> fxsave_area; // offset 0xc0
} __attribute__((packed));

using TaskFunc = void (uint64_t, int64_t);

class TaskManager;

struct FileMapping {
  int fd;
  uint64_t vaddr_begin, vaddr_end;
};

class Task{
  public:
    static const int kDefaultLevel = 1;
    static const size_t kDefaultStackBytes = 4096 * 16;

    Task(uint64_t id);
    Task& InitContext(TaskFunc* f, int64_t data);
    TaskContext& Context();
    uint64_t& OSStackPointer();
    uint64_t ID() const;
    Task& Sleep();
    Task& Wakeup();
    void SendMessage(const Message& msg);
    std::optional<Message> ReceiveMessage();
    std::vector<std::shared_ptr<::FileDescriptor>>& Files();
    uint64_t DPagingBegin() const;
    void SetDPagingBegin(uint64_t v);
    uint64_t DPagingEnd() const;
    void SetDPagingEnd(uint64_t v);
    uint64_t FileMapEnd() const;
    void SetFileMapEnd(uint64_t v);
    std::vector<FileMapping>& FileMaps();

    int Level() const { return _level; }
    bool ReadyOrRunning() const { return _ready_or_running;}
  private:
    uint64_t _id;
    std::vector<uint64_t> _stack;
    alignas(16) TaskContext _context;
    uint64_t _os_stack_ptr;
    std::deque<Message> _msgs;
    unsigned int _level{kDefaultLevel};
    bool _ready_or_running{false};
    std::vector<std::shared_ptr<::FileDescriptor>> _files{};
    uint64_t _dpaging_begin{0}, _dpaging_end{0};
    uint64_t _file_map_end{0};
    std::vector<FileMapping> _file_maps{};

    Task& SetLevel(int level){ 
      _level = level;
      return *this;
    }
     Task& SetReadyOrRunning(int ready_or_running){ 
      _ready_or_running = ready_or_running;
      return *this;
    }

    friend TaskManager;
};

class TaskManager{
  public:
    //highest:kMaxLevel lowest:0 
    static const int kMaxLevel = 3;

    TaskManager();
    Task& NewTask();
    void SwitchTask(const TaskContext& current_ctx);

    void Sleep(Task* task);
    Error Sleep(uint64_t id);
    void Wakeup(Task* task, int level = -1);
    Error Wakeup(uint64_t id, int level = -1);
    Error SendMessage(uint64_t id, const Message& msg);
    Task& CurrentTask();
    void Finish(int exit_code);
    WithError<int> WaitFinish(uint64_t task_id);

  private:
    std::vector<std::unique_ptr<Task>> _tasks{};
    uint64_t _last_id{0};
    // size_t _current_task_index{0};
    std::array<std::deque<Task*>, kMaxLevel + 1> _runnning{};
    int _current_level{kMaxLevel};
    bool _level_changed{false};
     // key: ID of a finished task
    std::map<uint64_t, int> _finish_tasks{};
    std::map<uint64_t, Task*> _finish_waiter{};

    void ChangeLevelRunning(Task* task, int level);
    Task* RotateCurrentRunQueue(bool current_sleep);
};

extern TaskManager* task_manager;


void InitializeTask();