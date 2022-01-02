#include <cstdio>
#include <cstdlib>

extern "C" void main(int argc, char** argv) {
  const char* path = "/memmap";
  if (argc >= 2) {
    path = argv[1];
  }

  FILE* fp = fopen(path, "r");
  if (fp == nullptr) {
    printf("failed to open: %s\n", path);
    exit(1);
  }

  char line[256];
  for (int i = 0; i < 3; ++i) {
    if (fgets(line, sizeof(line), fp) == nullptr) {
      //EOF
      printf("\n");
      exit(0);
    }
    printf("%s", line);
  }
  printf("----\n");
  exit(0);
}
