#pragma once

#include <cstddef>
#include <cstring>

namespace usb{
  //4k * 32 byte
  static const size_t kMemoryPollSize = 4096 * 32;

  void* AllocMem(size_t size, unsigned int alignment, unsigned int boundary);
  void FreeMem(void* p);

  template <class T>
  T* AllocArray(size_t count, unsigned int alignment, unsigned int boundary){
    
    void* p = AllocMem(sizeof(T) * count, alignment, boundary);
    if(p != nullptr){
        memset(p, 0, count * sizeof(T));
    }
    T* tp = reinterpret_cast<T*>(p);
    
    return tp;
  }

  template <class T>
  T* AllocArray(size_t count){
    return AllocArray<T>(count, 64, 4096);
  }
}