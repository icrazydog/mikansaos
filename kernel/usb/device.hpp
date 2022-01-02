#pragma once

#include "error.hpp"
#include "usb/endpoint.h"
#include "usb/setupdata.hpp"

namespace usb {
  class ClassDriver;

  class Device {
   public:

    virtual uint8_t SlotID() const =0;
    virtual Error ControlIn(EndpointID ep_id, SetupData setup_data,
                            void* buf, int len, ClassDriver* issuer);
    virtual Error ControlOut(EndpointID ep_id, SetupData setup_data,
                             const void* buf, int len, ClassDriver* issuer);
    virtual Error InterruptIn(EndpointID ep_id, void* buf, int len);
  };
}