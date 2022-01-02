#include <cstring>
#include <cstdlib>


#include "../syscall.h"

// #include "../../kernel/logger.hpp"

// #include "../../kernel/graphics.hpp"
// //nm -C kernel.elf 
// auto& printk = *reinterpret_cast<int (*)(const char*, ...)>(0x000000000010c000);
// auto& fill_rect = *reinterpret_cast<decltype(FillRectangle)*>(0x000000000010d640);
// auto& screen_writer = *reinterpret_cast<decltype(screen_pixel_writer)*>(0x000000000025a088);

int stack_ptr;
long stack[100];

long Pop() {
  long value = stack[stack_ptr--];
  return value;
}

void Push(long value) {
  stack_ptr++;
  stack[stack_ptr] = value;
}

// extern "C" int64_t SyscallLogString(LogLevel, const char*);
// extern "C" void SyscallExit(int exit_code);

extern "C" void main(int argc, char** argv) {
  stack_ptr = -1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "+") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a + b);
      // printk("[%d] <- %ld\n", stack_ptr, a + b);
    } else if (strcmp(argv[i], "-") == 0) {
      long b = Pop();
      long a = Pop();
      Push(a - b);
      // printk("[%d] <- %ld\n", stack_ptr, a - b);
    } else {
      long a = atol(argv[i]);
      Push(a);
      // printk("[%d] <- %ld\n", stack_ptr, a);
    }
  }

  // fill_rect(*screen_writer, Vector2D<int>{100, 10}, Vector2D<int>{200, 200}, ToColor(0x00ff00));

  long result = 0;
  if (stack_ptr >= 0) {
    result = Pop();
  }
  
  // SyscallLogString(kWarn, "\nhello, this is rpn\n");
  printf("%ld\n", result);
  // SyscallExit(static_cast<int>(result));
  exit(static_cast<int>(result));

  while (1);
  //return static_cast<int>(Pop());
}