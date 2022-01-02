#include "task.hpp"

#include <cstring>
#include <algorithm>

#include "asmfunc.h"
#include "timer.hpp"
#include "segment.hpp"
#include "logger.hpp"

namespace{
  template<class T, class U>
  void Erase(T& c, const U& value){
    auto iter = std::remove(c.begin(), c.end(), value);
    c.erase(iter, c.end());
  }

  void TaskIdle(uint64_t task_id, int64_t data) {
    printk("TaskIdle: task_id=%d, data=%d\n", task_id, data);
    while (true) __asm__("hlt");
  }

}

Task::Task(uint64_t id) : _id{id}, _msgs{} {
}


Task& Task::InitContext(TaskFunc* f, int64_t data){
  const size_t stack_size = kDefaultStackBytes / sizeof(_stack[0]);
  _stack.resize(stack_size);

  uint64_t stack_end = reinterpret_cast<uint64_t>(&_stack[stack_size]);

  memset(&_context, 0, sizeof(_context));
  _context.rip = reinterpret_cast<uint64_t>(f);
  _context.rdi = _id;
  _context.rsi = data;

  _context.cr3 = GetCR3();
  //IF(interrupt FLAG)
  _context.rflags = 0x202;
  _context.cs = kKernelCS;
  _context.ss = kKernelSS;
  _context.rsp = (stack_end & ~0xflu) - 8;
  //mxcsr 12:7 1 (intel sdm 10.2.3 mxcsr control and status register)
  *reinterpret_cast<uint32_t*>(&_context.fxsave_area[24]) = 0x1f80;

  return *this;
}

TaskContext& Task::Context(){
  return _context;
}

uint64_t& Task::OSStackPointer(){
   return _os_stack_ptr;
}

uint64_t Task::ID() const{
  return _id;
}
Task& Task::Sleep(){
  task_manager->Sleep(this);
  return *this;
}
Task& Task::Wakeup(){
  task_manager->Wakeup(this);
  return *this;
}

void Task::SendMessage(const Message& msg){
  _msgs.push_back(msg);
  Wakeup();
}

std::optional<Message> Task::ReceiveMessage(){
  if(_msgs.empty()){
    return std::nullopt;
  }

  auto m = _msgs.front();
  _msgs.pop_front();
  return m;
}

std::vector<std::shared_ptr<::FileDescriptor>>& Task::Files() {
  return _files;
}

uint64_t Task::DPagingBegin() const {
  return _dpaging_begin;
}

void Task::SetDPagingBegin(uint64_t v) {
  _dpaging_begin = v;
}

uint64_t Task::DPagingEnd() const {
  return _dpaging_end;
}

void Task::SetDPagingEnd(uint64_t v) {
  _dpaging_end = v;
}

uint64_t Task::FileMapEnd() const {
  return _file_map_end;
}

void Task::SetFileMapEnd(uint64_t v) {
  _file_map_end = v;
}

std::vector<FileMapping>& Task::FileMaps() {
  return _file_maps;
}

TaskManager::TaskManager(){
  Task& main_task = NewTask()
      .SetLevel(_current_level)
      .SetReadyOrRunning(true);
  
  _runnning[_current_level].push_back(&main_task);

  Task& idle_task = NewTask()
     .InitContext(TaskIdle, 0)
     .SetLevel(0)
     .SetReadyOrRunning(true);

  _runnning[0].push_back(&idle_task);
}

Task& TaskManager::NewTask(){
  _last_id++;
  return *_tasks.emplace_back(new Task{_last_id});
}

void TaskManager::SwitchTask(const TaskContext& current_ctx){
  TaskContext& task_ctx = task_manager->CurrentTask().Context();
  memcpy(&task_ctx, &current_ctx, sizeof(TaskContext));

  Task* current_task = RotateCurrentRunQueue(false);

  if (&CurrentTask() != current_task) {
    RestoreContext(&CurrentTask().Context());
  }

  // SwitchContext(&next_task->Context(), &current_task->Context());
}

void TaskManager::Sleep(Task* task){
  // auto iter = std::find(_runnning.begin(), _runnning.end(), task);

  // if(iter == _runnning.begin()){
  //   SwitchTask(true);
  //   return;
  // }

  // if(iter== _runnning.end()){
  //   return;
  // }

  // _runnning.erase(iter);
  if(!task->ReadyOrRunning()){
    return;
  }
  task->SetReadyOrRunning(false);

  if(task == _runnning[_current_level].front()){
    // SwitchTask(true);

    Task* current_task = RotateCurrentRunQueue(true);
    SwitchContext(&CurrentTask().Context(), &current_task->Context());
    return;
  }

 
  Erase(_runnning[task->Level()], task);
}

