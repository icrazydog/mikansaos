#pragma once

#include <cstdint>
#include <array>

#include "usb/xhci/context.hpp"

namespace usb::xhci{
    //6.4.5 Completion Code
    const std::array<const char*, 37> kTRBCompletionCodeToName{
    "Invalid",
    "Success",
    "Data Buffer Error",
    "Babble Detected Error",
    "USB Transaction Error",
    "TRB Error",
    "Stall Error",
    "Resource Error",
    "Bandwidth Error",
    "No Slots Available Error",
    "Invalid Stream Type Error",
    "Slot Not Enabled Error",
    "Endpoint Not Enabled Error",
    "Short Packet",
    "Ring Underrun",
    "Ring Overrun",
    "VF Event Ring Full Error",
    "Parameter Error",
    "Bandwidth Overrun Error",
    "Context State Error",
    "No ping Response Error",
    "Event Ring Full Error",
    "Incompatible Device Error",
    "Missed Service Error",
    "Command Ring Stopped",
    "Command Aborted",
    "Stopped",
    "Stopped - Length Invalid",
    "Stopped - Short Packet",
    "Max Exit Latency Too Large Error",
    "Reserved",
    "Isoch Buffer Overrun",
    "Event Lost Error",
    "Undefined Error",
    "Invalid Stream ID Error",
    "Secondary Bandwidth Error",
    "Split Transaction Error",
  };

  const std::array<const char*, 64> kTRBTypeToName{
    "Reserved",                             // 0
    "Normal",
    "Setup Stage",
    "Data Stage",
    "Status Stage",
    "Isoch",
    "Link",
    "EventData",
    "No-Op",                                // 8
    "Enable Slot Command",
    "Disable Slot Command",
    "Address Device Command",
    "Configure Endpoint Command",
    "Evaluate Context Command",
    "Reset Endpoint Command",
    "Stop Endpoint Command",
    "Set TR Dequeue Pointer Command",       // 16
    "Reset Device Command",
    "Force Event Command",
    "Negotiate Bandwidth Command",
    "Set Latency Tolerance Value Command",
    "Get Port Bandwidth Command",
    "Force Header Command",
    "No Op Command",
    "Reserved",                             // 24
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Transfer Event",                       // 32
    "Command Completion Event",
    "Port Status Change Event",
    "Bandwidth Request Event",
    "Doorbell Event",
    "Host Controller Event",
    "Device Notification Event",
    "MFINDEX Wrap Event",
    "Reserved",                             // 40
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Vendor Defined",                       // 48
    "Vendor Defined",
    "Vendor Defined",
    "Vendor Defined",
    "Vendor Defined",
    "Vendor Defined",
    "Vendor Defined",
    "Vendor Defined",
    "Vendor Defined",                       // 56
    "Vendor Defined",
    "Vendor Defined",
    "Vendor Defined",
    "Vendor Defined",
    "Vendor Defined",
    "Vendor Defined",
    "Vendor Defined",
  };

  union TRB{
      std::array<uint32_t, 4> data{};
      struct{
          uint64_t paramameter;
          uint32_t status;
          uint32_t cycle_bit :1;
          uint32_t evaluate_nex_trb :1;
          uint32_t :8;
          uint32_t trb_type :6;
          uint32_t control :16;
      }__attribute__((packed)) bits;
  };

  union LinkTRB{
      static const unsigned int Type = 6;
      std::array<uint32_t, 4> data{};
      struct{
          uint64_t  :4;
          uint64_t ring_segment_pointer :60;
          uint32_t  :22;
          uint32_t interrupter_target :10;
          uint32_t cycle_bit :1;
          uint32_t toggle_cycle :1;
          uint32_t  :2;
          uint32_t chain_bit :1;
          uint32_t innterrupt_on_completion :1;
          uint32_t  :4;
          uint32_t trb_type :6;
          uint32_t  :16;
      }__attribute__((packed)) bits;

      LinkTRB(const TRB* ring_segment_pointer){
          bits.trb_type = Type;
          SetPointer(ring_segment_pointer);
      }

      void SetPointer(const TRB* p){
          bits.ring_segment_pointer = reinterpret_cast<uint64_t>(p)  >> 4;
      }

      TRB* Pointer() const{
          return reinterpret_cast<TRB*>(bits.ring_segment_pointer << 4);
      }
  };

