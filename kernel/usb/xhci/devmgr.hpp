#pragma once

#include <cstdint>

#include "error.hpp"

#include "usb/xhci/device.hpp"
#include "usb/xhci/context.hpp"
#include "usb/xhci/registers.hpp"

namespace usb::xhci{
  class DeviceManager {
    public:
      Error Initialize(size_t max_slots);
      DeviceContext** DeviceContexts() const{
        return _device_context_pointers;
      }
      Error AllocDevice(uint8_t slot_id, DoorbellRegister* dbreg);
      Device* FindBySlot(uint8_t slot_id);
      Device* FindByPort(uint8_t port_num) const;
      Error LoadDCBAA(uint8_t slot_id);

    private:
    size_t _max_slots;
    //elements count max_slots_ + 1.
    Device** _devices;
    DeviceContext** _device_context_pointers;
    


   
  };
}