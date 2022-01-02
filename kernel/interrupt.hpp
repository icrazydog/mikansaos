#pragma once

#include <cstdint>
#include <array>
#include <deque>

#include "x86_descriptor.hpp"
#include "message.hpp"

// index of the interrupt stack table
const int kISTForTimer = 1;


class InterruptVector {
 public:
  //32-255
  enum Number {
    kXHCI = 51,
    kLAPICTimer = 52,
  };
};

struct InterruptFrame {
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
};


struct InterruptDescriptor {
  uint16_t offset_low;
  uint16_t segment_selector;
  struct {
    uint16_t interrupt_stack_table : 3;
    uint16_t : 5;
    DescriptorType type : 4;
    uint16_t : 1;
    uint16_t descriptor_privilege_level : 2;
    uint16_t present : 1;
  } __attribute__((packed)) attr;
  uint16_t offset_middle;
  uint32_t offset_high;
  uint32_t reserved;
} __attribute__((packed));

extern std::array<InterruptDescriptor, 256> idt;

void NotifyEndOfInterrupt();

void SetIDTEntry(InterruptDescriptor& desc,
                 DescriptorType type, uint8_t descriptor_privilege_level,
                 uint64_t offset, uint16_t segment_selector,
                 uint8_t interrupt_stack_table = 0,bool present = true);

// void InitializeInterrupt(std::deque<Message>* msg_queue);
void InitializeInterrupt();