    union NormalTRB {
    static const unsigned int Type = 1;
    std::array<uint32_t, 4> data{};
    struct {
      uint64_t data_buffer_pointer;
      uint32_t trb_transfer_length :17;
      uint32_t td_size :5;
      uint32_t interrupter_target :10;
      uint32_t cycle_bit :1;
      uint32_t evaluate_next_trb :1;
      uint32_t interrupt_on_short_packet :1;
      uint32_t no_snoop :1;
      uint32_t chain_bit :1;
      uint32_t interrupt_on_completion :1;
      uint32_t immediate_data :1;
      uint32_t :2;
      uint32_t block_event_interrupt :1;
      uint32_t trb_type :6;
      uint32_t : 16;
    } __attribute__((packed)) bits;

    NormalTRB() {
      bits.trb_type = Type;
    }

    void* Pointer() const {
      return reinterpret_cast<TRB*>(bits.data_buffer_pointer);
    }

    void SetPointer(const void* p) {
      bits.data_buffer_pointer = reinterpret_cast<uint64_t>(p);
    }
  };


  union PortStatusChangeEventTRB {
    static const unsigned int Type = 34;
    std::array<uint32_t, 4> data{};
    struct {
      uint32_t  :24;
      uint32_t port_id :8;
      uint32_t  :32;
      uint32_t  :24;
      uint32_t completion_code :8;
      uint32_t cycle_bit :1;
      uint32_t  :9;
      uint32_t trb_type :6;
    } __attribute__((packed)) bits;

    PortStatusChangeEventTRB() {
      bits.trb_type = Type;
    }
  };


  union CommandCompletionEventTRB {
    static const unsigned int Type = 33;
    std::array<uint32_t, 4> data{};
    struct {
      uint64_t  :4;
      uint64_t command_trb_pointer :60;
      uint32_t command_completion_parameter :24;
      uint32_t completion_code :8;
      uint32_t cycle_bit :1;
      uint32_t  :9;
      uint32_t trb_type :6;
      uint32_t vf_id :8;
      uint32_t slot_id :8;
    } __attribute__((packed)) bits;

    CommandCompletionEventTRB() {
      bits.trb_type = Type;
    }

    TRB* Pointer() const {
      return reinterpret_cast<TRB*>(bits.command_trb_pointer << 4);
    }

    void SetPointer(TRB* p) {
      bits.command_trb_pointer = reinterpret_cast<uint64_t>(p) >> 4;
    }
  };

  union TransferEventTRB {
    static const unsigned int Type = 32;
    std::array<uint32_t, 4> data{};
    struct {
      uint64_t trb_pointer :64;
      uint32_t trb_transfer_length :24;
      uint32_t completion_code :8;
      uint32_t cycle_bit :1;
      uint32_t  :1;
      uint32_t event_data :1;
      uint32_t  :7;
      uint32_t trb_type :6;
      uint32_t endpoint_id :5;
      uint32_t  :3;
      uint32_t slot_id :8;
    } __attribute__((packed)) bits;

    TransferEventTRB() {
      bits.trb_type = Type;
    }

    TRB* Pointer() const {
      return reinterpret_cast<TRB*>(bits.trb_pointer);
    }

    void SetPointer(const TRB* p) {
      bits.trb_pointer = reinterpret_cast<uint64_t>(p);
    }

    EndpointID EndpointID() const {
      return usb::EndpointID{bits.endpoint_id};
    }
  };

  union EnableSlotCommandTRB {
    static const unsigned int Type = 9;
    std::array<uint32_t, 4> data{};
    struct {
      uint32_t  :32;
      uint32_t  :32;
      uint32_t  :32;
      uint32_t cycle_bit :1;
      uint32_t  :9;
      uint32_t trb_type :6;
      uint32_t slot_type :5;
      uint32_t  :11;
    } __attribute__((packed)) bits;

    EnableSlotCommandTRB() {
      bits.trb_type = Type;
    }
  };

  union AddressDeviceCommandTRB {
    static const unsigned int Type = 11;
    std::array<uint32_t, 4> data{};
    struct {
      uint64_t  :4;
      uint64_t input_context_pointer :60;
      uint32_t  :32;
      uint32_t cycle_bit :1;
      uint32_t  :8;
      uint32_t block_set_address_request :1;
      uint32_t trb_type :6;
      uint32_t  :8;
      uint32_t slot_id :8;
    } __attribute__((packed)) bits;

