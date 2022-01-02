/**
 * @file usb/xhci/xhci.cpp
 */

#include "usb/xhci/xhci.hpp"

#include <functional>

#include "logger.hpp"
#include "delay.h"
#include "usb/xhci/trb.hpp"
#include "pci.hpp"
#include "interrupt.hpp"

namespace{
  using namespace usb::xhci;

  void RequestHCOwnership(uintptr_t mmio_base, HCCPARAMS1_Bitmap hccp){

    if(hccp.bits.xhci_extended_capabilities_pointer==0){
      return;
    }
    
    MemMapRegister<USBLEGSUP_Bitmap>* ext_usblegsup = nullptr;

    
 
    auto ext_reg = reinterpret_cast<MemMapRegister<USBLEGSUP_Bitmap>*>(mmio_base+hccp.bits.xhci_extended_capabilities_pointer);
    while(ext_reg){
      auto ext_reg_bitmap = ext_reg->Read();
      if(ext_reg_bitmap.bits.capability_id == 1){
        ext_usblegsup = ext_reg;
        break;
      }
      if(ext_reg_bitmap.bits.next_pointer == 0){
        ext_reg = nullptr;
      }else{
        ext_reg += ext_reg_bitmap.bits.next_pointer << 2;
      }
    }


    if(ext_usblegsup==nullptr){
      return;
    }

    auto ext_reg_bitmap = ext_usblegsup->Read();
    if (ext_reg_bitmap.bits.hc_os_owned_semaphore) {
      return;
    }

    ext_reg_bitmap.bits.hc_os_owned_semaphore = 1;
    Log(kDebug, "waiting until OS owns xHC\n");
    ext_usblegsup->Write(ext_reg_bitmap);

    do {
      ext_reg_bitmap = ext_usblegsup->Read();
    } while (ext_reg_bitmap.bits.hc_bios_owned_semaphore ||
             !ext_reg_bitmap.bits.hc_os_owned_semaphore);
    Log(kDebug, "OS has owned xHC\n");
  }

  uint8_t addressing_port{0};

