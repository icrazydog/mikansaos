#pragma once

#include <cstdint>
#include <array>
#include <cstring>

#include "error.hpp"
#include "usb/xhci/trb.hpp"
#include "usb/memory.hpp"
#include "usb/xhci/registers.hpp"

namespace usb::xhci{

   /** @brief Command/Transfer Ring */
  class Ring{
    public:
      Ring() = default;
      Ring(const Ring&) = delete;
      ~Ring();
      Ring& operator=(const Ring&) = delete;

      Error Initialize(size_t buf_size);

      template <typename T>
      TRB* Push(const T& trb){
        return Push(trb.data);
      }

      TRB* Buffer() const { return _buf;}

      bool RingCycleState() const { return _cycle_bit==1u; }

    private:
      TRB* _buf = nullptr;
      size_t _buf_size = 0;

      uint8_t _cycle_bit;
      size_t _write_index;

      TRB* Push(const std::array<uint32_t, 4>& data);
  };

  union EventRingSegmentTableEntry{
    std::array<uint32_t, 4> data;
    struct {
      // 64 byte alignment [5:0] = 0
      uint64_t ring_segment_base_address;
      uint32_t ring_segment_size : 16;
      uint32_t : 16;
      uint32_t : 32;
    } __attribute__((packed)) bits;
  };

  class EventRing{
    public:
      Error Initialize(size_t buf_size, InterrupterRegisterSet* interrupter);
      bool HasFront() const{
        return Front()->bits.cycle_bit == _cycle_bit;
      }
      TRB* Front() const{
        return ReadDequeuePointer();
      }
      void Pop();

    private:
      TRB* _buf = nullptr;
      size_t _buf_size = 0;

      bool _cycle_bit;
      EventRingSegmentTableEntry* _erst;
      InterrupterRegisterSet* _interrupter;

      TRB* ReadDequeuePointer() const {
        
        auto t= reinterpret_cast<TRB*>(_interrupter->ERDP.Read().Pointer());
        Log(kDebugMass, "ReadDequeuePointer:%08lx(%08lx)type=%d\n",_interrupter->ERDP.Read().Pointer(),_buf,t->bits.trb_type);
        return t;
      }

      void WriteDequeuePointer(TRB* p);
  };
}