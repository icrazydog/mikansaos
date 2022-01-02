#pragma once

#include <cstdio>
#include <limits>

#include "error.hpp"
#include "memory_map.hpp"

namespace{
  constexpr unsigned long long operator""_KiB(unsigned long long kib){
    return kib * 1024;
  }

  constexpr unsigned long long operator""_MiB(unsigned long long mib){
    return mib * 1024_KiB;
  }

  constexpr unsigned long long operator""_GiB(unsigned long long gib){
    return gib * 1024_MiB;
  }
}

static const auto kBytesPerFrame{4_KiB};

class FrameID{
  public:
    explicit FrameID(size_t id) : _id{id}{}
    size_t ID() const {return _id;}
    void* Frame() const {return reinterpret_cast<void*>(_id * kBytesPerFrame);}

  private:
    size_t _id;
};

static const FrameID kNullFrame{std::numeric_limits<size_t>::max()};

struct MemoryStat {
  size_t allocated_frames;
  size_t total_frames;
};

class BitmapMemoryManager{
  public:
   using MapLineType = unsigned long;
    static const auto kMaxPhysicalMemoryBytes{128_GiB};
    static const auto kFrameCount{kMaxPhysicalMemoryBytes / kBytesPerFrame};
    static const size_t kBitsPerMapLine{8 * sizeof(MapLineType)};
    
    BitmapMemoryManager();
    WithError<FrameID> Allocate(size_t num_frames);
    Error Free(FrameID start_frame, size_t num_frames);

    void MarkAllocated(FrameID start_frame, size_t num_frames);
    void SetMemoryRange(FrameID start_frame, FrameID end_frame);

    MemoryStat Stat() const;
    
  private:
    std::array<MapLineType, kFrameCount / kBitsPerMapLine> _alloc_map;
    FrameID _range_start;
    FrameID _range_end;

    bool GetBit(FrameID frame) const;
    void SetBit(FrameID frame, bool allocated);
};

extern BitmapMemoryManager* memory_manager;
void InitializeMemoryManager(const MemoryMap& memory_map);
