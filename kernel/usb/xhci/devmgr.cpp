/**
 * @file usb/xhci/devmgr.cpp
 */

#include "usb/xhci/devmgr.hpp"

#include "usb/memory.hpp"

namespace usb::xhci{
  Error DeviceManager::Initialize(size_t max_slots) {
    _max_slots = max_slots;

    _devices = AllocArray<Device*>(_max_slots+1, 0, 0);
    if(_devices == nullptr){
      return MAKE_ERROR(Error::kNoEnoughMemory);
    }

    _device_context_pointers = AllocArray<DeviceContext*>(_max_slots+1);
    if(_device_context_pointers == nullptr){
       FreeMem(_devices);
      return MAKE_ERROR(Error::kNoEnoughMemory);
    }

    return MAKE_ERROR(Error::kSuccess);
  }

  Error DeviceManager::AllocDevice(uint8_t slot_id, DoorbellRegister* dbreg) {
    if(slot_id > _max_slots){
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    if(_devices[slot_id] != nullptr){
      return MAKE_ERROR(Error::kAlreadyAllocated); 
    }

    _devices[slot_id] = AllocArray<Device>(1);
    new(_devices[slot_id]) Device(slot_id, dbreg);

    return MAKE_ERROR(Error::kSuccess);
  }

  Device* DeviceManager::FindBySlot(uint8_t slot_id){
    if(slot_id > _max_slots){
      return nullptr;
    }

    return _devices[slot_id];
  }

    Device* DeviceManager::FindByPort(uint8_t port_num) const {
    for (size_t i = 1; i <= _max_slots; ++i) {
      auto dev = _devices[i];
      if (dev == nullptr) continue;
      if (dev->DeviceContext()->slot_context.bits.root_hub_port_num == port_num) {
        return dev;
      }
    }
    return nullptr;
  }

  Error DeviceManager::LoadDCBAA(uint8_t slot_id) {
    if (slot_id > _max_slots) {
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    auto dev = _devices[slot_id];
    _device_context_pointers[slot_id] = dev->DeviceContext();
    return MAKE_ERROR(Error::kSuccess);
  }
}