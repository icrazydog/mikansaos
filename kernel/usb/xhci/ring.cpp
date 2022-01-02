/**
 * @file usb/xhci/ring.cpp
 */

#include "usb/xhci/ring.hpp"
#include "logger.hpp"

namespace usb::xhci{
  Ring::~Ring(){
    if(_buf != nullptr){
      FreeMem(_buf);
    }
  }

  Error Ring::Initialize(size_t buf_size){
    if(_buf != nullptr){
      FreeMem(_buf);
    }

    _cycle_bit = 1;
    _write_index = 0;
    _buf_size = buf_size;

    _buf = AllocArray<TRB>(_buf_size, 64, 64*1024);
    if(_buf == nullptr){
      return MAKE_ERROR(Error::kNoEnoughMemory);
    }
    //memset(_buf, 0, _buf_size * sizeof(TRB));
    
    return MAKE_ERROR(Error::kSuccess);
  }

  TRB* Ring::Push(const std::array<uint32_t, 4>& data){
    auto trb_ptr = &_buf[_write_index];

    for(int i=0; i<3; i++){
      _buf[_write_index].data[i] = data[i];
    }
    _buf[_write_index].data[3] = (data[3] & 0xfffffffeu) | _cycle_bit;

    _write_index++;
    if(_write_index == _buf_size -1){
      LinkTRB link{_buf};
      link.bits.toggle_cycle = true;
      
      for(int i=0; i<3; i++){
        _buf[_write_index].data[i] = link.data[i];
      }
      _buf[_write_index].data[3] = (link.data[3] & 0xfffffffeu) | _cycle_bit;

      _write_index = 0;
      _cycle_bit = _cycle_bit^0x1u;
      Log(kDebugMass,"_cycle_bit flip %d\n",_cycle_bit);
    }

    return trb_ptr;
  }

  Error EventRing::Initialize(size_t buf_size, InterrupterRegisterSet* interrupter){
    if(_buf != nullptr){
      FreeMem(_buf);
    }

    _cycle_bit = true;
    _buf_size = buf_size;
    _interrupter = interrupter;

    _buf = AllocArray<TRB>(_buf_size, 64, 64*1024);
    if(_buf == nullptr){
      return MAKE_ERROR(Error::kNoEnoughMemory);
    }

    _erst = AllocArray<EventRingSegmentTableEntry>(1, 64, 64*1024);
    if(_erst == nullptr){
      return MAKE_ERROR(Error::kNoEnoughMemory);
    }

    _erst[0].bits.ring_segment_base_address = reinterpret_cast<uint64_t>(_buf);
    _erst[0].bits.ring_segment_size = _buf_size;

    ERSTSZ_Bitmap erstsz = _interrupter->ERSTSZ.Read();
    erstsz.SetSize(1);
    _interrupter->ERSTSZ.Write(erstsz);

    WriteDequeuePointer(_buf);

    ERSTBA_Bitmap erstba = _interrupter->ERSTBA.Read();
    erstba.SetPointer(reinterpret_cast<uint64_t>(_erst));
    _interrupter->ERSTBA.Write(erstba);

    return MAKE_ERROR(Error::kSuccess);
  }

  void EventRing::Pop(){
    TRB* p = ReadDequeuePointer() + 1;
    
    TRB* segment_begin = reinterpret_cast<TRB*>(_erst[0].bits.ring_segment_base_address);
    TRB* segment_end = segment_begin + _erst[0].bits.ring_segment_size;

    if(p == segment_end){
      p = segment_begin;
      _cycle_bit = !_cycle_bit;
      Log(kDebugMass,"event_cycle_bit flip\n");
    }

    WriteDequeuePointer(p);
  }

  void EventRing::WriteDequeuePointer(TRB* p){
    ERDP_Bitmap erdp = _interrupter->ERDP.Read();
    erdp.SetPointer(reinterpret_cast<uint64_t>(p));
    _interrupter->ERDP.Write(erdp);
  }
}