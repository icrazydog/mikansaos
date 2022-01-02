typedef unsigned short CHAR16;
typedef unsigned long long EFI_STATUS;
typedef unsigned long long UINTN;
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef short INT16;
typedef unsigned long UINT32;
typedef void *EFI_HANDLE;

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCAL;
typedef EFI_STATUS (*EFI_TEXT_STRING)(
  struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCAL *This,
  CHAR16                                  *String);

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCAL {
  void *dummy;
  EFI_TEXT_STRING OutputString;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCAL;

typedef struct {
UINT16 Year; // 1900 – 9999
UINT8 Month; // 1 – 12
UINT8 Day; // 1 – 31
UINT8 Hour; // 0 – 23
UINT8 Minute; // 0 – 59
UINT8 Second; // 0 – 59
UINT8 Pad1;
UINT32 Nanosecond; // 0 – 999,999,999
INT16 TimeZone; // -1440 to 1440 or 2047
UINT8 Daylight;
UINT8 Pad2;
} EFI_TIME;

typedef struct {
  UINT32 Resolution;
  UINT32 Accuracy;
  unsigned char SetsToZero;
} EFI_TIME_CAPABILITIES;

typedef EFI_STATUS (*EFI_GET_TIME) (
  EFI_TIME *Time,
  EFI_TIME_CAPABILITIES *Capabilities
);

typedef enum {
  EfiResetCold,
  EfiResetWarm,
  EfiResetShutdown,
  EfiResetPlatformSpecific
} EFI_RESET_TYPE;

typedef void (*EFI_RESET_SYSTEM) (
 EFI_RESET_TYPE ResetType,
 EFI_STATUS ResetStatus,
 UINTN DataSize,
 void *ResetData
);

typedef struct { 
  char dummy2[24];
  EFI_GET_TIME GetTime;
  char dummy[72];
  EFI_RESET_SYSTEM ResetSystem;
} EFI_RUNTIME_SERVICES;

typedef struct {
  char dummy[52];
  EFI_HANDLE ConsoleOutHandle;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCAL *ConOut;
  void *dummy2;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCAL *dummy3;
  EFI_RUNTIME_SERVICES *RuntimeServices;
} EFI_SYSTEM_TABLE;

int itoa(int value,CHAR16 *ptr)
     {
        int count=0,temp;
        if(ptr==0)
            return 0;   
        if(value==0)
        {   
            *ptr='0';
            return 1;
        }

        if(value<0)
        {
            value*=(-1);    
            *ptr++='-';
            count++;
        }
        for(temp=value;temp>0;temp/=10,ptr++);
        *ptr='\0';
        for(temp=value;temp>0;temp/=10)
        {
            *--ptr=temp%10+'0';
            count++;
        }
        return count;
     }

EFI_STATUS EfiMain(EFI_STATUS ImageHandle,
                   EFI_SYSTEM_TABLE *SystemTable) {
  SystemTable->ConOut->OutputString(SystemTable->ConOut, L"hello\n");
  
  EFI_TIME now;
  int lastSecond;
  int count=10;
  CHAR16 strCount[5];
  while(count){
    SystemTable->RuntimeServices->GetTime(&now,0);
    if(lastSecond != now.Second){
      lastSecond = count;
      while(lastSecond%10!=0){
        lastSecond = lastSecond/10;
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"\b");
      }

      lastSecond = now.Second;
      count--;
      
      itoa(count, strCount);
      SystemTable->ConOut->OutputString(SystemTable->ConOut, strCount);
    }
  }
  SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,0);
  while(1);
  return 0;
 }
