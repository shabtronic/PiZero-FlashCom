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

CKernel Kernel;
int I2CWrite(int addr, unsigned char* buf, int len)
	{
	return Kernel.m_I2CMaster.Write(addr, buf, len);
	}

// FileSize00000000
// CheckSum00000000
// FileData000000000000000000
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
void WaitRecDelim(char* buf, char delimiter, bool AddNull = true)
	{
	char lastchar = 0;
	while (lastchar=delimiter)
		{ }
		if (Kernel.m_Serial.Read(&lastchar, 1) > 0)
			{
			if (lastchar!=delimiter)
			*buf++= lastchar;
			}
	if (AddNull)
		*buf = 0;
	}
void TimerHandler(TKernelTimerHandle hTimer, void *pParam, void *pContext)
	{
	char sbuf[1024];
	//Get Magic String
	WaitRec(sbuf, 6);
	// Get command
	WaitRec(sbuf, 4);
	Kernel.log.printf("PI: Cmd-%s\n", sbuf);
	if (strstr(sbuf, "HELO"))		
		Kernel.log.printf("PI: We are Connected!\n");
	if (strstr(sbuf, "FILE"))
		{
		CString filename = "";
		}
	if (strstr(sbuf, "RBOO")) // Reboot the PI!
		{
		reboot();
		}
	}

// this is called from inside the kernel serial interrupt
// function, so we need to kick off a timer function to
// be able to read/write serial data without blocking
// up the kernel interrupt.

void MagicHandler()
	{
	Kernel.m_Timer.StartKernelTimer(1, TimerHandler);	
	}

int main(void)
	{


	if (!Kernel.Initialize())
		{
		halt();;
		return EXIT_HALT;
		}
	//	Kernel.m_Serial.RegisterMagicReceivedHandler("KernelFile:", SerialRec);
	


	Kernel.m_Serial.RegisterMagicReceivedHandler("PICMD:", MagicHandler);

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
