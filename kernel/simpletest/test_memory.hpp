#pragma once

#include "test_base.hpp"

#include <cstdint>

#include "logger.hpp"
#include "memory_manager.hpp"

namespace{
  using namespace SimpleTest;
  void CHECK_EQUAL(uint64_t v1, uint64_t v2){

    if(v1 != v2){
      printk("   CHECK_[EQUAL] FAIL:%d,%d \n",v1,v2);
      simpleTestHasError = true;
    }
  }

  void CHECK_TRUE(bool v){

    if(!v){
      printk("   CHECK_[TRUE] FAIL\n");
      simpleTestHasError = true;
    }
  }

  void clear(BitmapMemoryManager& mgr){
    mgr.Free(FrameID{0},BitmapMemoryManager::kFrameCount);
  }
}


void START_TEST_MEMORY(BitmapMemoryManager& mgr){
  if(!kEnableSimpleTest){
    return;
  }


  {
    printk("TEST(MemoryManager, Allocate)\n"); 

    const auto frame1 = mgr.Allocate(3);
    const auto frame2 = mgr.Allocate(1);

    CHECK_EQUAL(0, frame1.value.ID());
    CHECK_TRUE(3 <= frame2.value.ID());
    clear(mgr);
  }


  {
    printk("TEST(MemoryManager, AllocateMultipleLine) \n");

    const auto frame1 = mgr.Allocate(BitmapMemoryManager::kBitsPerMapLine + 3);
    const auto frame2 = mgr.Allocate(1);

    CHECK_EQUAL(0, frame1.value.ID());
    CHECK_TRUE(BitmapMemoryManager::kBitsPerMapLine + 3 <= frame2.value.ID());
    clear(mgr);
  }

  {
    printk("TEST(MemoryManager, AllocateNoEnoughMemory) \n"); 

    const auto frame1 = mgr.Allocate(BitmapMemoryManager::kFrameCount + 1);

    CHECK_EQUAL(Error::kNoEnoughMemory, frame1.error.Cause());
    CHECK_EQUAL(kNullFrame.ID(), frame1.value.ID());
    clear(mgr);
  }

  {
    printk("TEST(MemoryManager, Free)\n") ;

    const auto frame1 = mgr.Allocate(3);
    mgr.Free(frame1.value, 3);
    const auto frame2 = mgr.Allocate(2);

    CHECK_EQUAL(0, frame1.value.ID());
    CHECK_EQUAL(0, frame2.value.ID());
    clear(mgr);
  } 
  {
    printk("TEST(MemoryManager, FreeDifferentFrames) \n") ;

    const auto frame1 = mgr.Allocate(3);
    mgr.Free(frame1.value, 1);
    const auto frame2 = mgr.Allocate(2);

    CHECK_EQUAL(0, frame1.value.ID());
    CHECK_TRUE(3 <= frame2.value.ID());
    clear(mgr);
  }

  {
    printk("TEST(MemoryManager, MarkAllocated) \n") ;
   
    mgr.MarkAllocated(FrameID{61}, 3);
    const auto frame1 = mgr.Allocate(64);

    CHECK_EQUAL(64, frame1.value.ID());
    clear(mgr);
  }


  {
    printk("TEST(MemoryManager, SetMemoryRange)\n") ;
  
    const auto frame1 = mgr.Allocate(1);
    mgr.SetMemoryRange(FrameID{10}, FrameID{64});
    const auto frame2 = mgr.Allocate(1);

    CHECK_EQUAL(0, frame1.value.ID());
    CHECK_EQUAL(10, frame2.value.ID());
    clear(mgr);
  }


  if(simpleTestHasError){
    while (1);
  }
}