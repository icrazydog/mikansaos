#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../syscall.h"

void stackoverflow(int foo){
   //infinite recursive function
   char s[1024];
   int cur = foo + 1;
   sprintf(s,"s:%d\n",cur);
   printf(s);
   stackoverflow(cur);
}

extern "C" void main(int argc, char** argv) {
  const char* cmd = "hlt";
  if (argc >= 2) {
    cmd = argv[1];
  }

  if (strcmp(cmd, "hlt") == 0) {
    //GP
    __asm__("hlt");
  } else if (strcmp(cmd, "wr_kernel") == 0) {
    //PF
    int* p = reinterpret_cast<int*>(0x100);
    *p = 42;
  } else if (strcmp(cmd, "wr_app") == 0) {
    //PF
    int* p = reinterpret_cast<int*>(0xffff8000ffff0000);
    *p = 123;
  } else if (strcmp(cmd, "zero") == 0) {
    //DE
    volatile int z = 0;
    printf("100/%d = %d\n", z, 100/z);
  } else if (strcmp(cmd, "stackof") == 0) {
    //PF
    stackoverflow(0);
  }else{
     printf("no such cmd\n");
  }

  exit(0);
}