Error TaskManager::Sleep(uint64_t id){
  auto iter = std::find_if(_tasks.begin(), _tasks.end(),
      [id](const auto& t){ return t->ID() == id; });
  if(iter == _tasks.end()){
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  Sleep(iter->get());
  return MAKE_ERROR(Error::kSuccess);
}

void TaskManager::Wakeup(Task* task, int level){
  // auto iter = std::find(_runnning.begin(), _runnning.end(), task);
  // if(iter == _runnning.end()){
  //   _runnning.push_back(task);
  // }
  if(task->ReadyOrRunning()){
    ChangeLevelRunning(task, level);
    return;
  }

  if(level < 0){
    level = task->Level();
  }

  task->SetLevel(level);
  task->SetReadyOrRunning(true);

  _runnning[level].push_back(task);
  if(level > _current_level){
    _level_changed = true;
  }
}

Error TaskManager::Wakeup(uint64_t id, int level){
  auto iter = std::find_if(_tasks.begin(), _tasks.end(),
      [id](const auto& t){ return t->ID() == id; });
  if(iter == _tasks.end()){
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  Wakeup(iter->get(), level);
  return MAKE_ERROR(Error::kSuccess);
}

Error TaskManager::SendMessage(uint64_t id, const Message& msg){
  auto iter = std::find_if(_tasks.begin(), _tasks.end(),
      [id](const auto& t){ return t->ID() == id; });
  if(iter == _tasks.end()){
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  iter->get()->SendMessage(msg);
  return MAKE_ERROR(Error::kSuccess);
}

Task& TaskManager::CurrentTask(){
  return *_runnning[_current_level].front();
}

void TaskManager::Finish(int exit_code){
  Task* current_task = RotateCurrentRunQueue(true);

  const auto task_id = current_task->ID();
  auto it = std::find_if(
      _tasks.begin(), _tasks.end(),
      [current_task](const auto& t){ return t.get() == current_task; });
  _tasks.erase(it);

  _finish_tasks[task_id] = exit_code;
  auto iter = _finish_waiter.find(task_id); 
  if (iter != _finish_waiter.end()) {
    auto waiter = iter->second;
    _finish_waiter.erase(iter);
    Wakeup(waiter);
  }

  RestoreContext(&CurrentTask().Context());
}

WithError<int> TaskManager::WaitFinish(uint64_t task_id){
  int exit_code;
  Task* current_task = &CurrentTask();
  while (true) {
    auto iter = _finish_tasks.find(task_id);
    if ( iter != _finish_tasks.end()) {
      //already finished
      exit_code = iter->second;
      _finish_tasks.erase(iter);
      break;
    }
    //wait finish
    _finish_waiter[task_id] = current_task;
    Sleep(current_task);
  }
  return { exit_code, MAKE_ERROR(Error::kSuccess) };
}

void TaskManager::ChangeLevelRunning(Task* task, int level){
  if(level < 0 || level == task->Level()){
    return;
  }

  if(task != _runnning[_current_level].front()){
    Erase(_runnning[task->Level()], task);
    _runnning[level].push_back(task);
  
    task->SetLevel(level);
    if(level > _current_level){
      _level_changed = true;
    }
  }

  //change self
  _runnning[_current_level].pop_front();
  _runnning[level].push_front(task);

  task->SetLevel(level);
  if(level>= _current_level){
    _current_level = level;
  }else{
    _current_level = level;
    _level_changed = true;
  }
}

Task* TaskManager::RotateCurrentRunQueue(bool current_sleep) {
  // size_t next_task_index = _current_task_index + 1;
  // if(next_task_index>=_tasks.size()){
  //   next_task_index = 0;
  // }

  // Task& current_task = *_tasks[_current_task_index];
  // Task& next_task = *_tasks[next_task_index];

  // _current_task_index = next_task_index;

  auto current_level_queue = &_runnning[_current_level];
  Task* current_task = current_level_queue->front();
  current_level_queue->pop_front();
  if(!current_sleep){
    current_level_queue->push_back(current_task);
  }

  if(current_level_queue->empty()){
    _level_changed = true;
  }

  if(_level_changed){
    _level_changed = false;
    for(int lv = kMaxLevel; lv>= 0; lv--){
      if(!_runnning[lv].empty()){
        _current_level = lv;
        current_level_queue = &_runnning[_current_level];
        break;
      }
    }
  }

  // Task* next_task = current_level_queue->front();

  return current_task;
}

TaskManager* task_manager;

void InitializeTask() {
  task_manager = new TaskManager;

  __asm__("cli");
  timer_manager->AddTimer(
      Timer{timer_manager->CurrentTick() + kTaskTimerPeriod, kTaskTimerValue, 1});
  __asm__("sti");
}

__attribute__((no_caller_saved_registers))
extern "C" uint64_t GetCurrentTaskOSStackPointer() {
  return task_manager->CurrentTask().OSStackPointer();
}