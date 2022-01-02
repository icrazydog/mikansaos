#pragma once

#include <cstdint>

#include "error.hpp"
#include "usb/xhci/registers.hpp"

namespace usb::xhci{
    enum class ConfigPhase {
    kNotConnected,
    kWaitingAddressed,
    kResettingPort,
    kEnablingSlot,
    kAddressingDevice,
    kInitializingDevice,
    kConfiguringEndpoints,
    kConfigured,
  };

  class Port{
    public:
      Port()  = default;
      // Port(uint8_t port_num, PortRegisterSet& port_reg_set)
      //     :_port_num{port_num}, _port_reg_set{port_reg_set}{
      // }

      void set(uint8_t port_num, PortRegisterSet& port_reg_set){
        _port_num = port_num;
        _port_reg_set = &port_reg_set;
      }

      uint8_t Number() const { return _port_num; }

      bool IsConnected() const;
      bool IsEnabled() const;
      bool IsConnectStatusChanged() const;
      bool IsPortResetChanged() const;
      int Speed() const;
      Error Reset();
      void ClearConnectStatusChanged();
      void ClearPortResetChange();

      ConfigPhase GetConfigPhase();
      void SetConfigPhase(ConfigPhase configPhase);

    private:
      uint8_t _port_num;
      PortRegisterSet* _port_reg_set;
      ConfigPhase _config_phase = ConfigPhase::kNotConnected;
  };
}