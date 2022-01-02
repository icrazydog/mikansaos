#include "terminal.hpp"

#include <cstring>

#include "layer.hpp"
#include "task.hpp"
#include "logger.hpp"
#include "font.hpp"
#include "pci.hpp"
#include "usb/xhci/xhci.hpp"
#include "elf.hpp"
#include "memory_manager.hpp"
#include "paging.hpp"
#include "asmfunc.h"
#include "timer.hpp"
#include "uefi.h"
#include "keyboard.hpp"


namespace {
  WithError<int> MakeArgv(char* command, char* arg,
      char** argv, int argv_len, char* argbuf, int argbuf_len) {
    int argc = 0;
    int argbuf_index = 0;

    auto push_to_argv = [&](const char* s) {
      if (argc >= argv_len || argbuf_index >= argbuf_len) {
        return MAKE_ERROR(Error::kFull);
      }

      argv[argc] = &argbuf[argbuf_index];
      argc++;
      strcpy(&argbuf[argbuf_index], s);
      argbuf_index += strlen(s) + 1;
      return MAKE_ERROR(Error::kSuccess);
    };

    if (auto err = push_to_argv(command)) {
      return { argc, err };
    }
    if (!arg) {
      return { argc, MAKE_ERROR(Error::kSuccess) };
    }

    char* p = arg;
    while (true) {
      while (isspace(*p)) {
        p++;
      }
      if (*p == 0) {
        break;
      }
      const char* cur_arg = p;

      while (*p != 0 && !isspace(*p)) {
        p++;
      }
      const bool is_end = *p == 0;
      *p = 0;
      if (auto err = push_to_argv(cur_arg)) {
        return { argc, err };
      }
      if (is_end) {
        break;
      }
      p++;
    }

    return { argc, MAKE_ERROR(Error::kSuccess) };
  }

  Elf64_Phdr* GetProgramHeader(Elf64_Ehdr* ehdr) {
    return reinterpret_cast<Elf64_Phdr*>(
        reinterpret_cast<uintptr_t>(ehdr) + ehdr->e_phoff);
  }

  uintptr_t GetFirstLoadAddress(Elf64_Ehdr* ehdr) {
    auto phdr = GetProgramHeader(ehdr);
    for (int i = 0; i < ehdr->e_phnum; ++i) {
      if (phdr[i].p_type != PT_LOAD) continue;
      return phdr[i].p_vaddr;
    }
    return 0;
  }

  static_assert(kBytesPerFrame >= 4096);


