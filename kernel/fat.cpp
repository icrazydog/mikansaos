#include "fat.hpp"

#include <cstring>

#include "logger.hpp"

namespace{

  uint8_t CheckSum(const fat32::DirectoryEntry* entry){
    uint8_t i, sum;

    const uint8_t* p_entry = reinterpret_cast<const uint8_t *>(entry);
    for (i = sum = 0; i < 11; i++) {
        sum = (sum >> 1) + (sum << 7) + p_entry[i];
    }

    return sum;
  }

  std::pair<const char*, bool> NextPathElement(const char* path,
      char* path_elem) {
    const char* next_slash = strchr(path, '/');
    if (next_slash == nullptr) {
      strcpy(path_elem, path);
      return { nullptr, false };
    }

    const auto elem_len = next_slash - path;
    strncpy(path_elem, path, elem_len);
    path_elem[elem_len] = '\0';
    return { &next_slash[1], true };
  }


}
namespace fat32 {
  BPB* boot_volume_image;
  unsigned long bytes_per_cluster;

  void Initialize(void* volume_image){

        Log(kWarn,"boot_volume_image %p\n",volume_image);
    boot_volume_image = reinterpret_cast<fat32::BPB*>(volume_image);
    bytes_per_cluster =
        static_cast<unsigned long>(boot_volume_image->bytes_per_sector) *
        boot_volume_image->sectors_per_cluster;
  }

  //cluster(start from 2)
  uintptr_t GetClusterAddr(unsigned long cluster){
    unsigned long sector_num =
        boot_volume_image->reserved_sector_count +
        boot_volume_image->num_fats * boot_volume_image->fat_size_32 +
        (cluster - 2) * boot_volume_image->sectors_per_cluster;
    uintptr_t offset = sector_num * boot_volume_image->bytes_per_sector;

    return reinterpret_cast<uintptr_t>(boot_volume_image) + offset;
  }

  const LFNDirectoryEntry* GetLFNDirectoryEntry(const DirectoryEntry& entry,const DirectoryEntry& sfn_entry_for_check){
    auto* lfn_entry = reinterpret_cast<const LFNDirectoryEntry*>(&entry);
    if(CheckSum(&sfn_entry_for_check) == lfn_entry->check_sum){
      return lfn_entry;
    }else{
      return nullptr;
    }
  }

  void ReadName(const DirectoryEntry& entry, char* base, char* ext){
    memcpy(base, &entry.name[0], 8);
    base[8] = 0;
    for (int i = 7; i >= 0 && base[i] == 0x20; --i) {
      base[i] = 0;
    }

    memcpy(ext, &entry.name[8], 3);
    ext[3] = 0;
    for (int i = 2; i >= 0 && ext[i] == 0x20; --i) {
      ext[i] = 0;
    }
  }

  void FormatName(const DirectoryEntry& entry, char* dest) {
    char ext[5] = ".";
    ReadName(entry, dest, &ext[1]);
    if (ext[1]) {
      strcat(dest, ext);
    }
  }

  unsigned long NextCluster(unsigned long cluster){
      uint32_t next = GetFAT()[cluster];
      if (IsEndOfClusterchain(next)) {
        return kEndOfClusterchain;
      }
      return next;
  }

  // DirectoryEntry* FindFile(const char* name, unsigned long directory_cluster){
  //   if (directory_cluster == 0) {
  //     directory_cluster = boot_volume_image->root_cluster;
  //   }

  //   while (directory_cluster != kEndOfClusterchain) {
  //     auto dir_entry = GetSectorByCluster<DirectoryEntry>(directory_cluster);
  //     for (int i = 0; i < bytes_per_cluster / sizeof(DirectoryEntry); i++) {
  //       if (NameIsEqual(dir_entry[i], name)) {
  //         return &dir_entry[i];
  //       }
  //     }

  //     directory_cluster = NextCluster(directory_cluster);
  //     if(directory_cluster==kBrokenCluster){
  //       //broken
  //       return nullptr;
  //     }
  //   }

  // return nullptr;
  // }

