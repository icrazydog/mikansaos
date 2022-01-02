#include "usb/xhci/device.hpp"

#include <cstdint>
#include "usb/xhci/registers.hpp"
#include "usb/xhci/ring.hpp"
#include "trb.hpp"
#include "usb/setupdata.hpp"
#include "logger.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/classdriver/keyboard.hpp"

namespace {
  using namespace usb::xhci;
  class ConfigurationDescriptorReader {
    public:
    ConfigurationDescriptorReader(const uint8_t* desc_buf, int len)
      : _desc_buf{desc_buf},
        _desc_buf_len{len},
        _p{desc_buf} {
    }

    const uint8_t* Next() {
      _p += _p[0];
      if (_p < _desc_buf + _desc_buf_len) {
        return _p;
      }
      return nullptr;
    }

    template <class T>
    const T* Next() {
      while (auto n = Next()) {
        if (auto d = usb::DescriptorDynamicCast<T>(n)) {
          return d;
        }
      }
      return nullptr;
    }

   private:
    const uint8_t* const _desc_buf;
    const int _desc_buf_len;
    const uint8_t* _p;
  };

  usb::ClassDriver* FindClassDriver(usb::Device* dev, const usb::InterfaceDescriptor& if_desc){
    if (if_desc.interface_class == 3 &&
        if_desc.interface_sub_class == 1) { 
      //HID
      if (if_desc.interface_protocol == 1) { 
        // keyboard
        auto keyboard_driver = new usb::HIDKeyboardDriver{dev, if_desc.interface_number};
        if (usb::HIDKeyboardDriver::default_observer) {
          keyboard_driver->SubscribeKeyPush(usb::HIDKeyboardDriver::default_observer);
        }
        return keyboard_driver;
      } else if (if_desc.interface_protocol == 2) {  // mouse
        auto mouse_driver = new usb::HIDMouseDriver{dev, if_desc.interface_number};
        if (usb::HIDMouseDriver::default_observer) {
          mouse_driver->SubscribeMouseMove(usb::HIDMouseDriver::default_observer);
        }
        return mouse_driver;
      }
    }
    return nullptr;
  }

  void Log(LogLevel level, const TransferEventTRB& trb) {
    if (trb.bits.event_data) {
      Log(level,
          "Transfer (value %08lx) completed: %s, residual length %d, slot %d, ep dci %d\n",
          reinterpret_cast<uint64_t>(trb.Pointer()),
          kTRBCompletionCodeToName[trb.bits.completion_code],
          trb.bits.trb_transfer_length,
          trb.bits.slot_id,
          trb.EndpointID().Dci());
      return;
    }

    TRB* issuer_trb = trb.Pointer();
    Log(level,
        "%s completed: %s, residual length %d, slot %d, ep dci %d\n",
        kTRBTypeToName[issuer_trb->bits.trb_type],
        kTRBCompletionCodeToName[trb.bits.completion_code],
        trb.bits.trb_transfer_length,
        trb.bits.slot_id,
        trb.EndpointID().Dci());
    if (auto data_trb = TRBDynamicCast<DataStageTRB>(issuer_trb)) {
      Log(level, "  ");
      Log(level,
        "DataStageTRB: len %d, buf 0x%08lx, dir %d, attr 0x%02x\n",
        data_trb->bits.trb_transfer_length,
        data_trb->bits.data_buffer_pointer,
        data_trb->bits.direction,
        data_trb->data[3] & 0x7fu);
    } else if (auto setup_trb = TRBDynamicCast<SetupStageTRB>(issuer_trb)) {
      Log(level, "  ");
      Log(level,
        "  SetupStage TRB: req_type %02x, req %02x, val %02x, ind %02x, len %02x\n",
        setup_trb->bits.request_type,
        setup_trb->bits.request,
        setup_trb->bits.value,
        setup_trb->bits.index,
        setup_trb->bits.length);
    }
  }
}
namespace usb::xhci {
  Device::Device(uint8_t slot_id, DoorbellRegister* dbreg)
      : _slot_id{slot_id}, _dbreg{dbreg} {
  }

  Ring* Device::AllocTransferRing(EndpointID ep, size_t buf_size) {
    auto tr = AllocArray<Ring>(1);
    if (tr) {
      tr->Initialize(buf_size);
    }
    _transfer_rings[ep.Dci() -1] = tr;
    return tr;
  }

