/**
 * @file usb/xhci/port.cpp
 */

#include "logger.hpp"
#include "usb/xhci/port.hpp"

namespace usb::xhci{
  bool Port::IsConnected() const {
    return _port_reg_set->PORTSC.Read().bits.current_connect_status;
  }

  bool Port::IsEnabled() const {
    return _port_reg_set->PORTSC.Read().bits.port_enabled_disabled;
  }

  bool Port::IsConnectStatusChanged() const {
    return _port_reg_set->PORTSC.Read().bits.connect_status_change;
  }

  bool Port::IsPortResetChanged() const {
    return _port_reg_set->PORTSC.Read().bits.port_reset_change;
  }

  int Port::Speed() const {
    return _port_reg_set->PORTSC.Read().bits.port_speed;
  }

  ConfigPhase Port::GetConfigPhase() {
    return _config_phase;
  }

  void Port::SetConfigPhase(ConfigPhase configPhase) {
     _config_phase = configPhase;
  }


  Error Port::Reset(){
    auto portsc = _port_reg_set->PORTSC.Read();
    //keep:woe wde wce pic pp pls clear:others
    portsc.data[0] &= 0x0e00c3e0u;
    // Write 1 to PR and CSC
    //portsc.data[0] |= 0x00020010u;
    portsc.bits.port_reset = 1;
    portsc.bits.connect_status_change = 1;
  
    _port_reg_set->PORTSC.Write(portsc);

    while(_port_reg_set->PORTSC.Read().bits.port_reset);
    return MAKE_ERROR(Error::kSuccess);
  }

  void Port::ClearConnectStatusChanged(){
    auto portsc = _port_reg_set->PORTSC.Read();
    //keep:woe wde wce lws pic pp pls clear:others
    portsc.data[0] &= 0x0e01c3e0u;
    portsc.bits.connect_status_change = 1;
    _port_reg_set->PORTSC.Write(portsc);
  }

  void Port::ClearPortResetChange(){
    auto portsc = _port_reg_set->PORTSC.Read();
    //keep:woe wde wce lws pic pp pls clear:others
    portsc.data[0] &= 0x0e01c3e0u;
    portsc.bits.port_reset_change = 1;
    _port_reg_set->PORTSC.Write(portsc);
  }
}