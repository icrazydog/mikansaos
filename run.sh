#!/bin/sh


EFI_FILE=BOOTX64.EFI
KERNEL_FILE=$2
if [ $# -ge 1 ]
then
  EFI_FILE=$1
fi

rm -f disk.img
qemu-img create -f raw disk.img 200M
mkfs.fat -n 'MIKAN SAOS' -s 2 -f 2 -R 32 -F 32 disk.img


sudo mkdir mnt
sudo mount -o loop disk.img mnt
sudo rm -rf mnt/EFI/BOOT/*
sudo mkdir -p mnt/EFI/BOOT
sudo cp -rf $EFI_FILE mnt/EFI/BOOT/BOOTX64.EFI
if [ "$KERNEL_FILE" != "" ]
then
  sudo cp -f $KERNEL_FILE mnt/
# sudo apt install fatattr
  sudo fatattr +s mnt/$KERNEL_FILE
fi

if [ -f HelloWorld.data ]
then
sudo cp -rf HelloWorld.data mnt/
fi

sudo cp -f resource/* mnt/

APPS_DIR="apps/"
if [ "$APPS_DIR" != "" ]
then
  sudo mkdir mnt/$APPS_DIR
fi

for APP in $(ls "apps")
do
  if [ -f "apps/$APP/$APP" ]
  then
    sudo cp "apps/$APP/$APP" mnt/$APPS_DIR
  fi
done

sleep 0.5
sudo umount mnt
sleep 0.5
sudo rm -rf mnt

qemu-system-x86_64 \
  -m 1.5G \
  -vga std \
  -soundhw hda \
  -monitor stdio \
  -device nec-usb-xhci,id=xhci \
  -device usb-mouse \
  -device usb-kbd \
  -device usb-storage,bus=xhci.0,drive=stick \
  -drive if=pflash,format=raw,file=$HOME/mikansaos/env/qemu/OVMF_CODE.fd \
  -drive if=pflash,format=raw,file=$HOME/mikansaos/env/qemu/OVMF_VARS.fd \
  -drive file=disk.img,format=raw,index=0,media=disk \
  -drive if=none,id=stick,format=raw,file=usbstick.img 
#  -s -S           
#gdb target remote localhost:1234 
