/**
 * @file usb/memory.cpp
 */

#include "usb/memory.hpp"

#include <cstdint>

namespace {
  
  template<class T>
  T Align(T value, unsigned int alignment){
    return (value + alignment -1) & ~static_cast<T>(alignment-1);
  }
}

namespace usb{
  alignas(64) uint8_t memory_pool[kMemoryPollSize];
  uintptr_t alloc_ptr = reinterpret_cast<uintptr_t>(memory_pool);

  void* AllocMem(size_t size, unsigned int alignment, unsigned int boundary){
    if(alignment > 0){
      alloc_ptr = Align(alloc_ptr, alignment);
    }
    if(boundary > 0){
      auto next_boundary = Align(alloc_ptr, boundary);
      if(alloc_ptr + size > next_boundary){
        alloc_ptr = next_boundary;
      }
    }

    if(alloc_ptr + size >
      reinterpret_cast<uintptr_t>(memory_pool) + kMemoryPollSize){
        return nullptr;
    }

    auto pAllocated = alloc_ptr;
    alloc_ptr += size;
    return reinterpret_cast<void*>(pAllocated);
  }
  
  void FreeMem(void* p){

  }
}