#include "usb/classdriver/base.hpp"

namespace usb {
  ClassDriver::ClassDriver(Device* dev) : _dev{dev} {
  }

  ClassDriver::~ClassDriver() {
  }
}