  Error ResetPort(Controller& xhc, Port& port) {
    const bool is_connected = port.IsConnected();
    Log(kDebug, "ResetPort: port.IsConnected() = %s\n",
        is_connected ? "true" : "false");

    if (!is_connected) {
      return MAKE_ERROR(Error::kSuccess);
    }

     Log(kDebug, "addressing_port:%d\n",addressing_port);
    if (addressing_port != 0) {
      port.SetConfigPhase(ConfigPhase::kWaitingAddressed);
    } else {
      const auto port_phase = port.GetConfigPhase();
      Log(kDebug, "port_phase:%d\n",port_phase);
      if (port_phase != ConfigPhase::kNotConnected &&
          port_phase != ConfigPhase::kWaitingAddressed) {
        return MAKE_ERROR(Error::kInvalidPhase);
      }
     
      addressing_port = port.Number();
      port.SetConfigPhase(ConfigPhase::kResettingPort);
      port.Reset();
      Log(kDebug, "Reseted Port:%d phase:%d\n",addressing_port, 
          xhc.PortAt(addressing_port).GetConfigPhase());
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error EnableSlot(Controller& xhc, Port& port) {
    const bool is_enabled = port.IsEnabled();
    const bool reset_completed = port.IsPortResetChanged();
    Log(kDebug, "EnableSlot: port.IsEnabled() = %s, port.IsPortResetChanged() = %s\n",
        is_enabled ? "true" : "false",
        reset_completed ? "true" : "false");

    if(is_enabled && reset_completed){
        port.ClearPortResetChange();

        port.SetConfigPhase(ConfigPhase::kEnablingSlot);

        EnableSlotCommandTRB cmd{};
        xhc.CommandRing()->Push(cmd);
        xhc.DoorbellRegisterAt(0)->Ring(0);
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error AddressDevice(Controller& xhc, uint8_t port_id, uint8_t slot_id){
    Log(kDebug, "AddressDevice: port_id = %d, slot_id = %d\n", port_id, slot_id);


  Log(kDebug, "DoorbellRegister:%08lx\n",xhc.DoorbellRegisterAt(slot_id));
    //alloc device
    if(auto err = xhc.DeviceManager()->AllocDevice(slot_id, xhc.DoorbellRegisterAt(slot_id))){
      return err;
    }

    Device* dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if(dev==nullptr){
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    Log(kDebug, "DeviceContext:%08lx\n",dev->DeviceContext());
    Log(kDebug, "InputContext:%08lx\n",dev->InputContext());

    //init input context
    memset(&dev->InputContext()->input_control_context, 0,
        sizeof(InputControlContext));
    
    const auto ep0 = usb::EndpointID(0, true);
    auto slot_ctx = dev->InputContext()->EnableSlotContext();
    auto ep0_ctx = dev->InputContext()->EnableEndpoint(ep0);

    auto& port = xhc.PortAt(port_id);
    
    //Initialize SlotContext
    slot_ctx->bits.route_string = 0;
    slot_ctx->bits.root_hub_port_num = port.Number();
    slot_ctx->bits.context_entries = 1;
    slot_ctx->bits.speed = port.Speed();

    //Initialize ep0 Context

    //DetermineMaxPacketSize
    auto maxPacketSize = 8;
    Log(kDebug,"speed:%d port:%d\n",
        slot_ctx->bits.speed,slot_ctx->bits.root_hub_port_num);
    switch (slot_ctx->bits.speed) {
    case 4: 
        // Super Speed
        maxPacketSize = 512;
    case 3:
        // High Speed
        maxPacketSize = 64;
    }

    // Control Endpoint. Bidirectional
    ep0_ctx->bits.ep_type = 4; 
    ep0_ctx->bits.max_packet_size = maxPacketSize;
    ep0_ctx->bits.max_burst_size = 0;
    ep0_ctx->SetTransferRingBuffer(dev->AllocTransferRing(ep0, 32)->Buffer());
    ep0_ctx->bits.dequeue_cycle_state = 1;
    ep0_ctx->bits.interval = 0;
    ep0_ctx->bits.max_primary_streams = 0;
    ep0_ctx->bits.mult = 0;
    ep0_ctx->bits.error_count = 3;

    //init device context
    if(auto err = xhc.DeviceManager()->LoadDCBAA(slot_id)){
      return err;
    }

    port.SetConfigPhase(ConfigPhase::kAddressingDevice);

    AddressDeviceCommandTRB addr_dev_cmd{dev->InputContext(), slot_id};
    xhc.CommandRing()->Push(addr_dev_cmd);
    xhc.DoorbellRegisterAt(0)->Ring(0);

     return MAKE_ERROR(Error::kSuccess);
  }

  Error InitializeDevice(Controller& xhc, uint8_t port_id, uint8_t slot_id){
    Log(kDebug, "InitializeDevice: port_id = %d, slot_id = %d\n", port_id, slot_id);
  
    auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if(dev == nullptr){
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    auto& port = xhc.PortAt(port_id);
    port.SetConfigPhase(ConfigPhase::kInitializingDevice);
    dev->StartInitialize();

    return MAKE_ERROR(Error::kSuccess);
  }

  Error OnTransferEvent(Controller& xhc,TransferEventTRB& trb){
    const uint8_t slot_id = trb.bits.slot_id;
    auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
    if(dev == nullptr){
      return MAKE_ERROR(Error::kInvalidSlotID);
    }

    if(auto err = dev->OnTransferEventReceived(trb)){
      return err;
    }

    const auto port_id = dev->DeviceContext()->slot_context.bits.root_hub_port_num;
    auto& port = xhc.PortAt(port_id);
    if(dev->IsInitialized() && port.GetConfigPhase()==ConfigPhase::kInitializingDevice){
      return ConfigureEndPoints(xhc, *dev);
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error OnCommandCompletionEvent(Controller& xhc,CommandCompletionEventTRB& trb){
    const auto issuer_type = trb.Pointer()->bits.trb_type;
    const auto slot_id = trb.bits.slot_id;
    
    Log(kDebug, "CommandCompletionEvent(%d): slot_id = %d, issuer = %s\n",
        trb.bits.completion_code,trb.bits.slot_id, kTRBTypeToName[issuer_type]);
    
    if(issuer_type == EnableSlotCommandTRB::Type){
      if(xhc.PortAt(addressing_port).GetConfigPhase() != ConfigPhase::kEnablingSlot){
        return MAKE_ERROR(Error::kInvalidPhase);
      }

      return AddressDevice(xhc, addressing_port, slot_id);
    }else if(issuer_type == AddressDeviceCommandTRB::Type){
      auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
      if (dev == nullptr) {
        return MAKE_ERROR(Error::kInvalidSlotID);
      }
     
      auto port_id = dev->DeviceContext()->slot_context.bits.root_hub_port_num;
      Log(kDebug, "AddressDeviceCommandTRB: slot_id = %d, port_id= %d\n",slot_id, port_id);
      if(port_id != addressing_port || 
          xhc.PortAt(addressing_port).GetConfigPhase() != ConfigPhase::kAddressingDevice){
        return MAKE_ERROR(Error::kInvalidPhase);
      }

      //configure next port
      addressing_port = 0;
      for(int i = 0; i < xhc.MaxPorts(); i++){
        auto& port = xhc.PortAt(i);
        if(port.GetConfigPhase() == ConfigPhase::kWaitingAddressed){
          if(auto err = ResetPort(xhc, port)){
            return err;
          }
          break;
        }
      }

      return InitializeDevice(xhc, port_id, slot_id);
    }else if(issuer_type == ConfigureEndpointCommandTRB::Type){
      auto dev = xhc.DeviceManager()->FindBySlot(slot_id);
      if(dev == nullptr){
        return MAKE_ERROR(Error::kInvalidSlotID);
      }

      auto port_id = dev->DeviceContext()->slot_context.bits.root_hub_port_num;
      auto& port = xhc.PortAt(port_id);
      if(xhc.PortAt(port_id).GetConfigPhase() != ConfigPhase::kConfiguringEndpoints){
        return MAKE_ERROR(Error::kInvalidPhase);
      }

      Log(kDebug, "CompleteConfiguration: port_id = %d, slot_id = %d\n", port_id, slot_id);
      dev->OnEndpointsConfigured();

      port.SetConfigPhase(ConfigPhase::kConfigured);
      return MAKE_ERROR(Error::kSuccess);
    }

    return MAKE_ERROR(Error::kInvalidPhase);
  }

  Error OnPortStatusChangeEvent(Controller& xhc,PortStatusChangeEventTRB& trb){
    const auto port_id = trb.bits.port_id;
    auto& port = xhc.PortAt(port_id);
    Log(kDebug, "PortStatusChangeEvent: port_id = %d phase:%d\n", 
    port_id, port.GetConfigPhase());

    if(port.GetConfigPhase() == ConfigPhase::kNotConnected){
      return ResetPort(xhc, port);
    }else if(port.GetConfigPhase() == ConfigPhase::kResettingPort){
      return EnableSlot(xhc, port);
    }

    return MAKE_ERROR(Error::kInvalidPhase);

  }

  int MostSignificantBit(uint32_t value) {
    if (value == 0) {
      return -1;
    }

    int msb_index;
    __asm__("bsr %1, %0"
        : "=r"(msb_index) : "m"(value));
    return msb_index;
  }


  void SwitchEchi2Xhci(const pci::Device& xhc_dev){
    bool intel_ehc_exist = false;
    for(int i = 0; i< pci::num_device; i++){
      //find ehci
      if(pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) &&
          0x8086 == pci::ReadVendorId(pci::devices[i])){
        intel_ehc_exist = true;
        break;
      }
    }
    
    if(!intel_ehc_exist){
      return;
    }

    //USB3PRM
    uint32_t superspeed_ports = pci::ReadConfReg(xhc_dev,0xdc);
    //USB3_PSSEN
    pci::WriteConfReg(xhc_dev, 0xd8, superspeed_ports);
    //XUSB2PRM
    uint32_t echi2xchi_ports = pci::ReadConfReg(xhc_dev, 0xd4);
    pci::WriteConfReg(xhc_dev, 0xd0, echi2xchi_ports);

    Log(kDebug, "SwitchEhci2Xhci: SS = %02, xHCI = %02x\n",
        superspeed_ports, echi2xchi_ports);
  }
}


namespace usb::xhci {
  Controller::Controller(uintptr_t mmio_base)
    : _mmio_base{mmio_base},
      _cp{reinterpret_cast<CapabilityRegisters*>(mmio_base)},
      _op{reinterpret_cast<OperationalRegisters*>(
          mmio_base + _cp->CAPLENGTH.Read())},
      _max_ports{static_cast<uint8_t>(
          _cp->HCSPARAMS1.Read().bits.max_ports)} {
  }

  Error Controller::Initialize(){
    if(auto err = _devmgr.Initialize(kDeviceSize)){
      return err;
    }

    RequestHCOwnership(_mmio_base, _cp->HCCPARAMS1.Read());

    //stop hc
    auto usbcmd = _op->USBCMD.Read();
    usbcmd.bits.interrupter_enable = false;
    usbcmd.bits.host_system_error_enable = false;
    usbcmd.bits.enable_wrap_event = false;
    
    if(!_op->USBSTS.Read().bits.host_controller_halted){
      usbcmd.bits.run_stop = false;
    }
    _op->USBCMD.Write(usbcmd);
    
    while(!_op->USBSTS.Read().bits.host_controller_halted);
    
    //reset controller
    usbcmd = _op->USBCMD.Read();
    usbcmd.bits.host_controller_reset = true;
    _op->USBCMD.Write(usbcmd);
    //xHCI Host Controller Reset May Lead to System Hang
    //wait 1msreturn MAKE_ERROR(Error::kSuccess);
    udelay(1000);
    while(_op->USBCMD.Read().bits.host_controller_reset);
    while(_op->USBSTS.Read().bits.controller_not_ready);

    //set max slots
    Log(kDebug, "MaxSlots:%u\n", _cp->HCSPARAMS1.Read().bits.max_device_slots);
    auto config = _op->CONFIG.Read();
    config.bits.max_device_slots_enabled = kDeviceSize;
    _op->CONFIG.Write(config);

    //set scratchpad buffer
    auto hcsparams2 = _cp->HCSPARAMS2.Read();
    const uint16_t max_scratchpad_buffers = 
        hcsparams2.bits.max_scratchpad_buffers_low | 
        (hcsparams2.bits.max_scratchpad_buffers_high<<5);
    if(max_scratchpad_buffers > 0){
      auto scratchpad_buf_arr = AllocArray<void*>(max_scratchpad_buffers, 64, 4096);
      for(int i = 0; i< max_scratchpad_buffers; i++){
        //PAGESIZE
        scratchpad_buf_arr[i] = AllocMem(4096, 4096, 4096);
        Log(kDebug, "scratchpad buffer array %d = %p\n", i, scratchpad_buf_arr[i]);
      }
      _devmgr.DeviceContexts()[0] = reinterpret_cast<DeviceContext*>(scratchpad_buf_arr);
      Log(kInfo, "wrote scratchpad buffer array %p to DeviceContext[0]\n",scratchpad_buf_arr);
    }

    //dcbaap
    Log(kDebug, "DCBA:%08lx\n",_devmgr.DeviceContexts());
    DCBAAP_Bitmap dcbaap{};
    dcbaap.SetPointer(reinterpret_cast<uint64_t>(_devmgr.DeviceContexts()));
    _op->DCBAAP.Write(dcbaap);

    //command ring
    if(auto err = _cr.Initialize(32)){
      return err;
    }
    MemMapRegister<CRCR_Bitmap>* crcr= &_op->CRCR;
    CRCR_Bitmap crcr_bitmap = crcr->Read();
    crcr_bitmap.bits.ring_cycle_state = _cr.RingCycleState();
    crcr_bitmap.bits.command_stop = false;
    crcr_bitmap.bits.command_abort = false;
    crcr_bitmap.SetPointer(reinterpret_cast<uint64_t>(_cr.Buffer()));
    crcr->Write(crcr_bitmap);

    //interrupt
    auto primary_interrupter = &InterrupterRegisterSets()[0];


    //event ring
    if(auto err = _er.Initialize(32, primary_interrupter)){
      return err;
    }

    //interrupt
    auto iman = primary_interrupter->IMAN.Read();
    iman.bits.interrupt_pending = true;
    iman.bits.interrupt_enable = true;
    primary_interrupter->IMAN.Write(iman);

    //enable interrupt
    usbcmd = _op->USBCMD.Read();
    usbcmd.bits.interrupter_enable = true;
    _op->USBCMD.Write(usbcmd);

    return MAKE_ERROR(Error::kSuccess);
  }

  Error Controller::Run(){
    auto usbcmd = _op->USBCMD.Read();
    usbcmd.bits.run_stop = true;
    _op->USBCMD.Write(usbcmd);
    
    while(_op->USBSTS.Read().bits.host_controller_halted);

    return MAKE_ERROR(Error::kSuccess);
  }

  DoorbellRegister* Controller::DoorbellRegisterAt(uint8_t index){
    return &DoorbellRegisters()[index];
  }

  Error ConfigurePort(Controller& xhc, Port& port){
    if (port.GetConfigPhase() == ConfigPhase::kNotConnected) {
      return ResetPort(xhc, port);
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  Error ConfigureEndPoints(Controller& xhc, Device& dev){
    const auto configs = dev.EndpointConfigs();
    const auto configsLen = dev.EndpointConfigsLen();

    memset(&dev.InputContext()->input_control_context, 0, sizeof(InputControlContext)); 
    memcpy(&dev.InputContext()->slot_context,
           &dev.DeviceContext()->slot_context, sizeof(SlotContext));

    auto slot_ctx = dev.InputContext()->EnableSlotContext();
    slot_ctx->bits.context_entries = 31;
    const auto port_id = dev.DeviceContext()->slot_context.bits.root_hub_port_num;
    auto& port = xhc.PortAt(port_id);
    const int port_speed = port.Speed();
    if(port_speed == 0 || port_speed > kSuperSpeedPlus){
      return MAKE_ERROR(Error::kUnknownXHCISpeedID);
    }

    std::function<int(EndpointType, int)> convert_interval;
    if(port_speed==kFullSpeed || port_speed == kLowSpeed){
      convert_interval = [](EndpointType type, int interval){
        if(type == EndpointType::kIsochronous){
          return interval + 2;
        }else{
          return MostSignificantBit(interval) + 3;
        }
      };
    }else{
      convert_interval = [](EndpointType type, int interval){
        return interval - 1;
      };
    }

    for(int i=0; i < configsLen; i++){
      auto ep_ctx = dev.InputContext()->EnableEndpoint(configs[i].ep_id);
      switch (configs[i].ep_type)
      {
        case EndpointType::kControl:
          ep_ctx->bits.ep_type = 4;
          break;
        case EndpointType::kIsochronous:
          ep_ctx->bits.ep_type = configs[i].ep_id.IsDirIn() ? 5 : 1;
          break;
        case EndpointType::kBulk:
          ep_ctx->bits.ep_type = configs[i].ep_id.IsDirIn() ? 6 : 2;
          break;
        case EndpointType::kInterrupt:
          ep_ctx->bits.ep_type = configs[i].ep_id.IsDirIn() ? 7 : 3;
          break;
      }
      ep_ctx->bits.max_packet_size = configs[i].max_packet_size;
      
      ep_ctx->bits.interval = convert_interval(configs[i].ep_type, configs[i].interval);
      ep_ctx->bits.average_trb_length = 1;

      auto tr = dev.AllocTransferRing(configs[i].ep_id, 32);
      ep_ctx->SetTransferRingBuffer(tr->Buffer());

      ep_ctx->bits.dequeue_cycle_state = 1;
      ep_ctx->bits.max_primary_streams = 0;
      ep_ctx->bits.mult = 0;
      ep_ctx->bits.error_count = 3;

    }
    
    port.SetConfigPhase(ConfigPhase::kConfiguringEndpoints);

    ConfigureEndpointCommandTRB cmd{dev.InputContext(), dev.SlotID()};
    xhc.CommandRing()->Push(cmd);
    xhc.DoorbellRegisterAt(0)->Ring(0);

    return MAKE_ERROR(Error::kSuccess);
  }

  Error ProcessEvent(Controller& xhc) {
    if(!xhc.PrimaryEventRing()->HasFront()){
      return MAKE_ERROR(Error::kSuccess);
    }
    Log(kDebugMass, "ProcessEvent\n");

    Error err = MAKE_ERROR(Error::kNotImplemented);
    auto event_trb = xhc.PrimaryEventRing()->Front();
    if(auto trb = TRBDynamicCast<TransferEventTRB>(event_trb)){
      err = OnTransferEvent(xhc, *trb);
    }else if(auto trb = TRBDynamicCast<CommandCompletionEventTRB>(event_trb)){
      err = OnCommandCompletionEvent(xhc, *trb);
    }else if(auto trb = TRBDynamicCast<PortStatusChangeEventTRB>(event_trb)){
      err = OnPortStatusChangeEvent(xhc, *trb);
    }
    xhc.PrimaryEventRing()->Pop();
    
    return err;
  }

  Controller* xhc;

  void Initialize() {
     //find intel xhc first or other xhc
    pci::Device* xhc_dev = nullptr;
    for(int i = 0 ; i < pci::num_device; i++){
      if(pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u)){
        xhc_dev = &pci::devices[i];

        if(0x8086 == pci::ReadVendorId(*xhc_dev)){
          break;
        }
      }
    }
    if(xhc_dev){
      Log(kInfo, "xHC has been found: %d.%d.%d\n",
          xhc_dev->bus, xhc_dev->device, xhc_dev->function);
    } else {
      Log(kError, "xHC has not been found\n");
      exit(1);
    }


    //configure msi
    const uint8_t bsp_local_apic_id = 
        *reinterpret_cast<const uint32_t*>(0xfee00020) >> 24;
    Log(kDebug, "apic_id: %d\n", bsp_local_apic_id);
    if(auto err = pci::ConfigureMSIFixedDestination(*xhc_dev, bsp_local_apic_id,
                                      pci::MSITriggerMode::kLevel, pci::MSIDeliveryMode::kFixed,
                                      InterruptVector::kXHCI, 0)){
      Log(kError, err, "failed to configure msi: %s",
              err.Name());
      while(1) __asm__("hlt");
    }

    //read bar0
    const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
    Log(kDebug, "ReadBar: %s\n", xhc_bar.error.Name());
    //memory map io address
    const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
    Log(kDebug, "xHC mmio_base = %08lx\n", xhc_mmio_base);

    Log(kDebug, "xhc size:%d",sizeof(usb::xhci::Controller));

    //xhci init xhc
    usb::xhci::xhc = new Controller{xhc_mmio_base};

    if(0x8086 == pci::ReadVendorId(*xhc_dev)){
      SwitchEchi2Xhci(*xhc_dev);
    }
    
    if (auto err = xhc->Initialize()) {
      Log(kError, "xhc initialize failed: %s\n", err.Name());
      exit(1);
    }

    Log(kInfo, "xHC starting\n");
    xhc->Run();
    Log(kInfo, "xHC started maxport=%d \n", xhc->MaxPorts());

    //xhci configure usb port
    for(int i = 1; i<= xhc->MaxPorts(); i++){
      auto& port = xhc->PortAt(i);
      Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());

      if(port.IsConnected()){
        if(auto err = ConfigurePort(*xhc, port)){
          Log(kError, err, "failed to configure port: %s",
              err.Name());
          continue;
        }
      }
    }
  
  }

  void ProcessEvents() {
      while(xhc->PrimaryEventRing()->HasFront()){
      if(auto err = ProcessEvent(*xhc)){
        Log(kError, err, "Error while ProcessEvent: %s", err.Name());
      }
    }
  }
}
