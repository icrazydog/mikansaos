#pragma once

#include "error.hpp"

#include "usb/device.hpp"

namespace usb {
  class Device;
  
  class ClassDriver {
    public:
      ClassDriver(Device* dev);
      virtual ~ClassDriver();

      Device* ParentDevice() const { return _dev; }
      virtual Error SetEndpoint(const EndpointConfig& config) = 0;
      virtual Error OnEndpointsConfigured() = 0;
      virtual Error OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                                     const void* buf, int len) = 0;
      virtual Error OnInterruptCompleted(EndpointID ep_id, const void* buf, int len) = 0;

    private:
      Device* _dev;
  };
}