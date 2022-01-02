#pragma once

#include "error.hpp"

namespace usb {
  enum class EndpointType {
    kControl = 0,
    kIsochronous = 1,
    kBulk = 2,
    kInterrupt = 3,
  };

  class EndpointID {
    public:
      constexpr EndpointID() : _dci{0} {}
      explicit constexpr EndpointID(int dci) : _dci{dci} {}
      constexpr EndpointID(int ep_num, bool dir_in) : _dci{ep_num << 1 | dir_in} {}

      //0-31
      int Dci() const { return _dci; }

      //0-15
      int Number() const { return _dci >> 1; }

      //in 1 out 0
      bool IsDirIn() const { return _dci & 1; }

    
    private:
      int _dci;
  };

    constexpr EndpointID kDefaultControlEpId{0, true};

    struct EndpointConfig{
      EndpointID ep_id;
      EndpointType ep_type;

      int max_packet_size;
      
      //125*2^(interval-1) ms
      int interval;
    };
}