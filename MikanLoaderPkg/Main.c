#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include  <Library/UefiRuntimeServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/LoadedImage.h>
//#include <Protocol/SimpleFileSystem.h>
//#include <Protocol/DiskIo2.h>
#include <Protocol/BlockIo.h>
#include <Guid/FileInfo.h>
#include "frame_buffer_config.hpp"
#include "elf.h"
#include "memory_map.hpp"


EFI_STATUS GetMemoryMap(struct MemoryMap* map) {
  if(NULL == map->buffer){
    return EFI_BUFFER_TOO_SMALL;
  }

  map->map_size  = map->buffer_size;
  return gBS->GetMemoryMap(
      &map->map_size,
      (EFI_MEMORY_DESCRIPTOR*)map->buffer,
      &map->map_key,
      &map->descriptor_size,
      &map->descriptor_version);
}


const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type) {
  switch (type) {
    case EfiReservedMemoryType: return (CHAR16 *)L"EfiReservedMemoryType";
    case EfiLoaderCode: return (CHAR16 *)L"EfiLoaderCode";
    case EfiLoaderData: return (CHAR16 *)L"EfiLoaderData";
    case EfiBootServicesCode: return (CHAR16 *)L"EfiBootServicesCode";
    case EfiBootServicesData: return (CHAR16 *)L"EfiBootServicesData";
    case EfiRuntimeServicesCode: return (CHAR16 *)L"EfiRuntimeServicesCode";
    case EfiRuntimeServicesData: return (CHAR16 *)L"EfiRuntimeServicesData";
    case EfiConventionalMemory: return (CHAR16 *)L"EfiConventionalMemory";
    case EfiUnusableMemory: return (CHAR16 *)L"EfiUnusableMemory";
    case EfiACPIReclaimMemory: return (CHAR16 *)L"EfiACPIReclaimMemory";
    case EfiACPIMemoryNVS: return (CHAR16 *)L"EfiACPIMemoryNVS";
    case EfiMemoryMappedIO: return (CHAR16 *)L"EfiMemoryMappedIO";
    case EfiMemoryMappedIOPortSpace: return (CHAR16 *)L"EfiMemoryMappedIOPortSpace";
    case EfiPalCode: return (CHAR16 *)L"EfiPalCode";
    case EfiPersistentMemory: return (CHAR16 *)L"EfiPersistentMemory";
    case EfiMaxMemoryType: return (CHAR16 *)L"EfiMaxMemoryType";
    default: return (CHAR16 *)L"InvalidMemoryType";
  }
}


EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file){
  CHAR8 buf[256];
  UINTN len;

  CHAR8* header = (CHAR8*) "Index, Type, Type(name,PhysicalStart, NumberOfPages, Attribute\n";
  len = AsciiStrLen(header);
  file->Write(file, &len, header);

  Print((CHAR16*)L"map->buffer = %08lx, map->map_size = %08lx\n",
      map->buffer, map->map_size);
  
  EFI_PHYSICAL_ADDRESS iter;
  int i;
  for(iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i = 0;
      iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size;
      iter += map->descriptor_size, i++){
    EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)iter;
    len = AsciiSPrint(buf, sizeof(buf),
        "%u, %x, %-ls, %08lx, %lx, %lx\n",
        i, desc->Type, GetMemoryTypeUnicode((EFI_MEMORY_TYPE)desc->Type),
        desc->PhysicalStart, desc->NumberOfPages,desc->Attribute & 0xffffflu);
    file->Write(file, &len, buf);
  }
  
  return EFI_SUCCESS;
}


EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root){
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

  gBS->OpenProtocol(image_handle,
      &gEfiLoadedImageProtocolGuid,
      (VOID**)&loaded_image,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

  gBS->OpenProtocol(loaded_image->DeviceHandle,
      &gEfiSimpleFileSystemProtocolGuid,
      (VOID**)&fs,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  
  fs->OpenVolume(fs, root);

  return EFI_SUCCESS;
}


EFI_STATUS OpenGOP(EFI_HANDLE image_handle,
    EFI_GRAPHICS_OUTPUT_PROTOCOL** gop){
  UINTN num_gop_handles = 0;
  EFI_HANDLE* gop_handles = (EFI_HANDLE*)0;
  gBS->LocateHandleBuffer(ByProtocol,
      &gEfiGraphicsOutputProtocolGuid,
      NULL,
      &num_gop_handles,
      &gop_handles);

  gBS->OpenProtocol(gop_handles[0],
      &gEfiGraphicsOutputProtocolGuid,
      (VOID**)gop,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

  gBS->FreePool(gop_handles);

  return EFI_SUCCESS;
}


const CHAR16* GetPixelFotmatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt){
  switch(fmt){
    case PixelRedGreenBlueReserved8BitPerColor:
      return (CHAR16*)L"PixelRedGreenBlueReserved8BitPerColor";
    case PixelBlueGreenRedReserved8BitPerColor:
      return (CHAR16*)L"PixelBlueGreenRedReserved8BitPerColor";
    case PixelBitMask:
      return (CHAR16*)L"PixelBitMask";
    case PixelBltOnly:
      return (CHAR16*)L"PixelBltOnly";
    case PixelFormatMax:
      return (CHAR16*)L"PixelFormatMax";
    default:
      return (CHAR16*)L"InvalidPixelFormat";
  }
}


void Halt(void) {
  while(1) __asm__("hlt");
}


void CalcLoadAddressRange(Elf64_Ehdr* ehdr, UINT64* first, UINT64* last){
  Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
  *first = MAX_UINT64;
  *last = 0;
  for(Elf64_Half i = 0; i < ehdr->e_phnum; i++){
    if(phdr[i].p_type != PT_LOAD) continue;
    *first = MIN(*first, phdr[i].p_vaddr);
    *last = MAX(*last, phdr[i].p_vaddr + phdr[i].p_memsz);
  }
}

void CopyLoadSegments(Elf64_Ehdr* ehdr){
  Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
  for(Elf64_Half i = 0; i < ehdr->e_phnum;i++){
    if(phdr[i].p_type != PT_LOAD) continue;

    UINT64 segm_in_file = (UINT64)ehdr + phdr[i].p_offset;
    CopyMem((VOID*)phdr[i].p_vaddr, (VOID*)segm_in_file,phdr[i].p_filesz);
    
    UINTN remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;
    SetMem((VOID*)(phdr[i].p_vaddr + phdr[i].p_filesz), remain_bytes, 0);
  }
}


EFI_STATUS ReadFile(EFI_FILE_PROTOCOL* file, VOID** buffer) {
  EFI_STATUS status;
  UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
  UINT8 file_info_buffer[file_info_size];
  status = file->GetInfo(file, &gEfiFileInfoGuid,
      &file_info_size, file_info_buffer);
  if(EFI_ERROR(status)){
    // Print((CHAR16*)L"failed to get kernel file information: %r\n", status);
    return status;
  }

  EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer;
  UINTN file_size = file_info->FileSize;


  status = gBS->AllocatePool(EfiLoaderData, file_size, buffer);
  if(EFI_ERROR(status)){
    //Print((CHAR16*)L"failed to allocate pool: %r\n", status);
    return status;
  }

  return file->Read(file, &file_size, *buffer);
}

EFI_STATUS OpenBlockIoProtocolForLoadedImage(
    EFI_HANDLE image_handle, EFI_BLOCK_IO_PROTOCOL** block_io) {
  EFI_STATUS status;
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image;

  status = gBS->OpenProtocol(
      image_handle,
      &gEfiLoadedImageProtocolGuid,
      (VOID**)&loaded_image,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if (EFI_ERROR(status)) {
    return status;
  }

  status = gBS->OpenProtocol(
    loaded_image->DeviceHandle,
    &gEfiBlockIoProtocolGuid,
    (VOID**)block_io,
    image_handle,
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  
  return status;
}

EFI_STATUS ReadBlocks(
    EFI_BLOCK_IO_PROTOCOL* block_io, UINT32 media_id,
    UINTN read_bytes, VOID** buffer) {
  EFI_STATUS status;

  status = gBS->AllocatePool(EfiLoaderData, read_bytes, buffer);
  if (EFI_ERROR(status)) {
    return status;
  }

  status = block_io->ReadBlocks(
    block_io,
    media_id,
    // start LBA
    0, 
    read_bytes,
    *buffer);

  return status;
}


EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE* system_table) {
  EFI_STATUS status;

  Print((CHAR16 *)L"Hello Mikan!!\n");

  CHAR8 memmap_buf[4096*4];
  struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
  status = GetMemoryMap(&memmap);
  if(EFI_ERROR(status)){
    Print((CHAR16*)L"failed to get memory map: %r\n", status);
    Halt();
  }

  EFI_FILE_PROTOCOL* root_dir;
  status = OpenRootDir(image_handle, &root_dir);
  if(EFI_ERROR(status)){
    Print((CHAR16*)L"failed to open root directory: %r\n", status);
    Halt();
  } 

  EFI_FILE_PROTOCOL* memmap_file;
  status = root_dir->Open(root_dir, &memmap_file,(CHAR16*)L"\\memmap",
      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if(EFI_ERROR(status)){
    Print((CHAR16*)L"failed to open file memmap: %r\n", status);
    Print((CHAR16*)L"Ignored\n");
  }else{

    status = SaveMemoryMap(&memmap, memmap_file);
    if(EFI_ERROR(status)){
      Print((CHAR16*)L"failed to save memory map: %r\n", status);
      Halt();
    }  

    status = memmap_file->Close(memmap_file);
    if(EFI_ERROR(status)){
      Print((CHAR16*)L"failed to close memory map: %r\n", status);
      Halt();
    } 
  }
  //open gop
  EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
  status = OpenGOP(image_handle, &gop);
  if(EFI_ERROR(status)){
    Print((CHAR16*)L"failed to open GOP: %r\n", status);
    Halt();
  }

  //change resolution
  UINT32 hdMode = 0;
  UINT32 maxMode = gop->Mode->MaxMode;
  UINT32 currentMode = gop->Mode->Mode;
  Print((CHAR16*)L"maxMode: %u current:%u\n",maxMode, currentMode);

  UINTN  sizeOfInfo = 0;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = NULL;
  for(UINT32 i=0; i< maxMode; i++){
    status =gop->QueryMode(gop, i, &sizeOfInfo, &info);

    if(EFI_ERROR(status)){
      Print((CHAR16*)L"mode:%u failed to QueryMode: %r\n", i, status);
      Halt();
    }else{
      Print((CHAR16*)L"sizeOfInfo: %u\n",sizeOfInfo);
      Print((CHAR16*)L"mode:%u Resolution: %ux%u", 
          i,
          info->HorizontalResolution,
          info->VerticalResolution);
      if( info->HorizontalResolution == 1280 && 
          info->VerticalResolution ==720){
            hdMode = i;
          }
    }
  }

  if(hdMode !=0){
    status = gop->SetMode(gop,hdMode);
    if(EFI_ERROR(status)){
        Print((CHAR16*)L"mode:%u failed to SetMode: %r\n", hdMode, status);
        Halt();
    }
  }

  UINT8* frame_buffer = (UINT8*)gop->Mode->FrameBufferBase;
  //for(UINTN i = 0; i < gop->Mode->FrameBufferSize;i++){
  for(UINTN i = 0;
      i < gop->Mode->Info->HorizontalResolution*
        gop->Mode->Info->VerticalResolution * 4;
      i++){
    frame_buffer[i] = 255;
  }
  
  Print((CHAR16*)L"Resolution: %ux%u, Pixel Format: %s, %u pixelsPerLine\n",
      gop->Mode->Info->HorizontalResolution,
      gop->Mode->Info->VerticalResolution,
      GetPixelFotmatUnicode(gop->Mode->Info->PixelFormat),
      gop->Mode->Info->PixelsPerScanLine);
  Print((CHAR16*)L"Frame Buffer:0x%0lx - 0x%0lx, size: %lu bytes\n",
      gop->Mode->FrameBufferBase,
      gop->Mode->FrameBufferBase + gop->Mode->FrameBufferSize,
      gop->Mode->FrameBufferSize); 

  //load kernel
  EFI_FILE_PROTOCOL* kernel_file;
  status = root_dir->Open(root_dir, &kernel_file, (CHAR16*)L"\\kernel.elf",
      EFI_FILE_MODE_READ, 0);
  if(EFI_ERROR(status)){
    Print((CHAR16*)L"failed to open kernel file: %r\n", status);
    Halt();
  }

  //read kernel
  VOID* kernel_buffer;
  status = ReadFile(kernel_file, &kernel_buffer);
  if(EFI_ERROR(status)){
    Print((CHAR16*)L"failed to read kernel: %r\n", status);
    Halt();
  }

  //alloc page for kernel
  /*EFI_PHYSICAL_ADDRESS kernel_base_addr = 0x100000;
    status = kernel_file->Read(kernel_file, &kernel_file_size, (VOID*)kernel_base_addr);
    if(EFI_ERROR(status)){
      Print((CHAR16*)L"failed to load kernel: %r\n", status);
      Halt();
    }
  */
  Elf64_Ehdr* kernel_ehdr = (Elf64_Ehdr*)kernel_buffer;
  UINT64 kernel_first_addr, kernel_last_addr;
  CalcLoadAddressRange(kernel_ehdr, &kernel_first_addr, &kernel_last_addr);

  UINTN num_pages = (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000;
  status = gBS->AllocatePages(AllocateAddress, EfiLoaderData,
      num_pages, &kernel_first_addr);
  if(EFI_ERROR(status)){
    Print((CHAR16*)L"failed to allocate pages: %r\n", status);
    Halt();
  }
  
  //copy segments
  CopyLoadSegments(kernel_ehdr);
  Print((CHAR16*)L"kernel:0x%0lx - 0x%0lx (%lu bytes)\n",
      kernel_first_addr,
      kernel_last_addr,
      kernel_last_addr-kernel_first_addr);

  //free kernel image
  status = gBS->FreePool(kernel_buffer);
  if (EFI_ERROR(status)) {
    Print(L"failed to free pool: %r\n", status);
    Halt();
  }

  //read volume
  VOID* volume_image;

  EFI_FILE_PROTOCOL* volume_file;
  status = root_dir->Open(
      root_dir, &volume_file, L"\\fat_disk",
      EFI_FILE_MODE_READ, 0);
  if(status == EFI_SUCCESS){
    //read from file
    status = ReadFile(volume_file, &volume_image);
    if (EFI_ERROR(status)) {
      Print(L"failed to read volume file: %r", status);
      Halt();
    }
  }else{
    //read from block io media
    EFI_BLOCK_IO_PROTOCOL* block_io;
    status = OpenBlockIoProtocolForLoadedImage(image_handle, &block_io);
    if (EFI_ERROR(status)) {
      Print(L"failed to open Block I/O Protocol: %r\n", status);
      Halt();
    }

    EFI_BLOCK_IO_MEDIA* media = block_io->Media;
    UINTN volume_bytes = (UINTN)media->BlockSize * (media->LastBlock + 1);
    if (volume_bytes > 32 * 1024 * 1024) {
      //note:may cause read fail when file great than 16kb
      volume_bytes = 32 * 1024 * 1024;
    }
    Print(L"Reading %lu bytes (Present %d, BlockSize %u, LastBlock %u)\n",
        volume_bytes, media->MediaPresent, media->BlockSize, media->LastBlock);

    status = ReadBlocks(block_io, media->MediaId, volume_bytes, &volume_image);
    if (EFI_ERROR(status)) {
      Print(L"failed to read blocks: %r\n", status);
      Halt();
    }

  }


  

  //exit bs
  status = gBS->ExitBootServices(image_handle, memmap.map_key);
  if(EFI_ERROR(status)){
    status = GetMemoryMap(&memmap);
    if(EFI_ERROR(status)){
      Print((CHAR16*)L"failed to get memory map: %r\n", status);
      Halt();
    }
    status = gBS->ExitBootServices(image_handle, memmap.map_key);
    if(EFI_ERROR(status)){
      Print((CHAR16*)L"Could not exit boot service: %r\n", status);
      Halt();
    }
  }

  //init frame_buffer_config
  struct FrameBufferConfig config = {
    (UINT8*)gop->Mode->FrameBufferBase,
    gop->Mode->Info->PixelsPerScanLine,
    gop->Mode->Info->HorizontalResolution,
    gop->Mode->Info->VerticalResolution,
    (enum PixelFormat)0
  };

  switch(gop->Mode->Info->PixelFormat){
    case PixelRedGreenBlueReserved8BitPerColor:
      config.pixel_format = kPixelRGBResv8BitPerColor;
      break;
    case PixelBlueGreenRedReserved8BitPerColor:
      config.pixel_format = kPixelBGRResv8BitPerColor;
      break;
    default:
      Print((CHAR16*)L"Unimplemented pixel format: %d\n", gop->Mode->Info->PixelFormat);
      Halt();
  }

  //acpi table(for acpi pm)
  VOID* acpi_table = NULL;
  for(UINTN i = 0; i < system_table->NumberOfTableEntries; i++){
    if(CompareGuid(&gEfiAcpiTableGuid,
                    &system_table->ConfigurationTable[i].VendorGuid)){
      acpi_table = system_table->ConfigurationTable[i].VendorTable;
    }
  }

  //call kernel
  UINT64 entry_addr = *(UINT64*)(kernel_first_addr + 24);

  typedef void EntryPointType(const struct FrameBufferConfig*,
                              const struct MemoryMap*,
                              const VOID*,
                              VOID*,
                              EFI_RUNTIME_SERVICES*);
  EntryPointType* entry_point = (EntryPointType*)entry_addr;
  entry_point(&config,&memmap, acpi_table, volume_image, gRT);


  Print((CHAR16 *)L"ALL done!\n");
  
  while(1);
  return EFI_SUCCESS;
}