  std::pair<DirectoryEntry*, bool> FindFile(const char* path,
      unsigned long directory_cluster) {
    if (path[0] == '/') {
      directory_cluster = boot_volume_image->root_cluster;
      path++;
    } else if (directory_cluster == 0) {
      directory_cluster = boot_volume_image->root_cluster;
    }

    char path_elem[13];
    const auto [next_path, post_slash] = NextPathElement(path, path_elem);
    const bool path_last = next_path == nullptr || next_path[0] == '\0';

    bool not_found = false;
    while (directory_cluster != kEndOfClusterchain) {
      auto dir = GetSectorByCluster<DirectoryEntry>(directory_cluster);
      for (int i = 0; i < bytes_per_cluster / sizeof(DirectoryEntry); i++) {
        if (dir[i].name[0] == 0x00) {
          not_found = true;
          break;
        } else if (!NameIsEqual(dir[i], path_elem)) {
          continue;
        }

        if (dir[i].attr == Attribute::kDirectory && !path_last) {
          return FindFile(next_path, dir[i].FirstCluster());
        } else {
          // dir[i] is not directory or current is last path
          return { &dir[i], post_slash };
        }
      }

      if(not_found){
        break;
      }

      directory_cluster = NextCluster(directory_cluster);
    }

    return { nullptr, post_slash };
  }

  bool NameIsEqual(const DirectoryEntry& entry, const char* name){
    unsigned char name83[11];
    memset(name83, 0x20, sizeof(name83));

    int i = 0;
    int i83 = 0;
    for (; name[i] != 0 && i83 < sizeof(name83); i++) {
      if (name[i] == '.') {
        i83 = 8;
        continue;
      }
      name83[i83++] = toupper(name[i]);
    }

    return memcmp(entry.name, name83, sizeof(name83)) == 0;
  }


  size_t LoadFile(void* buf, size_t len, DirectoryEntry& file_entry){
    // auto is_valid_cluster = [](uint32_t c) {
    //   return c != 0 && c != fat32::kEndOfClusterchain && c != fat32::kBrokenCluster;
    // };

    // auto cluster = file_entry.FirstCluster();
    // const auto buf_uint8 = reinterpret_cast<uint8_t*>(buf);
    // const auto buf_end = buf_uint8 + len;
    // auto p = buf_uint8;

    // while (is_valid_cluster(cluster)) {
    //   if (bytes_per_cluster >= buf_end - p) {
    //     memcpy(p, GetSectorByCluster<uint8_t>(cluster), buf_end - p);
    //     return len;
    //   }

    //   uint8_t* sectorAddr=fat32::GetSectorByCluster<uint8_t>(cluster);
    //   //   Log(kDebug,"cluster %lu,add %p,02x\n",cluster, sectorAddr, *sectorAddr);
    //   memcpy(p, sectorAddr, bytes_per_cluster);
    //   p += bytes_per_cluster;
    //   cluster = NextCluster(cluster);
    // }

    // return p - buf_uint8;

    return FileDescriptor{file_entry}.Read(buf, len);
  }

  uint32_t* GetFAT(){
    uintptr_t fat_offset = boot_volume_image->reserved_sector_count *
        boot_volume_image->bytes_per_sector;
    uint32_t* fat = reinterpret_cast<uint32_t*>(
        reinterpret_cast<uintptr_t>(boot_volume_image) + fat_offset);
     return fat;
  }

  bool IsEndOfClusterchain(unsigned long cluster){
    return cluster >= 0x0ffffff8ul;
  }

  unsigned long ExtendCluster(unsigned long eoc_cluster, size_t n){
    uint32_t* fat = GetFAT();
    while (!IsEndOfClusterchain(fat[eoc_cluster])) {
      eoc_cluster = fat[eoc_cluster];
    }

    size_t num_allocated = 0;
    auto current = eoc_cluster;

    //Warning not check total fat number,maybe overflow
    for (unsigned long candidate = 2; num_allocated < n; candidate++) {
      if (fat[candidate] != 0) {
        continue;
      }
      fat[current] = candidate;
      current = candidate;
      num_allocated++;
    }
    fat[current] = kEndOfClusterchain;
    return current;
  }


  void FreeCluster(unsigned long cluster){
    uint32_t* fat = GetFAT();
    if(IsEndOfClusterchain(fat[cluster]) || fat[cluster]==0){
      fat[cluster] = 0;
    }else{
      FreeCluster(fat[cluster]);
      fat[cluster] = 0;
    }
  }