    AddressDeviceCommandTRB(const InputContext* input_context, uint8_t slot_id) {
      bits.trb_type = Type;
      bits.slot_id = slot_id;
      SetPointer(input_context);
    }

    InputContext* Pointer() const {
      return reinterpret_cast<InputContext*>(bits.input_context_pointer << 4);
    }

    void SetPointer(const InputContext* p) {
      bits.input_context_pointer = reinterpret_cast<uint64_t>(p) >> 4;
    }
  };

  union SetupStageTRB {
    static const unsigned int Type = 2;
    static const unsigned int kNoDataStage = 0;
    static const unsigned int kOutDataStage = 2;
    static const unsigned int kInDataStage = 3;

    std::array<uint32_t, 4> data{};
    struct {
      uint32_t request_type : 8;
      uint32_t request : 8;
      uint32_t value : 16;

      uint32_t index : 16;
      uint32_t length : 16;

      uint32_t trb_transfer_length : 17;
      uint32_t : 5;
      uint32_t interrupter_target : 10;

      uint32_t cycle_bit : 1;
      uint32_t : 4;
      uint32_t interrupt_on_completion : 1;
      uint32_t immediate_data : 1;
      uint32_t : 3;
      uint32_t trb_type : 6;
      uint32_t transfer_type : 2;
      uint32_t : 14;
    } __attribute__((packed)) bits;

    SetupStageTRB() {
      bits.trb_type = Type;
      bits.immediate_data = true;
      bits.trb_transfer_length = 8;
    }
  };

  union DataStageTRB {
    static const unsigned int Type = 3;
    std::array<uint32_t, 4> data{};
    struct {
      uint64_t data_buffer_pointer;

      uint32_t trb_transfer_length : 17;
      uint32_t td_size : 5;
      uint32_t interrupter_target : 10;

      uint32_t cycle_bit : 1;
      uint32_t evaluate_next_trb : 1;
      uint32_t interrupt_on_short_packet : 1;
      uint32_t no_snoop : 1;
      uint32_t chain_bit : 1;
      uint32_t interrupt_on_completion : 1;
      uint32_t immediate_data : 1;
      uint32_t : 3;
      uint32_t trb_type : 6;
      uint32_t direction : 1;
      uint32_t : 15;
    } __attribute__((packed)) bits;

    DataStageTRB() {
      bits.trb_type = Type;
    }

    void* Pointer() const {
      return reinterpret_cast<void*>(bits.data_buffer_pointer);
    }

    void SetPointer(const void* p) {
      bits.data_buffer_pointer = reinterpret_cast<uint64_t>(p);
    }
  };

  union StatusStageTRB {
    static const unsigned int Type = 4;
    std::array<uint32_t, 4> data{};
    struct {
      uint64_t : 64;

      uint32_t : 22;
      uint32_t interrupter_target : 10;

      uint32_t cycle_bit : 1;
      uint32_t evaluate_next_trb : 1;
      uint32_t : 2;
      uint32_t chain_bit : 1;
      uint32_t interrupt_on_completion : 1;
      uint32_t : 4;
      uint32_t trb_type : 6;
      uint32_t direction : 1;
      uint32_t : 15;
    } __attribute__((packed)) bits;

    StatusStageTRB() {
      bits.trb_type = Type;
    }
  };

  union ConfigureEndpointCommandTRB {
    static const unsigned int Type = 12;
    std::array<uint32_t, 4> data{};
    struct {
      uint64_t :4;
      uint64_t input_context_pointer :60;
      uint32_t  :32;
      uint32_t cycle_bit :1;
      uint32_t  :8;
      uint32_t deconfigure :1;
      uint32_t trb_type :6;
      uint32_t  :8;
      uint32_t slot_id :8;
    } __attribute__((packed)) bits;

    ConfigureEndpointCommandTRB(const InputContext* input_context, uint8_t slot_id) {
      bits.trb_type = Type;
      bits.slot_id = slot_id;
      SetPointer(input_context);
    }

    InputContext* Pointer() const {
      return reinterpret_cast<InputContext*>(bits.input_context_pointer << 4);
    }

    void SetPointer(const InputContext* p) {
      bits.input_context_pointer = reinterpret_cast<uint64_t>(p) >> 4;
    }
  };

  template<class ToType, class FromType>
  ToType* TRBDynamicCast(FromType* trb){
      if(ToType::Type == trb->bits.trb_type){
        return reinterpret_cast<ToType*>(trb);
      }
      return nullptr;
  }
  
}