  WithError<uint64_t> CopyLoadSegments(Elf64_Ehdr* ehdr) {
    auto phdr = GetProgramHeader(ehdr);
    uint64_t last_addr = 0;
    for (int i = 0; i < ehdr->e_phnum; ++i) {
      if (phdr[i].p_type != PT_LOAD) continue;

      LinearAddress4Level dest_addr;
      dest_addr.value = phdr[i].p_vaddr;
      last_addr = std::max(last_addr, phdr[i].p_vaddr + phdr[i].p_memsz);
      const auto num_4kpages = (phdr[i].p_memsz + 4095) / 4096;

      if (auto err = SetupPageMaps(dest_addr, num_4kpages, false)) {
        return {last_addr, err};
      }

      const auto src = reinterpret_cast<uint8_t*>(ehdr) + phdr[i].p_offset;
      const auto dst = reinterpret_cast<uint8_t*>(phdr[i].p_vaddr);
      memcpy(dst, src, phdr[i].p_filesz);
      memset(dst + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
    }
    return { last_addr, MAKE_ERROR(Error::kSuccess) };
  }

  WithError<uint64_t> LoadELF(Elf64_Ehdr* ehdr) {
    if (ehdr->e_type != ET_EXEC) {
      return { 0, MAKE_ERROR(Error::kInvalidFormat) };
    }

    const auto addr_first = GetFirstLoadAddress(ehdr);
    if (addr_first < 0xffff'8000'0000'0000) {
      return { 0, MAKE_ERROR(Error::kInvalidFormatLA) };
    }

    return CopyLoadSegments(ehdr);
  }


  WithError<PageMapEntry*> SetupPML4(Task& current_task) {
    auto pml4 = NewPageMap();
    if (pml4.error) {
      return pml4;
    }

    const auto current_pml4 = reinterpret_cast<PageMapEntry*>(GetCR3());
    memcpy(pml4.value, current_pml4, 256 * sizeof(uint64_t));

    const auto cr3 = reinterpret_cast<uint64_t>(pml4.value);
    SetCR3(cr3);
    current_task.Context().cr3 = cr3;
    return pml4;
  }

  Error FreePML4(Task& current_task) {
    const auto cr3 = current_task.Context().cr3;
    current_task.Context().cr3 = 0;
    ResetCR3();

    return FreePageMap(reinterpret_cast<PageMapEntry*>(cr3));
  }

  // void ListAllEntries(Terminal* term, uint32_t dir_cluster) {
  void ListAllEntries(FileDescriptor& fd, uint32_t dir_cluster) {
    // auto root_dir_entries = fat32::GetSectorByCluster<fat32::DirectoryEntry>(
    // fat32::boot_volume_image->root_cluster);

    // auto entries_per_cluster =
    //     fat32::boot_volume_image->bytes_per_sector * fat32::boot_volume_image->sectors_per_cluster
    //     / sizeof(fat32::DirectoryEntry);

    // char base[9], ext[4];
    // char s[64];
    // char lfns[255];
    // for (int i = 0; i < entries_per_cluster; i++) {
    //   ReadName(root_dir_entries[i], base, ext);
    //   if (base[0] == 0x00) {
    //     break;
    //   } else if (static_cast<uint8_t>(base[0]) == 0xe5) {
    //     continue;
    //   } else if (root_dir_entries[i].attr == fat32::Attribute::kLongName) {
    //     continue;
    //   }

    

    //   if (ext[0]) {
    //     sprintf(s, "%s.%s", base, ext);
    //   } else {
    //     sprintf(s, "%s", base);
    //   }

    //   Print(s);

    //   if(has_lfn){
    //     Print("    ");
    //     Print(lfns);
    //   }
    //   Print("\n");
    // }
    const auto kEntriesPerCluster =
      fat32::bytes_per_cluster / sizeof(fat32::DirectoryEntry);

    while (dir_cluster != fat32::kEndOfClusterchain) {
      auto dir = fat32::GetSectorByCluster<fat32::DirectoryEntry>(dir_cluster);

      char lfns[255];
      for (int i = 0; i < kEntriesPerCluster; ++i) {
        if (dir[i].name[0] == 0x00) {
          return;
        } else if (static_cast<uint8_t>(dir[i].name[0]) == 0xe5) {
          continue;
        } else if (dir[i].attr == fat32::Attribute::kLongName) {
          continue;
        }


        //long name
        bool has_lfn = false;
        int j = i - 1 ;
        int sz = 0;
        while(j>=0 && dir[i].attr != fat32::Attribute::kVolumeID &&
            dir[j].attr == fat32::Attribute::kLongName){
          const auto* lfn_dir_entry = fat32::GetLFNDirectoryEntry(dir[j], dir[i]);
          if(lfn_dir_entry != nullptr){
            Log(kDebug, "lfn entity: ord: %x,chksum: %x\n",
                lfn_dir_entry->ord,
                lfn_dir_entry->check_sum);
            if(sz+13 >= 255){
              //err
              break;
            }
            sz += lfn_dir_entry->LongName(lfns+sz);
            if(lfn_dir_entry->IsLastLFNDirectoryEntry()){ 
              if(sz>0){
                lfns[sz] = 0;
                has_lfn= true;
              }
              break;
            }
          }else{
            //err
            break;
          }
          j--;
        }

        char name[13];
        fat32::FormatName(dir[i], name);
        // term->Print(name);
        PrintToFD(fd, "%s", name);

        if(has_lfn){
          // term->Print("    ");
          // term->Print(lfns);
          PrintToFD(fd, "    ");
          PrintToFD(fd, "%s", lfns);
        }

        // term->Print("\n");
        PrintToFD(fd, "\n");
      }

      dir_cluster = fat32::NextCluster(dir_cluster);
    }
  }

  WithError<AppLoadInfo> LoadApp(fat32::DirectoryEntry& file_entry, Task& task) {
    PageMapEntry* temp_pml4;


    //mutiple app started
    auto appinfo_iter = app_loads->find(&file_entry); 
    if(appinfo_iter != app_loads->end()){
      if(auto [pml4, err] = SetupPML4(task); err){
        return { {}, err };
      }else{
        temp_pml4 = pml4;
      }


      AppLoadInfo app_load = appinfo_iter->second;
      auto err = CopyPageMaps(temp_pml4, app_load.pml4, 4, 256);
      app_load.pml4 = temp_pml4;
      app_load.prototype->count++;
      return { app_load, err };
    }

    std::vector<uint8_t> file_buf(file_entry.file_size);
    fat32::LoadFile(&file_buf[0], file_buf.size(), file_entry);

    auto elf_header = reinterpret_cast<Elf64_Ehdr*>(&file_buf[0]);
    if(memcmp(elf_header->e_ident, "\x7f" "ELF", 4) != 0){
      return { {}, MAKE_ERROR(Error::kInvalidFile) };
    }

    if(auto [pml4, err] = SetupPML4(task); err){
      return { {}, err };
    }else{
      temp_pml4 = pml4;
    }

    //load to segment by elf
    auto [last_addr, err_load] = LoadELF(elf_header);
    if(err_load){
      AppLoadInfo app_load;
      app_load.entry = elf_header->e_entry;
      return { app_load, err_load };
    }

    AppLoadInfo app_load{last_addr, elf_header->e_entry, temp_pml4};
    auto appprototype_pair = app_loads->insert(std::make_pair(&file_entry, app_load));
    AppLoadInfo* appprototype = &appprototype_pair.first->second;
    appprototype->prototype = appprototype;
    app_load.prototype = appprototype;

    if(auto [pml4, err] = SetupPML4(task); err){
      //error
      return { app_load, err };
    }else{
      app_load.pml4 = pml4;
    }
    auto err = CopyPageMaps(app_load.pml4, temp_pml4, 4, 256);
    app_load.prototype->count++;
    return { app_load, err };
  }

  fat32::DirectoryEntry* FindCommand(const char* command,
      unsigned long dir_cluster = 0) {

    auto [file_entry, post_slash] = fat32::FindFile(command, dir_cluster);

    if(file_entry){
      if(file_entry->attr == fat32::Attribute::kDirectory || post_slash){
        return nullptr;
      }else{
        return file_entry;
      }
    }

    if (dir_cluster != 0 || strchr(command, '/') != nullptr) {
      return nullptr;
    }

    auto apps_entry = fat32::FindFile("apps");
    if (apps_entry.first == nullptr ||
        apps_entry.first->attr != fat32::Attribute::kDirectory) {
      return nullptr;
    }
    return FindCommand(command, apps_entry.first->FirstCluster());
  }

}// namespace

std::map<fat32::DirectoryEntry*, AppLoadInfo>* app_loads;

Terminal::Terminal(Task&  task, const TerminalDescriptor* term_desc) 
    : _task{task}{

  if(term_desc){
    _show_window = term_desc->show_window;
    for (int i = 0; i < _files.size(); ++i) {
      _files[i] = term_desc->files[i];
    }
  }else{
    _show_window = true;
    for (int i = 0; i < _files.size(); ++i) {
      _files[i] = std::make_shared<TerminalFileDescriptor>(*this);
    }
  }

  if(_show_window){
    _window = std::make_shared<ToplevelWindow>(
        kColumns * 8 + 8 + ToplevelWindow::kMarginX,
        kRows * 16 + 8 + ToplevelWindow::kMarginY,
        screen_frame_buffer_config.pixel_format,
        "MikanSAOSTerm");
    DrawTerminal(*_window->InnerWriter(), {0, 0}, _window->InnerSize());

    _layer_id = layer_manager->NewLayer()
      .SetWindow(_window)
      .SetDraggable(true)
      .SetTransparentable(true)
      .ID();
    
      Print(">");
  }
  _cmd_history.resize(8);
}


Rectangle<int> Terminal::BlinkCursor(){
  _cursor_visible = !_cursor_visible;
  DrawCursor(_cursor_visible);

  return {CalcCursorPos(), {7, 15}};
}

Rectangle<int> Terminal::InputKey(uint8_t modifier, uint8_t keycode, char ascii){
  DrawCursor(false);

  Rectangle<int> draw_area{CalcCursorPos(), {8*2, 16}};
  if(ascii == '\n'){
    _linebuf[_linebuf_index] = 0;
    Log(kDebug, "line: %s\n", &_linebuf[0]);
    if (_linebuf_index > 0) {
      _cmd_history.pop_back();
      _cmd_history.push_front(_linebuf);
    }
    _linebuf_index = 0;
    _cmd_history_index = -1;

    _cursor.x = 0;
    if(_cursor.y < kRows -1 ){
      _cursor.y++;
    }else{
      ScrollOneLine();
    }
    ExecuteLine();
    Print(">");
    draw_area.pos = ToplevelWindow::kTopLeftMargin;
    draw_area.size = _window->InnerSize();
  }else if(ascii == '\b'){
    if(_cursor.x > 1){
      _cursor.x--;
      if(_show_window){
        FillRectangle(*_window->Writer(), CalcCursorPos(),{8, 16}, kTerminalBGColor);
      }
      draw_area.pos = CalcCursorPos();

      if(_linebuf_index > 0){
        _linebuf_index--;
      }
    }
  }else if(ascii !=0){
    if(_cursor.x< kColumns - 1 && _linebuf_index < kLineMax - 1){
      _linebuf[_linebuf_index++] = ascii;
      if(_show_window){
        WriteAscii(*_window->Writer(), CalcCursorPos(), ascii, kTerminalFGColor);
      }
      _cursor.x++;
    }
  } else if (keycode == 0x51) {
    // down arrow
    draw_area = HistoryUpDown(-1);
  } else if (keycode == 0x52) {
    // up arrow
    draw_area = HistoryUpDown(1);
  }

  DrawCursor(true);

  return draw_area;
}

void Terminal::DrawCursor(bool visible){
  if(_show_window){
    const auto color = visible ? kTerminalFGColor : kTerminalBGColor;
    FillRectangle(*_window->Writer(), CalcCursorPos(), {7, 15}, color);
  }
}

Vector2D<int> Terminal::CalcCursorPos() const{
  return ToplevelWindow::kTopLeftMargin +
        Vector2D<int>{2 + 8*_cursor.x, 5 + 16*_cursor.y};
}
void Terminal::ScrollOneLine(){
  Rectangle<int> move_src{
    ToplevelWindow::kTopLeftMargin + Vector2D<int>{2, 5 + 16},
    {8*kColumns, 16*(kRows - 1)}};
  _window->Move(ToplevelWindow::kTopLeftMargin + Vector2D<int>{2, 5}, move_src);
  FillRectangle(*_window->InnerWriter(),
                {2, 5 + 16*_cursor.y}, {8*kColumns, 16}, {0, 0, 0});
}

void Terminal::ExecuteLine(){
  char* command = &_linebuf[0];
  char* arg = strchr(&_linebuf[0], ' ');
  char* redir_char = strchr(&_linebuf[0], '>');
  char* pipe_char = strchr(&_linebuf[0], '|');
  if(arg){
    *arg = 0;
    arg++;
    while (isspace(*arg)){
      arg++;
    }
  }

  auto original_stdout = _files[1];
  int exit_code = 0;

  if(redir_char){
    *redir_char = 0;
    char* redir_dest = &redir_char[1];
    while(isspace(*redir_dest)){
      redir_dest++;
    }

    auto [file, post_slash] = fat32::FindFile(redir_dest);
    if(file == nullptr){
      auto [new_file, err] = fat32::CreateFile(redir_dest);
      if(err){
        PrintToFD(*_files[2],
                  "failed to create a redirect file: %s\n", err.Name());
        return;
      }
      file = new_file;
    }else if(file->attr == fat32::Attribute::kDirectory || post_slash) {
      PrintToFD(*_files[2], "cannot redirect to a directory\n");
      return;
    }
    _files[1] = std::make_shared<fat32::FileDescriptor>(*file);
  }

    std::shared_ptr<PipeDescriptor> pipe_fd;
  uint64_t subtask_id = 0;

  if (pipe_char) {
    *pipe_char = 0;
    char* subcommand = &pipe_char[1];
    while (isspace(*subcommand)) {
      subcommand++;
    }

    auto& subtask = task_manager->NewTask();
    pipe_fd = std::make_shared<PipeDescriptor>(subtask);
    auto term_desc = new TerminalDescriptor{
      subcommand, true, false,
      { pipe_fd, _files[1], _files[2] }
    };
    _files[1] = pipe_fd;

    subtask_id = subtask
        .InitContext(TaskTerminal, reinterpret_cast<int64_t>(term_desc))
        .Wakeup()
        .ID();
    (*layer_task_map)[_layer_id] = subtask_id;
  }

  if(strcmp(command, "echo") == 0){
    if (arg && arg[0] == '$') {
      if (strcmp(&arg[1], "?") == 0) {
        PrintToFD(*_files[1], "%d", _last_exit_code);
      }
    } else if(arg){
      // Print(arg);
      PrintToFD(*_files[1], "%s", arg);
    }
    // Print("\n");
    PrintToFD(*_files[1], "\n");
  }else if(strcmp(command, "clear") == 0){
    if(_show_window){
      FillRectangle(*_window->InnerWriter(),
          {2,5}, {8*kColumns, 16*kRows}, kTerminalBGColor);
    }
    _cursor.y = 0;
  }else if(strcmp(command, "lspci") == 0){
    for(int i = 0; i < pci::num_device; i++){
      const auto& dev = pci::devices[i];
      auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
      // auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);

      PrintToFD(*_files[1], "%d.%d.%d: vend %04x, head %02x class=%02x.%02x.%02x\n",
          dev.bus, dev.device, dev.function,
          vendor_id, dev.header_type,
          dev.class_code.base, dev.class_code.sub, dev.class_code.interface);

    }
  }else if(strcmp(command, "lsusb") == 0){
    char s[64];

    auto* dm = usb::xhci::xhc->DeviceManager();
    for(int i = 1; i<= usb::xhci::xhc->MaxPorts(); i++){
      auto& port = usb::xhci::xhc->PortAt(i);
      if(port.IsConnected()){
        sprintf(s, "Port %d: IsConnected",i);
        Print(s);

        auto dev= dm->FindByPort(i);
        if(dev != nullptr){
          auto if_desc = dev->Interfaces();

          sprintf(s," class=%02x, sub=%02x, protocol=%02x",
              if_desc->interface_class,
              if_desc->interface_sub_class,
              if_desc->interface_protocol);
          Print(s);
        }
        Print("\n");
       
      }
    }

  }else if(strcmp(command, "ls") == 0){
    if (!arg || arg[0] == '\0') {
      ListAllEntries(*_files[1], fat32::boot_volume_image->root_cluster);
    }else{
      auto [ dir, post_slash ] = fat32::FindFile(arg);
      if (dir == nullptr) {
        PrintToFD(*_files[2], "No such file or directory: %s\n", arg);
        exit_code = 1;
      } else if (dir->attr == fat32::Attribute::kDirectory) {
        ListAllEntries(*_files[1], dir->FirstCluster());
      } else {
        char name[13];
        fat32::FormatName(*dir, name);
        if (post_slash) {
          PrintToFD(*_files[2], "%s is not a directory\n", name);
          exit_code = 1;
        } else {
          PrintToFD(*_files[1], "%s\n", name);
        }
      }
    }
  }else if(strcmp(command, "fatdump") == 0){
    char s[64];
    uint8_t* p = reinterpret_cast<uint8_t*>(fat32::boot_volume_image);
    sprintf(s,"Volume Image:\n");
    Print(s);
    for (int i = 0; i < 16; ++i) {
      sprintf(s, "%04x:", i * 16);
      Print(s);
      for (int j = 0; j < 8; ++j) {
        sprintf(s, " %02x", *p);
        Print(s);
        p++;
      }
      Print(" ");
      for (int j = 0; j < 8; ++j) {
        sprintf(s, " %02x", *p);
        Print(s);
        p++;
      }
      Print("\n");
    }
  
  }else if(strcmp(command, "cat") == 0){
    std::shared_ptr<FileDescriptor> fd;
    if (!arg || arg[0] == '\0') {
      fd = _files[0];
    }else{
      auto [file_entry, post_slash] = fat32::FindFile(arg);
      if(!file_entry){
        PrintToFD(*_files[2], "no such file: %s\n", arg);
        exit_code = 1;
      }else if(file_entry->attr != fat32::Attribute::kDirectory && post_slash){
        char name[13];
        fat32::FormatName(*file_entry, name);
        PrintToFD(*_files[2], "%s is not a directory\n", name);
        exit_code = 1;
      }else{
        fd = std::make_shared<fat32::FileDescriptor>(*file_entry);
      }
    }
    // auto cluster = file_entry->FirstCluster();
    // auto remain_bytes = file_entry->file_size;
  
    // fat32::FileDescriptor fd{*file_entry};
    if(fd){
      char u8buf[1024];

      DrawCursor(false);
      // while (cluster != 0 && cluster != fat32::kEndOfClusterchain) {
      //   char* p = fat32::GetSectorByCluster<char>(cluster);

      //   int i = 0;
      //   for (; i < fat32::bytes_per_cluster && i < remain_bytes; i++) {
      //     Print(*p);
      //     p++;
      //   }
      //   remain_bytes -= i;
      //   cluster = fat32::NextCluster(cluster);
      //   if(cluster==fat32::kBrokenCluster){
      //     //broken
      //     Print("load fail file broken");
      //     break;
      //   }
      // }

      while (true) {
        // if (fd.Read(&u8buf[0], 1) != 1) {
        //   break;
        // }
        // const int u8_remain = CountUTF8Size(u8buf[0]) - 1;
        // if (u8_remain > 0 && fd.Read(&u8buf[1], u8_remain) != u8_remain) {
        //   break;
        // }
        // u8buf[u8_remain + 1] = 0;
        if (ReadDelim(*fd, '\n', u8buf, sizeof(u8buf)) == 0) {
          break;
        }

        // const auto [ u32, u8_next ] = ConvertUTF8To32(u8buf);
        // Print(u32 ? u32 : U'□');
        PrintToFD(*_files[1], "%s", u8buf);
      }
      DrawCursor(true);
    }
    
  }else if(strcmp(command, "noterm") == 0){
    auto term_desc = new TerminalDescriptor{
      arg, true, false, _files
    };
    task_manager->NewTask()
      .InitContext(TaskTerminal, reinterpret_cast<int64_t>(term_desc))
      .Wakeup();
  }else if(strcmp(command, "memstat") == 0){
    const auto p_stat = memory_manager->Stat();

    PrintToFD(*_files[1], "Phys used : %lu frames (%llu MiB)\n",
        p_stat.allocated_frames,
        p_stat.allocated_frames * kBytesPerFrame / 1024 / 1024);

    PrintToFD(*_files[1],  "Phys total: %lu frames (%llu MiB)\n",
        p_stat.total_frames,
        p_stat.total_frames * kBytesPerFrame / 1024 / 1024);

  }else if(strcmp(command, "date") == 0){
    EFI_TIME t;
    uefi_rts->GetTime(&t, nullptr);
    if (t.TimeZone == EFI_UNSPECIFIED_TIMEZONE) {
      PrintToFD(*_files[1], "%d-%02d-%02d %02d:%02d:%02d\n",
          t.Year, t.Month, t.Day, t.Hour, t.Minute, t.Second);
    } else {
      PrintToFD(*_files[1], "%d-%02d-%02d %02d:%02d:%02d ",
          t.Year, t.Month, t.Day, t.Hour, t.Minute, t.Second);
      if (t.TimeZone >= 0) {
        PrintToFD(*_files[1], "+%02d%02d\n", t.TimeZone / 60, t.TimeZone % 60);
      } else {
        PrintToFD(*_files[1], "-%02d%02d\n", -t.TimeZone / 60, -t.TimeZone % 60);
      }
    }
  }else if(strcmp(command, "reboot") == 0){
    uefi_rts->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, nullptr);
  }else if (strcmp(command, "poweroff") == 0){
    uefi_rts->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, nullptr);
  }else if (strcmp(command, "rm") == 0){
    char err_str[256];
    if(fat32::DeleteFile(arg, err_str)!=Error::kSuccess){
      Print("delete fail ");
      Print(err_str);
    }
  }else if(command[0] != 0){
    auto file_entry = FindCommand(command);
    if(file_entry){
        auto [ec, err] = ExecuteFile(*file_entry, command, arg);
        if(err){
          PrintToFD(*_files[2], "failed to exec file: %s\n", err.Name());
          exit_code = -ec;
        }else{
          exit_code = ec;
        }
    }else{
       PrintToFD(*_files[2], "no such command: %s\n", command);
       exit_code = 1;
    }
  }

  if (pipe_fd) {
    pipe_fd->FinishWrite();
    __asm__("cli");
    auto [ec, err] = task_manager->WaitFinish(subtask_id);
    (*layer_task_map)[_layer_id] = _task.ID();
    __asm__("sti");
    if (err) {
      Log(kWarn, "failed to wait finish: %s\n", err.Name());
    }
    exit_code = ec;
  }

  _last_exit_code = exit_code;
  _files[1] = original_stdout;
}

