#pragma once

#include "error.hpp"
#include "usb/xhci/registers.hpp"
#include "usb/xhci/ring.hpp"
#include "usb/xhci/port.hpp"
#include "usb/xhci/devmgr.hpp"

namespace usb::xhci{
  enum PortSpeed{
    kFullSpeed = 1,
    kLowSpeed = 2,
    kHighSpeed = 3,
    kSuperSpeed = 4,
    kSuperSpeedPlus = 5,
  };


  class Controller {
    public:
      Controller(uintptr_t mmio_base);
      Error Initialize();
      Error Run();
      Ring* CommandRing(){ return &_cr; }
      EventRing* PrimaryEventRing(){ return &_er; }
      DoorbellRegister* DoorbellRegisterAt(uint8_t index);
      Port& PortAt(uint8_t port_num){
        if(_ports[port_num - 1] == nullptr){
          Log(kDebug,"new PortAt:%d\n",port_num);
          _ports[port_num - 1] = &_ports_buf[port_num - 1];
          _ports[port_num - 1]->set(port_num, PortRegisterSets()[port_num - 1]);
        }

        return *_ports[port_num - 1];
      }
      uint8_t MaxPorts() const { return _max_ports; }
      DeviceManager* DeviceManager(){ return &_devmgr; }

    private:
      static const size_t kDeviceSize = 8;

      const uintptr_t _mmio_base;
      CapabilityRegisters* const _cp;
      OperationalRegisters* const _op;
      const uint8_t _max_ports;

      class DeviceManager _devmgr;
      Ring _cr;
      EventRing _er;
      std::array<Port*, 256> _ports {nullptr};
      Port _ports_buf[256];

      InterrupterRegisterSetArray InterrupterRegisterSets() const{
        return {_mmio_base + _cp->RTSOFF.Read().Offset() + 0x20u, 1024};
      }

      PortRegisterSetArray PortRegisterSets() const{
        return {reinterpret_cast<uintptr_t>(_op) + 0x400u, _max_ports};
      }

      DoorbellRegisterArray DoorbellRegisters() const{
        return {_mmio_base + _cp->DBOFF.Read().Offset(), 256};
      }
  };

  Error ConfigurePort(Controller& xhc, Port& port);
  Error ConfigureEndPoints(Controller& xhc, Device& dev);

  Error ProcessEvent(Controller& xhc);

  extern Controller* xhc;
  void Initialize();
  void ProcessEvents();
}
