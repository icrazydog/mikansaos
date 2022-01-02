#pragma once

#include "usb/classdriver/base.hpp"
#include "usb/endpoint.h"

namespace usb {
  class HIDBaseDriver : public ClassDriver {
    public:
      HIDBaseDriver(Device* dev, int interface_index, int in_packet_size);

      Error SetEndpoint(const EndpointConfig& config) override;
      Error OnEndpointsConfigured() override;
      Error OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                             const void* buf, int len) override;
      Error OnInterruptCompleted(EndpointID ep_id, const void* buf, int len) override;

      virtual Error OnDataReceived() = 0;
      const static size_t kBufferSize = 1024;
      const std::array<uint8_t, kBufferSize>& Buffer() const { return _buf; }
      const std::array<uint8_t, kBufferSize>& PreviousBuffer() const { return _previous_buf; }
  
    private:
      EndpointID _ep_interrupt_in{0};
      EndpointID _ep_interrupt_out{0};
      const int _interface_index;
      int _in_packet_size;
      std::array<uint8_t, kBufferSize> _buf, _previous_buf;
      int _initialize_phase = 0;


  };
}