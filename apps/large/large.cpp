#include <cstdlib>

char table[3*1024*1024];
extern "C" void main(int argc, char** argv){
  exit(atoi(argv[argc-1]));
  // return atoi(argv[1]);
}