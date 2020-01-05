//
// main.c
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014  R. Stange <rsta2@o2online.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "kernel.h"
#include <circle/startup.h>
#define ANSIRED "\033[0;31m"
#define ANSIGREEN "\033[0;32m"
#define ANSIWHITE "\033[0m"
extern int atoi(char *str);

unsigned int crc32_for_byte(unsigned int r)
	{
	for (int j = 0; j < 8; j++)
		r = (r & 1 ? 0 : (unsigned int)0xEDB88320L) ^ r >> 1;
	return r ^ (unsigned int)0xFF000000L;
	}

unsigned int crc32(const void *data, size_t n_bytes)
	{
	unsigned int crc = 0;
	static unsigned int table[0x100] = { 0 };
	if (!*table)
		for (int i = 0; i < 0x100; i++)
			table[i] = crc32_for_byte(i);
	for (size_t i = 0; i < n_bytes; ++i)
		crc = table[(unsigned char)crc ^ ((unsigned char*)data)[i]] ^ crc >> 8;
	return crc;
	}

CKernel Kernel;
int I2CWrite(int addr, unsigned char* buf, int len)
	{
	return Kernel.m_I2CMaster.Write(addr, buf, len);
	}

void WaitRec(char* buf, int size, bool AddNull = true)
	{
	while (size)
		if (Kernel.m_Serial.Read(buf, 1) > 0)
			{
			buf++;
			size--;
			}
	if (AddNull)
		*buf = 0;
	}
CString WaitRecDelim(char delimiter)
	{
	CString str = "";
	char lastchar[2] = { 0, 0 };
	while (*lastchar != delimiter)
		{
		if (Kernel.m_Serial.Read(lastchar, 1) == 1)
			{
			lastchar[1] = 0;
			if (*lastchar != delimiter)
				str.Append(lastchar);
			}
		}
	return str;
	}
void EmptyFifo()
	{
	char lastchar[2];
	while (Kernel.m_Serial.Read(lastchar, 1) == 1);
	}
void Consume(char *eatme)
	{
	char* teatme = eatme;
	char byte;
	while (*teatme)
		{
		if (Kernel.m_Serial.Read(&byte, 1) == 1)
			if (byte == *teatme)
				teatme++;
			else
				teatme = eatme;
		}
	}

bool ConsumeBytes(char* data, int count)
	{
	int timeout = 0;
	int cc = 0;
	while (count > 0)
		{
		if (Kernel.m_Serial.Read(data, 1) == 1)
			{
			data++;
			count--;
			}
		cc++;
		if ((cc & 0xffffff) == 0xffffff)
			{
			Kernel.m_Serial.printf("Stuck in ConsumeBytes! %d Count: %d\n", cc, count);
			Kernel.m_Serial.Flush();
			}
		}
	if (count == 0) return true;
	return false;
	}
static volatile bool TimeRun = false;
static volatile int TimeInst = 0;
//void TimerHandler(TKernelTimerHandle hTimer, void *pParam, void *pContext)
static volatile bool SerialProcess = false;
void SerialHandler()
	{
	if (!SerialProcess) return;
	//	Kernel.m_Serial.printf("Timer Handler\n");
	//	return;
	if (TimeRun) return;
	TimeRun = true;
	TimeInst++;
	char sbuf[1024];
	// Eat the Magic String
//	WaitRec(sbuf, 6);
	Consume("PICMD:");
	// Get command
	WaitRec(sbuf, 4);
	if (strstr(sbuf, "HELO") > 0)
		Kernel.m_Serial.printf("PI: We are Connected!\n");
	if (strstr(sbuf, "FILE") > 0)
		{
		CString filename = WaitRecDelim(',');
		CString filesize = WaitRecDelim(',');
		CString filesize2 = WaitRecDelim(',');
		CString filecrc32 = WaitRecDelim(',');
		CString filetype = WaitRecDelim(',');
		int fsize = atoi(filesize.c_str());
		int ofsize = fsize;
		unsigned int crc = atoi(filecrc32.c_str());
		char* filedata = (char*)malloc(fsize);
		char* tfiledata = filedata;
		int chunk = 0;
		while (fsize > 0)
			{
			if (fsize > 1024)
				{
				if (ConsumeBytes(tfiledata, 1024))
					{
					tfiledata += 1024;
					fsize -= 1024;
					Kernel.m_Serial.printf("FOK");
					}
				else
					{
					Kernel.m_Serial.printf("PI: Error receiveing file.\n");
					fsize = 0;
					}
				}
			else
				{
				if (ConsumeBytes(tfiledata, fsize))
					{
					fsize = 0;
					Kernel.log.printf("FOK");
					}
				else
					{
					Kernel.m_Serial.printf("PI: Error receiveing file.\n");
					fsize = 0;
					}

				}
			chunk++;
			}
		unsigned int freshcrc = crc32(filedata, ofsize);
		Kernel.m_Serial.printf("PI: file data received OK!\n");
		Kernel.m_Serial.printf("PI: sent crc %u my crc %u\n", crc, freshcrc);

		if (crc == freshcrc)
			{
			Kernel.m_Serial.printf("PI:"ANSIGREEN" Success crc's match - writing file"ANSIWHITE"\n");
			FIL File;
			f_open(&File, filename.c_str(), FA_WRITE | FA_CREATE_ALWAYS);
			f_write(&File, filedata, ofsize, 0);
			f_close(&File);
			}
		else
			{
			Kernel.m_Serial.printf("PI:"ANSIRED" Error crc's dont match!"ANSIWHITE"\n");
			Kernel.m_Serial.printf("PI:"ANSIRED" writing as badfile.bad"ANSIWHITE"\n");
			FIL File;
			f_open(&File, "Badfile.bad", FA_WRITE | FA_WRITE| FA_CREATE_NEW);
			f_write(&File, filedata, ofsize, 0);
			f_close(&File);

			}
		if (filedata)
			free(filedata);
		}
	if (strstr(sbuf, "RBOO") > 0) // Reboot the PI!
		{
		Kernel.m_Serial.printf("PI: Rebooting!\n");
		Kernel.m_Serial.Flush();
		reboot();
		}
	EmptyFifo();
	TimeInst--;
	TimeRun = false;
	SerialProcess = false;
	}

// MagicHandler()
// this is called from inside the kernel serial interrupt
// function, so we need to kick off a timer function to
// be able to read/write serial data without blocking
// up the kernel interrupt.

void MagicHandler()
	{
	SerialProcess = true;
	//	Kernel.m_Timer.StartKernelTimer(30, TimerHandler);
	}

int main(void)
	{
	if (!Kernel.Initialize())
		{
		halt();;
		return EXIT_HALT;
		}
	Kernel.m_Serial.RegisterMagicReceivedHandler("PICMD:", MagicHandler);
	//	Kernel.m_Timer.StartKernelTimer(30, TimerHandler);

	TShutdownMode ShutdownMode = Kernel.Run();

	switch (ShutdownMode)
		{
		case ShutdownReboot:
			reboot();
			return EXIT_REBOOT;

		case ShutdownHalt:
		default:
			halt();
			return EXIT_HALT;
		}
	}
