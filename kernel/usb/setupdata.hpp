#pragma once

#include <cstdint>

namespace usb {

  namespace request_type {
    // bmRequestType recipient
    const int kDevice = 0;
    const int kInterface = 1;
    const int kEndpoint = 2;
    const int kOther = 3;

    // bmRequestType type
    const int kStandard = 0;
    const int kClass = 1;
    const int kVendor = 2;

    // bmRequestType direction
    const int kOut = 0;
    const int kIn = 1;
  }

  namespace request {
    const int kGetStatus = 0;
    const int kClearFeature = 1;
    const int kSetFeature = 3;
    const int kSetAddress = 5;
    const int kGetDescriptor = 6;
    const int kSetDescriptor = 7;
    const int kGetConfiguration = 8;
    const int kSetConfiguration = 9;
    const int kGetInterface = 10;
    const int kSetInterface = 11;
    const int kSynchFrame = 12;
    const int kSetEncryption = 13;
    const int kGetEncryption = 14;
    const int kSetHandshake = 15;
    const int kGetHandshake = 16;
    const int kSetConnection = 17;
    const int kSetSecurityData = 18;
    const int kGetSecurityData = 19;
    const int kSetWUSBData = 20;
    const int kLoopbackDataWrite = 21;
    const int kLoopbackDataRead = 22;
    const int kSetInterfaceDS = 23;
    const int kSetSel = 48;
    const int kSetIsochDelay = 49;

    // HID class specific report values
    const int kGetReport = 1;
    const int kSetProtocol = 11;
  }
  
  struct SetupData {
    union {
      uint8_t data;
      struct {
        uint8_t recipient : 5;
        uint8_t type : 2;
        uint8_t direction : 1;
      } bits;
    } request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
  } __attribute__((packed));

  inline bool operator ==(SetupData lhs, SetupData rhs) {
    return
      lhs.request_type.data == rhs.request_type.data &&
      lhs.request == rhs.request &&
      lhs.value == rhs.value &&
      lhs.index == rhs.index &&
      lhs.length == rhs.length;
  }
}