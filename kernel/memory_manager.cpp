#include "memory_manager.hpp"

#include <bitset>

#include "logger.hpp"
#include "simpletest/test_memory.hpp"

BitmapMemoryManager::BitmapMemoryManager()
    : _alloc_map{},_range_start{FrameID{0}}, _range_end{FrameID{kFrameCount}}{
}

WithError<FrameID> BitmapMemoryManager::Allocate(size_t num_frames){
  size_t start_frame_id = _range_start.ID();
  while(start_frame_id < _range_end.ID()){
    size_t i = 0;
    for(; i < num_frames; i++){
      if(start_frame_id + i >= _range_end.ID()){
        return {kNullFrame, MAKE_ERROR(Error::kNoEnoughMemory)};
      }

      if(GetBit(FrameID{start_frame_id + i})){
        //has allocated
        break;
      }
    }
    if(i == num_frames){
      //allocate success
      MarkAllocated(FrameID{start_frame_id}, num_frames);
      return { FrameID{start_frame_id}, MAKE_ERROR(Error::kSuccess)};
    }

    start_frame_id += i + 1;
  }
  return {kNullFrame, MAKE_ERROR(Error::kNoEnoughMemory)};
}

Error BitmapMemoryManager::Free(FrameID start_frame, size_t num_frames){
  for(size_t i = 0; i < num_frames; i++){
    SetBit(FrameID{start_frame.ID() + i}, false);
  }
  return MAKE_ERROR(Error::kSuccess);
}

void BitmapMemoryManager::MarkAllocated(FrameID start_frame, size_t num_frames){
  for(size_t i = 0; i < num_frames; i++){
    SetBit(FrameID{start_frame.ID() + i}, true);
  }
}
void BitmapMemoryManager::SetMemoryRange(FrameID start_frame, FrameID end_frame){
  _range_start = start_frame;
  _range_end = end_frame;
}

MemoryStat BitmapMemoryManager::Stat() const {
  size_t sum = 0;
  for (int i = _range_start.ID() / kBitsPerMapLine;
       i < _range_end.ID() / kBitsPerMapLine; ++i) {
    sum += std::bitset<kBitsPerMapLine>(_alloc_map[i]).count();
  }
  return { sum, _range_end.ID() - _range_start.ID() };
}


bool BitmapMemoryManager::GetBit(FrameID frame) const{
  auto line_index = frame.ID() / kBitsPerMapLine;
  auto bit_index = frame.ID() % kBitsPerMapLine;
  return (_alloc_map[line_index] & (static_cast<MapLineType>(0x1) << bit_index)) != 0 ;
}

void BitmapMemoryManager::SetBit(FrameID frame, bool allocated){
  auto line_index = frame.ID() / kBitsPerMapLine;
  auto bit_index = frame.ID() % kBitsPerMapLine;
  if(allocated){
    _alloc_map[line_index] |= (static_cast<MapLineType>(0x1) << bit_index);
  }else{
    _alloc_map[line_index] &= ~(static_cast<MapLineType>(0x1) << bit_index);
  }
}


extern "C" caddr_t program_break, program_break_end;

namespace {
  char memory_manager_buf[sizeof(BitmapMemoryManager)];


  Error InitializeHeap(BitmapMemoryManager& memory_manager){
    //64*2M = 128M
    const int kHeapFrames = 64 * 512;
    const auto heap_start = memory_manager.Allocate(kHeapFrames);
    if(heap_start.error){
      return heap_start.error;
    }

    program_break = reinterpret_cast<caddr_t>(heap_start.value.ID() * kBytesPerFrame);
    program_break_end = program_break + kHeapFrames * kBytesPerFrame;
    
    Log(kDebug, "heap addr:%08lx - %08lx\n",program_break,program_break_end);
    return MAKE_ERROR(Error::kSuccess);
  }
}

BitmapMemoryManager* memory_manager;

void InitializeMemoryManager(const MemoryMap& memory_map) {
  Log(kDebug,"memory_map:%p\n", &memory_map);

  //init manage memory(4k frame)
  ::memory_manager = new(memory_manager_buf) BitmapMemoryManager;

  START_TEST_MEMORY(*memory_manager);

  uintptr_t available_end = 0;
  const auto memory_map_base = reinterpret_cast<uintptr_t>(memory_map.buffer);
  for(uintptr_t iter = memory_map_base;
      iter < memory_map_base + memory_map.map_size;
      iter += memory_map.descriptor_size){
    auto desc = reinterpret_cast<const MemoryDescriptor*>(iter);

    const auto physical_end = desc->physical_start + desc->number_of_pages * kUEFIPageSize;
    if(IsAvailable(static_cast<MemoryType>(desc->type))){
      Log(kDebug,"free memory type = %u, phys = %08lx - %08lx, pages = %lu, attr = %08lx\n",
          desc->type,
          desc->physical_start,
          desc->physical_start + desc->number_of_pages * kUEFIPageSize - 1,
          desc->number_of_pages,
          desc->attribute);
      if(available_end < desc->physical_start){
        memory_manager->MarkAllocated(FrameID{available_end/kBytesPerFrame}, 
        (desc->physical_start-available_end)/kBytesPerFrame);
      }
      available_end = physical_end;
    }else{
      memory_manager->MarkAllocated(FrameID{available_end/kBytesPerFrame}, 
      (physical_end - available_end)/kBytesPerFrame);
    }
  }
  
  memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end/kBytesPerFrame});
  Log(kDebug,"total memory frame:%d\n",available_end/kBytesPerFrame);

  if(auto err = InitializeHeap(*memory_manager)){
    Log(kError, err, "failed to allocate pages: %s at %s:%d\n",
        err.Name());
    while (1) __asm__("hlt");
  }

}