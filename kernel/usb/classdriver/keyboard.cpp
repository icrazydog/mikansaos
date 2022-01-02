#include "usb/classdriver/keyboard.hpp"

#include <algorithm>
#include <bitset>

#include "usb/memory.hpp"

namespace usb {
  std::function<HIDKeyboardDriver::ObserverType> HIDKeyboardDriver::default_observer;

  HIDKeyboardDriver::HIDKeyboardDriver(Device* dev, int interface_index)
    : HIDBaseDriver{dev, interface_index, 8} {
  }

  void HIDKeyboardDriver::SubscribeKeyPush(
      std::function<ObserverType> observer) {
    _observers[_num_observers++] = observer;
  }

  void HIDKeyboardDriver::NotifyKeyPush(uint8_t modifier, uint8_t keycode, bool press) {
    for (int i = 0; i < _num_observers; i++) {
      _observers[i](modifier, keycode, press);
    }
  }

  Error HIDKeyboardDriver::OnDataReceived() {
    // for (int i = 2; i < 8; ++i) {
    //   const uint8_t key = Buffer()[i];
    //   if (key == 0) {
    //     continue;
    //   }
    //   const auto& prev_buf = PreviousBuffer();
    //   if (std::find(prev_buf.begin() + 2, prev_buf.end(), key) != prev_buf.end()) {
    //     continue;
    //   }
    //   NotifyKeyPush(Buffer()[0], key);
    // }

    //multiple press support
    std::bitset<256> prev, current;

    for (int i = 2; i < 8; ++i) {
      prev.set(PreviousBuffer()[i], true);
      current.set(Buffer()[i], true);
    }
    const auto changed = prev ^ current;
    const auto pressed = changed & current;

    for (int key = 1; key < 256; key++) {
      if (changed.test(key)) {
        NotifyKeyPush(Buffer()[0], key, pressed.test(key));
      }
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  void* HIDKeyboardDriver::operator new(size_t size) {
    return AllocMem(sizeof(HIDKeyboardDriver), 0, 0);
  }

  void HIDKeyboardDriver::operator delete(void* ptr) noexcept {
    FreeMem(ptr);
  }

}