  DirectoryEntry* AllocateEntry(unsigned long dir_cluster){
    while (true) {
      auto dir = GetSectorByCluster<DirectoryEntry>(dir_cluster);
      for (int i = 0; i < bytes_per_cluster / sizeof(DirectoryEntry); ++i) {
        if (dir[i].name[0] == 0 || dir[i].name[0] == 0xe5) {
          return &dir[i];
        }
      }
      auto next = NextCluster(dir_cluster);
      if (next == kEndOfClusterchain) {
        break;
      }
      dir_cluster = next;
    }

    dir_cluster = ExtendCluster(dir_cluster, 1);
    auto dir = GetSectorByCluster<DirectoryEntry>(dir_cluster);
    memset(dir, 0, bytes_per_cluster);
    return &dir[0];
  }

  void SetFileName(DirectoryEntry& entry, const char* name){
    const char* dot_pos = strrchr(name, '.');
    memset(entry.name, ' ', 8+3);
    if (dot_pos) {
      for (int i = 0; i < 8 && i < dot_pos - name; ++i) {
        entry.name[i] = toupper(name[i]);
      }
      for (int i = 0; i < 3 && dot_pos[i + 1]; ++i) {
        entry.name[8 + i] = toupper(dot_pos[i + 1]);
      }
    } else {
      for (int i = 0; i < 8 && name[i]; ++i) {
        entry.name[i] = toupper(name[i]);
      }
    }
  }

  WithError<DirectoryEntry*> CreateFile(const char* path){
    auto parent_dir_cluster = fat32::boot_volume_image->root_cluster;
    const char* filename = path;

    if (const char* slash_pos = strrchr(path, '/')) {
      filename = &slash_pos[1];
      if (slash_pos[1] == '\0') {
        return { nullptr, MAKE_ERROR(Error::kIsDirectory) };
      }

      char parent_dir_name[slash_pos - path + 1];
      strncpy(parent_dir_name, path, slash_pos - path);
      parent_dir_name[slash_pos - path] = '\0';

      if (parent_dir_name[0] != '\0') {
        auto [parent_dir, post_slash2] = fat32::FindFile(parent_dir_name);
        if (parent_dir == nullptr) {
          return { nullptr, MAKE_ERROR(Error::kNoSuchEntry) };
        }

        if(parent_dir->attr != fat32::Attribute::kDirectory){
           return { nullptr, MAKE_ERROR(Error::kIsNotDirectory) };
        }

        parent_dir_cluster = parent_dir->FirstCluster();
      }
    }
   
    auto dir = fat32::AllocateEntry(parent_dir_cluster);
    if (dir == nullptr) {
      return { nullptr, MAKE_ERROR(Error::kNoEnoughMemory) };
    }
    fat32::SetFileName(*dir, filename);
    dir->file_size = 0;
    return { dir, MAKE_ERROR(Error::kSuccess) };
  }

  ///not finish
  ///not delete long name file well
  Error DeleteFile(const char* path, char* err_str){
     auto [file_entry, post_slash] = fat32::FindFile(path);
    if (file_entry) {
      if (file_entry->attr != fat32::Attribute::kDirectory && post_slash) {
        
        if(err_str){
          char name[13];
          fat32::FormatName(*file_entry, name);
          sprintf(err_str, "%s", name);
          strcat(err_str," is not a directory\n");
        }
        return MAKE_ERROR(Error::kInvalidFile);
      }else if(file_entry->attr == fat32::Attribute::kDirectory){
        
        if(err_str){
          char name[13];
          fat32::FormatName(*file_entry, name);
          sprintf(err_str, "%s", name);
          strcat(err_str," is  a directory\n");
        }
        return MAKE_ERROR(Error::kInvalidFile);
      }else if(file_entry->attr == fat32::Attribute::kSystem){
        if(err_str){
          char name[13];
          fat32::FormatName(*file_entry, name);
          sprintf(err_str, "%s", name);
          strcat(err_str," is  a system file\n");
        }
        return MAKE_ERROR(Error::kInvalidFile);
      } else{
        //
        //sizeof(DirectoryEntry)
        DirectoryEntry* prev_entry = file_entry - 1;
        if(prev_entry->attr == fat32::Attribute::kLongName){
          if(err_str){
            char name[13];
            fat32::FormatName(*file_entry, name);
            sprintf(err_str, "%s", name);
            strcat(err_str," is a LNF\n");
          }
          return MAKE_ERROR(Error::kInvalidFile); 
        }

        FreeCluster(file_entry->FirstCluster());
        file_entry->name[0]=0xe5;
        return MAKE_ERROR(Error::kSuccess);
      }
    }else{
      if(err_str){
        sprintf(err_str,"no such file:");
        strcat(err_str,path);
        strcat(err_str,"\n");
      }
      return MAKE_ERROR(Error::kInvalidFile);
    }
  }


