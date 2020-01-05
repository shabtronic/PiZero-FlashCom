# PiZero-FlashCom
Simple USB-Uart flash com server, sits ontop of the awesome Circle Bare metal lib (https://github.com/rsta2/circle)

Two Parts:


1) FlashCom.c - PC side console program/

This will display any debug info from the logger(if contructed with a CSerial device)

Will also upload files to the PI and reboot:

speed is fixed to 11520x8

com port is auto detected

FlashCom Kernel.img reboot

will connect to PI - upload kernel.img - write to sdcard and reboot the PI :)

2) Main.cpp - Circle lib main that has the "Flash" server

Waits for commands from the PC.

Three basic internal commands

PICMD:HELO
  PI will send back "PI: We are Connected!\n" - FlashCom uses this to auto detect COM port.

PICMD:RBOO
  PI will Reboot!

PICMD:FILE
  PI will wait for file info in this simple csv format:
  
  FileName,FileSize,CRC32,RawFileData
  Example:
  Kernel.img,11200,2342526262,---data------
  PI will then write file to SDCard
