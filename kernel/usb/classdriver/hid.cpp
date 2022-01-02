#include "usb/classdriver/hid.hpp"

#include <algorithm>

#include "logger.hpp"
namespace usb {
  HIDBaseDriver::HIDBaseDriver(Device* dev, int interface_index, int in_packet_size)
      : ClassDriver{dev}, _interface_index{interface_index},_in_packet_size{in_packet_size} {
  }

  Error HIDBaseDriver::SetEndpoint(const EndpointConfig& config){
    Log(kDebug, "HIDBaseDriver::SetEndpoint: slot:%d\n",
        ParentDevice()->SlotID());
    if(config.ep_type == EndpointType::kInterrupt && config.ep_id.IsDirIn()){
      _ep_interrupt_in = config.ep_id;
    }else if(config.ep_type == EndpointType::kInterrupt && !config.ep_id.IsDirIn()){
      _ep_interrupt_out = config.ep_id;
    }
     return MAKE_ERROR(Error::kSuccess);
  }

  Error HIDBaseDriver::OnEndpointsConfigured() {
    SetupData setup_data{};
    setup_data.request_type.bits.direction = request_type::kOut;
    setup_data.request_type.bits.type = request_type::kClass;
    setup_data.request_type.bits.recipient = request_type::kInterface;
    setup_data.request = request::kSetProtocol;
    //boot
    setup_data.value = 0;
    setup_data.index = _interface_index;
    setup_data.length = 0;

    _initialize_phase = 1;
    return ParentDevice()->ControlOut(kDefaultControlEpId, setup_data, nullptr, 0 , this);
  }

  Error HIDBaseDriver::OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                                          const void* buf, int len) {
    Log(kDebug, "HIDBaseDriver::OnControlCompleted: slot:%d dev %08x, phase = %d, len = %d\n",
        ParentDevice()->SlotID(),this, _initialize_phase, len);
    if (_initialize_phase == 1) {
      _initialize_phase = 2;
      return ParentDevice()->InterruptIn(_ep_interrupt_in, _buf.data(), _in_packet_size);
    }
    return MAKE_ERROR(Error::kInvalidPhase);
  }
  Error HIDBaseDriver::OnInterruptCompleted(EndpointID ep_id, const void* buf, int len) {
    if (ep_id.IsDirIn()) {
      OnDataReceived();
      std::copy_n(_buf.begin(), len, _previous_buf.begin());
      return ParentDevice()->InterruptIn(_ep_interrupt_in, _buf.data(), _in_packet_size);
    }

    return MAKE_ERROR(Error::kNotImplemented);
  }
}