#pragma once

#include "usb/device.hpp"
#include "usb/classdriver/base.hpp"
#include "usb/xhci/ring.hpp"
#include "usb/arraymap.hpp"
#include "usb/descriptor.hpp"

namespace usb::xhci {
  class Device : public usb::Device {
    public:
      Device(uint8_t slot_id, DoorbellRegister* dbreg);

      DeviceContext* DeviceContext() { return &_dev_ctx; }
      InputContext* InputContext() { return &_input_ctx; }

      uint8_t SlotID() const override { return _slot_id; }

      Ring* AllocTransferRing(EndpointID ep, size_t buf_size);

      Error ControlIn(EndpointID ep_id, SetupData setup_data,
                    void* buf, int len, ClassDriver* issuer) override;
      Error ControlOut(EndpointID ep_id, SetupData setup_data,
                     const void* buf, int len, ClassDriver* issuer) override;
      Error InterruptIn(EndpointID ep_id, void* buf, int len) override;

      Error OnTransferEventReceived(const TransferEventTRB& trb);

      Error StartInitialize();
      bool IsInitialized() { return _is_initialized; }
      EndpointConfig* EndpointConfigs() { return _ep_configs.data(); }
      int EndpointConfigsLen() { return _ep_configs_len; }
      Error OnEndpointsConfigured();

      InterfaceDescriptor* Interfaces() { return &_interfaces[0]; }

    private:
      alignas(64) struct DeviceContext _dev_ctx;
      alignas(64) struct InputContext _input_ctx;
      const uint8_t _slot_id;
      DoorbellRegister* const _dbreg;

      std::array<Ring*, 31> _transfer_rings;

       ArrayMap<const void*, const SetupStageTRB*, 16> _setup_stage_map{};

      //1-15 endpoint,class deriver can not use 0 endpoint
      std::array<ClassDriver*, 16> _class_drivers{};
      std::array<uint8_t, 256> _buf{};
      std::array<InterfaceDescriptor, 3> _interfaces{};


      bool _is_initialized = false;
      int _initialize_phase = 0;
      std::array<EndpointConfig, 16> _ep_configs;
      int _ep_configs_len;
      Error InitializePhase2(const uint8_t* buf, int len);
      Error InitializePhase3(const uint8_t* buf, int len);
      Error InitializePhase4(uint8_t config_value);

      ArrayMap<SetupData, ClassDriver*, 4> _event_waiters{};

      int _desc_index = 0;
      Error GetDescriptor(EndpointID ep_id,
                        uint8_t desc_type, uint8_t desc_index,
                        void* buf, int len);
      Error SetConfiguration(EndpointID ep_id,
        uint8_t config_value);

      Error OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                             const void* buf, int len);
      Error OnInterruptCompleted(EndpointID ep_id, const void* buf, int len);
  };

}