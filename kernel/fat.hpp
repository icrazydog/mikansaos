#pragma once

#include <cstdint>
#include <cstddef>

#include "file.hpp"
#include "error.hpp"

namespace fat32 {

  struct BPB {
    uint8_t jump_boot[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
  } __attribute__((packed));

  enum class Attribute : uint8_t {
    kReadOnly  = 0x01,
    kHidden    = 0x02,
    kSystem    = 0x04,
    kVolumeID  = 0x08,
    kDirectory = 0x10,
    kArchive   = 0x20,
    kLongName  = 0x0f,
  };

  struct DirectoryEntry {
    unsigned char name[11];
    Attribute attr;
    uint8_t ntres;
    uint8_t create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;

    uint32_t FirstCluster() const {
      return first_cluster_low |
        (static_cast<uint32_t>(first_cluster_high) << 16);
    }
  } __attribute__((packed));

  const uint8_t LFN_ORD_LAST_FLAG = 0x40;

  struct LFNDirectoryEntry {
    uint8_t ord;
    //1-5
    unsigned char name1[10];
    Attribute attr;
    uint8_t ntres;
    uint8_t check_sum;
    //6-11
    unsigned char name2[12];
    uint16_t first_cluster_low;
    //12-13
    unsigned char name3[4];

    bool IsLastLFNDirectoryEntry() const{
      return ord & LFN_ORD_LAST_FLAG;
    }

    int LongName(char* fullname) const {
      int name_index = 0;
      //name1
      for(int i=0;i<5;i++){
        if(name1[i*2+1] != 00){
          //not support unioncode
          fullname[name_index++] = '?';
          continue;
        }else{
          fullname[name_index++] = name1[i*2];
          if(name1[i*2] == 00){
            return name_index;
          }
        }
      }

      //name2
      for(int i=0;i<6;i++){
        if(name2[i*2+1] != 00){
          //not support unioncode
          fullname[name_index++] = '?';
          continue;
        }else{
          fullname[name_index++] = name2[i*2];
          if(name2[i*2] == 00){
            return name_index;
          }
        }
      }

      //name3
      for(int i=0;i<2;i++){
        if(name3[i*2+1] != 00){
          //not support unioncode
          fullname[name_index++] = '?';
          continue;
        }else{
          fullname[name_index++] = name3[i*2];
          if(name3[i*2] == 00){
            return name_index;
          }
        }
      }

      return name_index;
    }
  } __attribute__((packed));


  extern BPB* boot_volume_image;
  extern unsigned long bytes_per_cluster;
  void Initialize(void* volume_image);

  //cluster(start from 2)
  uintptr_t GetClusterAddr(unsigned long cluster);

  template <class T>
  T* GetSectorByCluster(unsigned long cluster) {
    return reinterpret_cast<T*>(GetClusterAddr(cluster));
  }

  const LFNDirectoryEntry* GetLFNDirectoryEntry(const DirectoryEntry& entry,const DirectoryEntry& sfn_entry_for_check);

  void ReadName(const DirectoryEntry& entry, char* base, char* ext);

  void FormatName(const DirectoryEntry& entry, char* dest);

  static const unsigned long kEndOfClusterchain = 0x0ffffffflu;
  static const unsigned long kBrokenCluster     = 0x0ffffff7lu;

  unsigned long NextCluster(unsigned long cluster);

  // DirectoryEntry* FindFile(const char* name, unsigned long directory_cluster = 0);
  std::pair<DirectoryEntry*, bool> FindFile(const char* path, 
      unsigned long directory_cluster = 0);
  

  bool NameIsEqual(const DirectoryEntry& entry, const char* name);

  size_t LoadFile(void* buf, size_t len, DirectoryEntry& dir_entry);


  uint32_t* GetFAT();
  bool IsEndOfClusterchain(unsigned long cluster);
  unsigned long ExtendCluster(unsigned long eoc_cluster, size_t n);
  DirectoryEntry* AllocateEntry(unsigned long dir_cluster);
  void SetFileName(DirectoryEntry& entry, const char* name);
  WithError<DirectoryEntry*> CreateFile(const char* path);
  Error DeleteFile(const char* path, char* err_str);
  unsigned long AllocateClusterChain(size_t n);

  class FileDescriptor : public ::FileDescriptor{
    public:
      explicit FileDescriptor(DirectoryEntry& fat_entry);
      size_t Read(void* buf, size_t len) override;
      size_t Write(const void* buf, size_t len) override;
      size_t Size() const override { return _fat_entry.file_size; }
      size_t Load(void* buf, size_t len, size_t offset) override;

    private:
      DirectoryEntry& _fat_entry;

      size_t _rd_off = 0;
      unsigned long _rd_cluster = 0;
      size_t _rd_cluster_off = 0;

      size_t _wr_off = 0;
      unsigned long _wr_cluster = 0;
      size_t _wr_cluster_off = 0;
  };
}