  //Warning not check total fat number,
  //maybe overflow,infinite loop
  unsigned long AllocateClusterChain(size_t n) {
    uint32_t* fat = GetFAT();
    unsigned long first_cluster = 2;
    while (true) {
      if (fat[first_cluster] == 0) {
        fat[first_cluster] = kEndOfClusterchain;
        break;
      }
      first_cluster++;
    }

    if (n > 1) {
      ExtendCluster(first_cluster, n - 1);
    }
    return first_cluster;
  }

  FileDescriptor::FileDescriptor(DirectoryEntry& fat_entry)
      : _fat_entry{fat_entry} {
  }

  size_t FileDescriptor::Read(void* buf, size_t len) {
    if (_rd_cluster == 0) {
      _rd_cluster = _fat_entry.FirstCluster();
    }
    uint8_t* buf8 = reinterpret_cast<uint8_t*>(buf);
    len = std::min(len, _fat_entry.file_size - _rd_off);

    size_t total = 0;
    while (total < len) {
      uint8_t* sec = GetSectorByCluster<uint8_t>(_rd_cluster);
      size_t n = std::min(len - total, bytes_per_cluster - _rd_cluster_off);
      memcpy(&buf8[total], &sec[_rd_cluster_off], n);
      total += n;

      _rd_cluster_off += n;
      if (_rd_cluster_off == bytes_per_cluster) {
        _rd_cluster = NextCluster(_rd_cluster);
        _rd_cluster_off = 0;
      }
    }

    _rd_off += total;
    return total;
  }

  size_t FileDescriptor::Write(const void* buf, size_t len) {
    auto num_cluster = [](size_t bytes) {
      return (bytes + bytes_per_cluster - 1) / bytes_per_cluster;
    };

    if (_wr_cluster == 0) {
      if (_fat_entry.FirstCluster() != 0) {
        _wr_cluster = _fat_entry.FirstCluster();
      } else {
        _wr_cluster = AllocateClusterChain(num_cluster(len));
        _fat_entry.first_cluster_low = _wr_cluster & 0xffff;
        _fat_entry.first_cluster_high = (_wr_cluster >> 16) & 0xffff;
      }
    }

    const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(buf);

    size_t total = 0;
    while (total < len) {
      if (_wr_cluster_off == bytes_per_cluster) {
        const auto next_cluster = NextCluster(_wr_cluster);
        if (next_cluster == kEndOfClusterchain) {
          _wr_cluster = ExtendCluster(_wr_cluster, num_cluster(len - total));
        } else {
          _wr_cluster = next_cluster;
        }
        _wr_cluster_off = 0;
      }

      uint8_t* sec = GetSectorByCluster<uint8_t>(_wr_cluster);
      size_t n = std::min(len, bytes_per_cluster - _wr_cluster_off);
      memcpy(&sec[_wr_cluster_off], &buf8[total], n);
      total += n;

      _wr_cluster_off += n;
    }

    _wr_off += total;
    _fat_entry.file_size = _wr_off;
    return total;
  }

  size_t FileDescriptor::Load(void* buf, size_t len, size_t offset) {
    FileDescriptor fd{_fat_entry};
    fd._rd_off = offset;

    unsigned long cluster = _fat_entry.FirstCluster();
    while (offset >= bytes_per_cluster) {
      offset -= bytes_per_cluster;
      cluster = NextCluster(cluster);
    }

    fd._rd_cluster = cluster;
    fd._rd_cluster_off = offset;
    return fd.Read(buf, len);
  }
}// namespace fat