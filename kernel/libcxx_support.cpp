#include <new>
#include <cerrno>
#include <cstdlib>

int printk(const char* format, ...);

std::new_handler std::get_new_handler() noexcept {
    return [] {
    printk("not enough memory\n");
    exit(1);
  };
  //return nullptr;
}

extern "C" int posix_memalign(void**, size_t, size_t) {
  return ENOMEM;
}