  Error Device::ControlIn(EndpointID ep_id, SetupData setup_data,
                            void* buf, int len, ClassDriver* issuer) {
    if (ep_id.Number() < 0 || 15 < ep_id.Number()) {
      return MAKE_ERROR(Error::kInvalidEndpointNumber);
    }
    Log(kDebug, "Device::ControlIn: ep addr %d, buf 0x%08x, len %d\n",
        ep_id.Dci(), buf, len);
    
    if (issuer) {
      _event_waiters.Put(setup_data, issuer);
    }
    
    Ring* tr = _transfer_rings[ep_id.Dci() - 1];

    if(tr == nullptr){
      return MAKE_ERROR(Error::kTransferRingNotSet);
    }

    SetupStageTRB setup_stage;
    setup_stage.bits.request_type = setup_data.request_type.data;
    setup_stage.bits.request = setup_data.request;
    setup_stage.bits.value = setup_data.value;
    setup_stage.bits.index = setup_data.index;
    setup_stage.bits.length = setup_data.length;
    setup_stage.bits.transfer_type = SetupStageTRB::kInDataStage;
    auto setup_trb_position = TRBDynamicCast<SetupStageTRB>(tr->Push(setup_stage));

    DataStageTRB data_stage;
    data_stage.SetPointer(buf);
    data_stage.bits.trb_transfer_length = len;
    data_stage.bits.td_size = 0;
    data_stage.bits.direction = 1;
    data_stage.bits.interrupt_on_completion = true;
    auto data_trb_position = TRBDynamicCast<DataStageTRB>(tr->Push(data_stage));

    StatusStageTRB status_stage;
    tr->Push(status_stage);

    _setup_stage_map.Put(data_trb_position, setup_trb_position);
    _dbreg->Ring(ep_id.Dci());
    
    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::ControlOut(EndpointID ep_id, SetupData setup_data,
                            const void* buf, int len, ClassDriver* issuer) {
    if (ep_id.Number() < 0 || 15 < ep_id.Number()) {
      return MAKE_ERROR(Error::kInvalidEndpointNumber);
    }

    Log(kDebug, "Device::ControlOut: ep addr %d, buf 0x%08x, len %d\n",
        ep_id.Dci(), buf, len);
    
    if (issuer) {
      _event_waiters.Put(setup_data, issuer);
    }

    Ring* tr = _transfer_rings[ep_id.Dci() - 1];

    if (tr == nullptr) {
      return MAKE_ERROR(Error::kTransferRingNotSet);
    }

    SetupStageTRB setup_stage;
    setup_stage.bits.request_type = setup_data.request_type.data;
    setup_stage.bits.request = setup_data.request;
    setup_stage.bits.value = setup_data.value;
    setup_stage.bits.index = setup_data.index;
    setup_stage.bits.length = setup_data.length;
    setup_stage.bits.transfer_type = SetupStageTRB::kNoDataStage;
    auto setup_trb_position = TRBDynamicCast<SetupStageTRB>(tr->Push(setup_stage));

    StatusStageTRB status_stage;
    status_stage.bits.direction = true;
    status_stage.bits.interrupt_on_completion = true;
    auto status_trb_position = tr->Push(status_stage);

    _setup_stage_map.Put(status_trb_position, setup_trb_position);
    _dbreg->Ring(ep_id.Dci());
    
    return MAKE_ERROR(Error::kSuccess);

  }

  Error Device::InterruptIn(EndpointID ep_id, void* buf, int len) {
    // Log(kDebug, "Device::InterruptIn: slotid:%d ep addr %d, buf 0x%08x, len %d\n",
    //    ep_id.Dci(), _slot_id, buf, len);
    Ring* tr = _transfer_rings[ep_id.Dci() - 1];

    if (tr == nullptr) {
      return MAKE_ERROR(Error::kTransferRingNotSet);
    }

    NormalTRB normal{};
    normal.SetPointer(buf);
    normal.bits.trb_transfer_length = len;
    normal.bits.interrupt_on_short_packet = true;
    normal.bits.interrupt_on_completion = true;

    tr->Push(normal);
    _dbreg->Ring(ep_id.Dci());
    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::OnTransferEventReceived(const TransferEventTRB& trb) {
    Log(kDebugMass, trb);

    // 1 Success Short Packet
    if (trb.bits.completion_code != 1  &&
        trb.bits.completion_code != 13) {
      return MAKE_ERROR(Error::kTransferFailed);
    }

    TRB* issuer_trb = trb.Pointer();
    if(auto normal_trb = TRBDynamicCast<NormalTRB>(issuer_trb)){
      const auto transfer_length = 
          normal_trb->bits.trb_transfer_length - trb.bits.trb_transfer_length;
      return OnInterruptCompleted(trb.EndpointID(), normal_trb->Pointer(), transfer_length);
    }

    auto opt_setup_stage_trb = _setup_stage_map.Get(issuer_trb);
    if (!opt_setup_stage_trb) {
      Log(kDebug, "No Corresponding Setup Stage for issuer %s\n",
          kTRBTypeToName[issuer_trb->bits.trb_type]);
      return MAKE_ERROR(Error::kNoCorrespondingSetupStage);
    }
    _setup_stage_map.Delete(issuer_trb);

    auto setup_stage_trb = opt_setup_stage_trb.value();
    SetupData setup_data{};
    setup_data.request_type.data = setup_stage_trb->bits.request_type;
    setup_data.request = setup_stage_trb->bits.request;
    setup_data.value = setup_stage_trb->bits.value;
    setup_data.index = setup_stage_trb->bits.index;
    setup_data.length = setup_stage_trb->bits.length;

    void* data_stage_buffer = nullptr;
    int transfer_length = 0;
    if (auto data_stage_trb = TRBDynamicCast<DataStageTRB>(issuer_trb)) {
      data_stage_buffer = data_stage_trb->Pointer();
      transfer_length =
        data_stage_trb->bits.trb_transfer_length - trb.bits.trb_transfer_length;
    } else if (auto status_stage_trb = TRBDynamicCast<StatusStageTRB>(issuer_trb)) {
      // pass
    } else {
      return MAKE_ERROR(Error::kNotImplemented);
    }
    

    return this->OnControlCompleted(
        trb.EndpointID(), setup_data, data_stage_buffer, transfer_length);
  }

  Error Device::OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                                   const void* buf, int len) {
    Log(kDebug, "Device::OnControlCompleted: buf 0x%08x, len %d, dir %d\n",
        buf, len, setup_data.request_type.bits.direction);
    if (_is_initialized) {
      if (auto w = _event_waiters.Get(setup_data)) {
        return w.value()->OnControlCompleted(ep_id, setup_data, buf, len);
      }
      return MAKE_ERROR(Error::kNoWaiter);
    }

    const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(buf);
    if(_initialize_phase == 1){
      if(setup_data.request == request::kGetDescriptor &&
          DescriptorDynamicCast<DeviceDescriptor>(buf8)){
        return InitializePhase2(buf8, len); 
      }
      return MAKE_ERROR(Error::kInvalidPhase);
    }else if(_initialize_phase == 2){
      if(setup_data.request == request::kGetDescriptor &&
          DescriptorDynamicCast<ConfigurationDescriptor>(buf8)){
        return InitializePhase3(buf8, len); 
      }
      return MAKE_ERROR(Error::kInvalidPhase);
    }else if(_initialize_phase == 3){
      if(setup_data.request == request::kSetConfiguration){
        return InitializePhase4(setup_data.value & 0xffu); 
      }
      return MAKE_ERROR(Error::kInvalidPhase);
    }
    return MAKE_ERROR(Error::kNotImplemented);
  }

  Error Device::OnInterruptCompleted(EndpointID ep_id, const void* buf, int len) {
    Log(kDebugMass, "Device::OnInterruptCompleted: ep addr %d\n", ep_id.Dci());
    if (auto w = _class_drivers[ep_id.Number()]) {
      return w->OnInterruptCompleted(ep_id, buf, len);
    }
    return MAKE_ERROR(Error::kNoWaiter);
  }

  Error Device::OnEndpointsConfigured(){
    for(auto class_driver : _class_drivers){
      if(class_driver != nullptr){
        if(auto err = class_driver->OnEndpointsConfigured()){
          return err;
        }
      }
    }
    return MAKE_ERROR(Error::kSuccess);
  }
   
  Error Device::StartInitialize(){
    _is_initialized = false;
    _initialize_phase = 1;

    return GetDescriptor(kDefaultControlEpId, DeviceDescriptor::kType, _desc_index,
                        _buf.data(),_buf.size()); 
  }

  Error Device::InitializePhase2(const uint8_t* buf, int len){
  _initialize_phase = 2;

  auto device_desc = DescriptorDynamicCast<DeviceDescriptor>(buf);
  if (device_desc == nullptr) {
      return MAKE_ERROR(Error::kInvalidDescriptor);
  }
  
  Log(kDebug, "issuing GetDesc(Config): index=%d configurations=%d)\n", _desc_index,
      device_desc->num_configurations);

  return GetDescriptor(kDefaultControlEpId, ConfigurationDescriptor::kType, _desc_index,
                        _buf.data(),_buf.size()); 
  }

  Error Device::InitializePhase3(const uint8_t* buf, int len){
    _initialize_phase = 3;

    auto conf_desc = DescriptorDynamicCast<ConfigurationDescriptor>(buf);
    if(conf_desc == nullptr){
      return MAKE_ERROR(Error::kInvalidDescriptor);
    }    

    //find class driver and set endpoint for class driver
    ClassDriver* class_driver = nullptr;
    ConfigurationDescriptorReader config_reader{buf, len};

    int if_index = 0;
    while(auto if_desc = config_reader.Next<InterfaceDescriptor>()){
      Log(kDebug, "Interface Descriptor: class=%d, sub=%d, protocol=%d\n",
      if_desc->interface_class,
      if_desc->interface_sub_class,
      if_desc->interface_protocol);

      if(if_index<3){
        _interfaces[if_index++] = *if_desc;
      }

      class_driver = FindClassDriver(this, *if_desc);
      if(class_driver==nullptr){
        continue;
      }

      _ep_configs_len = 0;
      while(_ep_configs_len< if_desc->num_endpoints){
        auto desc = config_reader.Next();
        if(desc==nullptr){
          //_ep_configs_len error
            Log(kError, "EndpointConf: inited_count=%d, total_endpoints=%d\n",
            _ep_configs_len, if_desc->num_endpoints);
            class_driver=nullptr;
          break;
        }

        if(auto ep_desc = DescriptorDynamicCast<EndpointDescriptor>(desc)){
          EndpointConfig ep_conf;
          ep_conf.ep_id = EndpointID{
            ep_desc->endpoint_address.bits.number,
            ep_desc->endpoint_address.bits.dir_in == 1};
          ep_conf.ep_type = static_cast<usb::EndpointType>(ep_desc->attributes.bits.transfer_type);
          ep_conf.max_packet_size = ep_desc->max_packet_size;
          ep_conf.interval = ep_desc->interval;

          Log(kDebug, "EndpointConf: ep_id=%d, ep_type=%d"
              ", max_packet_size=%d, interval=%d\n",
              ep_conf.ep_id.Dci(), ep_conf.ep_type,
              ep_conf.max_packet_size, ep_conf.interval);
          
          _ep_configs[_ep_configs_len++] = ep_conf;
          _class_drivers[ep_conf.ep_id.Number()] = class_driver;
        }else if(auto hid_desc = DescriptorDynamicCast<HIDDescriptor>(desc)){
          Log(kDebug, "HID Descriptor: release=0x%02x, num_desc=%d",
              hid_desc->hid_release,
              hid_desc->num_descriptors);
          for (int i = 0; i < hid_desc->num_descriptors; ++i) {
            Log(kDebug, ", desc_type=%d, len=%d",
                hid_desc->GetClassDescriptor(i)->descriptor_type,
                hid_desc->GetClassDescriptor(i)->descriptor_length);
          }
          Log(kDebug, "\n");
        }
      }
      break;
    }

    if (!class_driver) {
      Log(kDebug, "no deriver slot=%d\n",_slot_id);
      return MAKE_ERROR(Error::kSuccess);
    }

    Log(kDebug, "issuing SetConfiguration: conf_val=%d slot=%d\n",
    conf_desc->configuration_value, _slot_id);
    return SetConfiguration(kDefaultControlEpId, conf_desc->configuration_value);
  }

  Error Device::InitializePhase4(uint8_t config_value){
    _initialize_phase = 4;

    for(int i=0; i< _ep_configs_len; i++){
      _class_drivers[_ep_configs[i].ep_id.Number()]->SetEndpoint(_ep_configs[i]);
    }
    
    _is_initialized = true;
    return MAKE_ERROR(Error::kSuccess);
  }

  Error Device::GetDescriptor(EndpointID ep_id,
                              uint8_t desc_type, uint8_t desc_index,
                              void* buf, int len){
    SetupData setup_data;
    setup_data.request_type.bits.direction = request_type::kIn;
    setup_data.request_type.bits.type = request_type::kStandard;
    setup_data.request_type.bits.recipient = request_type::kDevice;
    setup_data.request = request::kGetDescriptor;
    setup_data.value = (static_cast<uint16_t>(desc_type)<<8) | desc_index;
    //language
    setup_data.index = 0;
    setup_data.length = len;
    return ControlIn(ep_id, setup_data, buf, len, nullptr);
  }

  Error Device::SetConfiguration(EndpointID ep_id,
                                  uint8_t config_value){
    SetupData setup_data{};
    setup_data.request_type.bits.direction = request_type::kOut;
    setup_data.request_type.bits.type = request_type::kStandard;
    setup_data.request_type.bits.recipient = request_type::kDevice;
    setup_data.request = request::kSetConfiguration;
    setup_data.value = config_value;
    setup_data.index = 0;
    setup_data.length = 0;
    return ControlOut(ep_id, setup_data, nullptr, 0, nullptr);
  }


}