WithError<int> Terminal::ExecuteFile(fat32::DirectoryEntry& file_entry, char* command, char* arg){
  { 
    //unsafe test onlyhlt
    char name[13];
    fat32::FormatName(file_entry, name);
    if(strcmp(name,"ONLYHLT")==0){
      std::vector<uint8_t> file_buf(file_entry.file_size);
      fat32::LoadFile(&file_buf[0], file_buf.size(), file_entry);
      auto elf_header = reinterpret_cast<Elf64_Ehdr*>(&file_buf[0]);
      if (memcmp(elf_header->e_ident, "\x7f" "ELF", 4) != 0) {
        using NoParamsFunc = void ();
        auto f = reinterpret_cast<NoParamsFunc*>(&file_buf[0]);
        f();
        return { 0, MAKE_ERROR(Error::kSuccess) };
      }
    }
  }

  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");

  bool invalidLoadAddr =false;
  auto [app_load, err] = LoadApp(file_entry, task);
  if (err) {
     if(err.Cause() != Error::kInvalidFormatLA){
      return { 0, err };
    }
    Log(kWarn, "LoadELF:InvalidLoadAddr\n");
    invalidLoadAddr = true;
  }


  //user args
  LinearAddress4Level args_frame_addr{0xffff'ffff'ffff'f000};
  if (auto err = SetupPageMaps(args_frame_addr, 1)) {
    return { 0, err };
  }

  //save arguments
  auto argv = reinterpret_cast<char**>(args_frame_addr.value);
  // argv = 8x32 = 256 bytes
  int argv_len = 32; 
  auto argbuf = reinterpret_cast<char*>(args_frame_addr.value + sizeof(char**) * argv_len);
  int argbuf_len = 4096 - sizeof(char**) * argv_len;
  auto argc = MakeArgv(command, arg, argv, argv_len, argbuf, argbuf_len);
  if (argc.error) {
    return { 0, argc.error };
  }

  //user stack
  const int user_stack_size = 16 * 4096;
  LinearAddress4Level stack_frame_addr{0xffff'ffff'ffff'f000 - user_stack_size};
  if (auto err = SetupPageMaps(stack_frame_addr, user_stack_size/4096)) {
    return { 0, err };
  }
  

  int ret = -1;
  if(invalidLoadAddr){
    std::vector<uint8_t> file_buf(file_entry.file_size);
    fat32::LoadFile(&file_buf[0], file_buf.size(), file_entry);
    //unsafe
    uintptr_t entry_addr = app_load.entry;
    Log(kDebug, "entry_addr: %p\n",static_cast<uint64_t>(entry_addr));

    entry_addr += reinterpret_cast<uintptr_t>(&file_buf[0]);
    using AppFunc = int (int ,char**);
    auto app = reinterpret_cast<AppFunc*>(entry_addr);
    ret = app(argc.value, &argv[0]);
  }else{
    // for (int i = 0; i < 3; i++) {
    //   task.Files().push_back(
    //       std::make_unique<TerminalFileDescriptor>(task, *this));
    // }

    for (int i = 0; i < _files.size(); ++i) {
      task.Files().push_back(_files[i]);
    }

    const uint64_t elf_next_page =
        (app_load.vaddr_end + 4095) & 0xffff'ffff'ffff'f000;
    task.SetDPagingBegin(elf_next_page);
    task.SetDPagingEnd(elf_next_page);

    task.SetFileMapEnd(stack_frame_addr.value);

    ret = CallApp(argc.value, argv, 3 << 3 | 3, app_load.entry,
        stack_frame_addr.value + user_stack_size - 8,
        &task.OSStackPointer());
   
    task.Files().clear();
    task.FileMaps().clear();
  }

  // char s[64];
  // sprintf(s, "app exited. ret = %d\n", ret);
  // Print(s);

  if(app_load.entry >= 0xffff800000000000){
    // const auto addr_first = GetFirstLoadAddress(elf_header);
    // if (auto err = CleanPageMaps(LinearAddress4Level{addr_first})) {
    //   return err;
    // }
    if (auto err = CleanPageMaps(LinearAddress4Level{0xffff'8000'0000'0000})) {
      return { ret, err };
    }
    app_load.prototype->count--;
  }

  if(auto err = FreePML4(task)){
    return { ret, err };
  }

  if(app_load.prototype && app_load.prototype->count==0){
     Log(kDebug, "clean meta:%p\n",app_load.pml4);
    if (auto err = CleanPageMapsForMeta(app_load.prototype->pml4)) {
      return { ret, err };
    }

    app_loads->erase(&file_entry);
  }

  return { ret, MAKE_ERROR(Error::kSuccess) };

}


void Terminal::Print(const char* s, std::optional<size_t> len){
  const auto cursor_before = CalcCursorPos();
  DrawCursor(false);
  // if (len) {
  //   for (size_t i = 0; i < *len; ++i) {
  //     Print(*s);
  //     s++;
  //   }
  // } else {
  //   while(*s){
  //     Print(*s++);
  //   }
  // }

  size_t i = 0;
  const size_t ptrlen = len ? *len : std::numeric_limits<size_t>::max();

  while(s[i] && i < ptrlen){
    const auto [u32, bytes] = ConvertUTF8To32(&s[i]);
    Print(u32 ? u32 : U'□');
    i += bytes;
  }

  DrawCursor(true);

  const auto cursor_after = CalcCursorPos();

  Vector2D<int> draw_pos{ToplevelWindow::kTopLeftMargin.x, cursor_before.y};
  Vector2D<int> draw_size{_window->InnerSize().x,
      cursor_after.y - cursor_before.y + 16};

  Rectangle<int> draw_area{draw_pos, draw_size};

  Message msg = MakeLayerMessage(_task.ID(), _layer_id,
      LayerOperation::DrawArea, draw_area);
  __asm__("cli");
  task_manager->SendMessage(1, msg);
  __asm__("sti");
}

void Terminal::Print(char32_t c){
  if(!_show_window){
    return;
  }

  auto newline = [this]() {
    _cursor.x = 0;
    if (_cursor.y < kRows - 1) {
      _cursor.y++;
    } else {
      ScrollOneLine();
    }
  };

  if(c == U'\n'){
      newline();
  }else if(IsHankaku(c)){
    // WriteAscii(*_window->Writer(), CalcCursorPos(), c, kTerminalFGColor);
  
    // if(_cursor.x == kColumns -1){
    //   newline();
    // }else{
    //   _cursor.x++;
    // }
    if(_cursor.x == kColumns){
      newline();
    }
    WriteUnicode(*_window->Writer(), CalcCursorPos(), c, kTerminalFGColor);
    _cursor.x++;
  }else{
    if(_cursor.x >= kColumns - 1){
      newline();
    }
    WriteUnicode(*_window->Writer(), CalcCursorPos(), c, kTerminalFGColor);
    _cursor.x += 2;
  }
}

void Terminal::Redraw() {
  Rectangle<int> draw_area{ToplevelWindow::kTopLeftMargin,
      _window->InnerSize()};

  Message msg = MakeLayerMessage(
      _task.ID(), LayerID(), LayerOperation::DrawArea, draw_area);
  __asm__("cli");
  task_manager->SendMessage(1, msg);
  __asm__("sti");
}

Rectangle<int> Terminal::HistoryUpDown(int direction){
  if (direction == -1 && _cmd_history_index >= 0) {
    _cmd_history_index--;
  } else if (direction == 1 && _cmd_history_index + 1 < _cmd_history.size()) {
    _cmd_history_index++;
  }

  _cursor.x = 1;
  const auto first_pos = CalcCursorPos();

  Rectangle<int> draw_area{first_pos, {8*(kColumns - 1), 16}};
  FillRectangle(*_window->Writer(), draw_area.pos, draw_area.size, kTerminalBGColor);

  const char* history = "";
  if (_cmd_history_index >= 0) {
    history = &_cmd_history[_cmd_history_index][0];
  }

  strcpy(&_linebuf[0], history);
  _linebuf_index = strlen(history);

  WriteString(*_window->Writer(), first_pos, history, kTerminalFGColor);
  _cursor.x = _linebuf_index + 1;
  return draw_area;
}

// std::map<uint64_t, Terminal*>* terminals;

void TaskTerminal(uint64_t task_id, int64_t data){
  // const char* command_line = reinterpret_cast<char*>(data);
  //const bool show_window = command_line == nullptr;
  const auto term_desc = reinterpret_cast<TerminalDescriptor*>(data);
  bool show_window = true;
  if(term_desc){
    show_window = term_desc->show_window;
  }

  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  // Terminal* terminal = new Terminal{task, show_window};
  Terminal* terminal = new Terminal{task, term_desc};
  if(show_window){
    layer_manager->Move(terminal->LayerID(), {450, 200});
    layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
    active_layer->Activate(terminal->LayerID());
  }
  // (*terminals)[task_id] = terminal;
  __asm__("sti");

  // if (command_line) {
  //   for (int i = 0; command_line[i] != '\0'; ++i) {
  //     terminal->InputKey(0, 0, command_line[i]);
  //   }
  //   terminal->InputKey(0, 0, '\n');
  // }

  if(term_desc && !term_desc->command_line.empty()){
    for (int i = 0; i < term_desc->command_line.length(); i++) {
      terminal->InputKey(0, 0, term_desc->command_line[i]);
    }
    terminal->InputKey(0, 0, '\n');
  }

  if (term_desc && term_desc->exit_after_command) {
    delete term_desc;
    __asm__("cli");
    task_manager->Finish(terminal->LastExitCode());
    __asm__("sti");
  }

  auto add_blink_timer = [task_id](unsigned long t){
    timer_manager->AddTimer(Timer{t + static_cast<int>(kTimerFreq * 0.5),
                                  1, task_id});
  };
  add_blink_timer(timer_manager->CurrentTick());

  bool window_isactive = false;

  while (true) {
    __asm__("cli");
    auto msg = task.ReceiveMessage();
    if (!msg) {
      task.Sleep();
      __asm__("sti");
      continue;
    }
    __asm__("sti");

    switch (msg->type) {
    case Message::kTimerTimeout:
      add_blink_timer(msg->arg.timer.timeout);
      if(show_window && window_isactive){
        const auto area = terminal->BlinkCursor();
        Message msg = MakeLayerMessage(
            task_id, terminal->LayerID(), 
            LayerOperation::DrawArea, area);
        __asm__("cli");
        task_manager->SendMessage(1, msg);
        __asm__("sti");
      }
      break;
    case Message::kKeyPush:
      if (msg->arg.keyboard.press){
        const auto area = terminal->InputKey(msg->arg.keyboard.modifier,
                                             msg->arg.keyboard.keycode,
                                             msg->arg.keyboard.ascii);
        if(show_window){
          Message msg = MakeLayerMessage(
              task_id, terminal->LayerID(),
              LayerOperation::DrawArea, area);  
          __asm__("cli");
          task_manager->SendMessage(1, msg);
          __asm__("sti");
        }
      }
      break;
    case Message::kWindowActive:
      window_isactive = msg->arg.window_active.activate;
      break;
    case Message::kWindowClose:
      CloseLayer(msg->arg.window_close.layer_id);
      __asm__("cli");
      task_manager->Finish(terminal->LastExitCode());
      break;
    default:
      break;
    }
  }
}

TerminalFileDescriptor::TerminalFileDescriptor(Terminal& term)
    :  _term{term} {
}

size_t TerminalFileDescriptor::Read(void* buf, size_t len) {
  char* bufc = reinterpret_cast<char*>(buf);

  while (true) {
    __asm__("cli");
    // auto msg = _task.ReceiveMessage();
    auto msg = _term.UnderlyingTask().ReceiveMessage();
    if (!msg) {
      //_task.Sleep();
      _term.UnderlyingTask().Sleep();
      continue;
    }
    __asm__("sti");

     if (msg->type != Message::kKeyPush || !msg->arg.keyboard.press) {
      continue;
    }

    if (msg->arg.keyboard.modifier & (kLControlBitMask | kRControlBitMask)) {
      char s[3] = "^ ";
      s[1] = toupper(msg->arg.keyboard.ascii);
      _term.Print(s);
      if (msg->arg.keyboard.keycode == 7) {
        //ctrl + d
  
        //EOT
        return 0;
      }
      continue;
    }

    bufc[0] = msg->arg.keyboard.ascii;
    _term.Print(bufc, 1);
    _term.Redraw();
    return 1;
  }
}

size_t TerminalFileDescriptor::Write(const void* buf, size_t len) {
  _term.Print(reinterpret_cast<const char*>(buf), len);
  _term.Redraw();
  return len;
}


size_t TerminalFileDescriptor::Load(void* buf, size_t len, size_t offset) {
  return 0;
}

PipeDescriptor::PipeDescriptor(Task& task) : _task{task}{
}

size_t PipeDescriptor::Read(void* buf, size_t len) {
  if(_len > 0){
    const size_t copy_bytes = std::min(_len, len);
    memcpy(buf, _data, copy_bytes);
    _len -= copy_bytes;
    memmove(_data, &_data[copy_bytes], _len);
    return copy_bytes;
  }

  if(_closed){
    return 0;
  }

  while (true) {
    __asm__("cli");
    auto msg = _task.ReceiveMessage();
    if (!msg) {
      _task.Sleep();
      continue;
    }
    __asm__("sti");

    if (msg->type != Message::kPipe) {
      continue;
    }

    if (msg->arg.pipe.len == 0) {
      _closed = true;
      return 0;
    }

    const size_t copy_bytes = std::min<size_t>(msg->arg.pipe.len, len);
    memcpy(buf, msg->arg.pipe.data, copy_bytes);
    _len = msg->arg.pipe.len - copy_bytes;
    memcpy(_data, &msg->arg.pipe.data[copy_bytes], _len);
    return copy_bytes;
  }
}

size_t PipeDescriptor::Write(const void* buf, size_t len) {
  auto bufc = reinterpret_cast<const char*>(buf);
  Message msg{Message::kPipe};
  size_t sent_bytes = 0;
  while (sent_bytes < len) {
    msg.arg.pipe.len = std::min(len - sent_bytes, sizeof(msg.arg.pipe.data));
    memcpy(msg.arg.pipe.data, &bufc[sent_bytes], msg.arg.pipe.len);
    sent_bytes += msg.arg.pipe.len;
    __asm__("cli");
    _task.SendMessage(msg);
    __asm__("sti");
  }
  return len;
}

void PipeDescriptor::FinishWrite() {
  Message msg{Message::kPipe};
  msg.arg.pipe.len = 0;
  __asm__("cli");
  _task.SendMessage(msg);
  __asm__("sti");
}