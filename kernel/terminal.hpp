#pragma once

#include <memory>
#include <deque>
#include <optional>
#include <map>

#include "graphics.hpp"
#include "window.hpp"
#include "fat.hpp"
#include "task.hpp"
#include "paging.hpp"

struct AppLoadInfo {
  uint64_t vaddr_end, entry;
  PageMapEntry* pml4;
  AppLoadInfo* prototype;
  uint32_t count;
};

extern std::map<fat32::DirectoryEntry*, AppLoadInfo>* app_loads;

struct TerminalDescriptor {
  std::string command_line;
  bool exit_after_command;
  bool show_window;
  std::array<std::shared_ptr<FileDescriptor>, 3> files;
};

class Terminal {
  public:
    static const int kRows = 18, kColumns = 60;
    static const int kLineMax = 128;

    // Terminal(uint64_t task_id, bool show_window);
    // Terminal(Task& task, bool show_window);
    Terminal(Task& task, const TerminalDescriptor* term_desc);
    unsigned int LayerID() const { return _layer_id; }
    Rectangle<int> BlinkCursor();
    Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);
  
    void Print(const char* s, std::optional<size_t> len = std::nullopt);
  
    Task& UnderlyingTask() const { return _task; }
    int LastExitCode() const { return _last_exit_code; }
    void Redraw();

  private:
    std::shared_ptr<ToplevelWindow> _window;
    unsigned int _layer_id;
    // uint64_t _task_id;
    Task& _task;
    bool _show_window;

    Vector2D<int> _cursor{0, 0};
    bool _cursor_visible{false};
    int _linebuf_index{0};
    std::array<char, kLineMax> _linebuf{};

    void DrawCursor(bool visible);
    Vector2D<int> CalcCursorPos() const;
    void ScrollOneLine();

    void ExecuteLine();
    WithError<int> ExecuteFile(fat32::DirectoryEntry& file_entry, char* command, char* arg);
    void Print(char32_t c);

    std::deque<std::array<char, kLineMax>> _cmd_history{};
    int _cmd_history_index{-1};
    Rectangle<int> HistoryUpDown(int direction);

     std::array<std::shared_ptr<FileDescriptor>, 3> _files;
     int _last_exit_code{0};
};

//task_id to terminal
// extern std::map<uint64_t, Terminal*>* terminals;

void TaskTerminal(uint64_t task_id, int64_t data);

class TerminalFileDescriptor : public FileDescriptor {
  public:
    // explicit TerminalFileDescriptor(Task& task, Terminal& term);
    explicit TerminalFileDescriptor(Terminal& term);
    size_t Read(void* buf, size_t len) override;
    size_t Write(const void* buf, size_t len) override;
    size_t Size() const override { return 0; }
    size_t Load(void* buf, size_t len, size_t offset) override;

  private:
    // Task& _task;
    Terminal& _term;
};


class PipeDescriptor : public FileDescriptor {
 public:
  explicit PipeDescriptor(Task& task);
  size_t Read(void* buf, size_t len) override;
  size_t Write(const void* buf, size_t len) override;
  size_t Size() const override { return 0; }
  size_t Load(void* buf, size_t len, size_t offset) override { return 0; }

  void FinishWrite();

 private:
  Task& _task;
  char _data[16];
  size_t _len{0};
  bool _closed{false};
};