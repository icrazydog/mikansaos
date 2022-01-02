#pragma once

#include <functional>
#include "usb/classdriver/hid.hpp"

namespace usb {
    class HIDMouseDriver : public HIDBaseDriver {
      public:
        HIDMouseDriver(Device* dev,int interface_index);

        void* operator new(size_t size);
        void operator delete(void* ptr) noexcept;

        using ObserverType = void (uint8_t buttons, int8_t displacement_x, int8_t displacement_y);
        void SubscribeMouseMove(std::function<ObserverType> observer);
        static std::function<ObserverType> default_observer;

        Error OnDataReceived();

      private:
        std::array<std::function<ObserverType>, 4> _observers;
        int _num_observers;

        void NotifyMouseMove(uint8_t buttons, int8_t displacement_x, int8_t displacement_y);
    };
}