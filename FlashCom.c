#include <windows.h>
#include <stdio.h>
#include <string.h>

const int UartSpeed = 115200 * 8;

#define ConsumeTimeout 5

#define ORIGIN "PC: "
#define ANSIBLACK "\033[0;30m"
#define ANSIRED "\033[0;31m"
#define ANSIGREEN "\033[0;32m"
#define ANSIYELLOW "\033[0;33m"
#define ANSIBLUE "\033[0;34m"
#define ANSIMAGENTA "\033[0;35m"
#define ANSICYAN "\033[0;36m"
#define ANSIWHITE "\033[0m"

// PICOHELO - pi will return PIOK
// PICORBOO - pi will reboot
// PICOFILE - pi will expect file info and transfer
//		FileName,FileSize,FileSize2,CRC32,Type,.....raw file data in 1024 byte chunks....
//		pi will send back PIOK after every 1024 byte chunk

char cornerset[] = "│─┐└┘┌";
char conerset2[] = "║═╗╚╝╔";
unsigned int crc32(const void *data, unsigned int n_bytes)
	{
	unsigned int crc = 0;
	static unsigned int table[0x100] = { 0 };
	if (!*table)
		for (unsigned int i = 0; i < 0x100; i++)
			{
			unsigned int xi = i;
			for (int j = 0; j < 8; j++)
				xi = (xi & 1 ? 0 : (unsigned int)0xEDB88320L) ^ xi >> 1;
			table[i] = (xi ^ (unsigned int)0xFF000000L);
			}
	while (n_bytes--)
		crc = table[(unsigned char)crc ^ (*(unsigned char*)data++)] ^ crc >> 8;
	return crc;
	}



HANDLE OpenPort(char* Name)
	{
	HANDLE hSerial = CreateFile(Name, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hSerial == INVALID_HANDLE_VALUE)
		{
		printf(ANSIRED"Error opening: %s\n"ANSIWHITE, Name);
		return 0;
		}
	return hSerial;
	}

int FileExists(char* filename)
	{
	FILE *F = fopen(filename, "rb");
	if (F)
		{
		fclose(F);
		return 1;
		}
	return 0;
	}

void PortConfig(HANDLE port, int Speed)
	{
	DCB dcbSerialParams = { 0 };
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
	GetCommState(port, &dcbSerialParams);
	dcbSerialParams.BaudRate = (DWORD)Speed;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;
SetCommState(port, &dcbSerialParams);

	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 50;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 50;
	timeouts.WriteTotalTimeoutMultiplier = 10;
	SetCommTimeouts(port, &timeouts);
	}


double GetTime()
	{
	__int64 time1 = 0, time2 = 0, freq = 0;
	QueryPerformanceCounter((LARGE_INTEGER *)&time1);
	QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
	return (double)time1 / freq;
	}

int WriteString(HANDLE cp, char* m)
	{
	DWORD BytesWritten;
	WriteFile(cp, m, strlen(m), &BytesWritten, NULL);
	return BytesWritten;
	}
int WriteBytes(HANDLE cp, char* buf, int size)
	{
	DWORD BytesWritten;
	WriteFile(cp, buf, size, &BytesWritten, NULL);
	return BytesWritten;
	}
int WriteDWORD(HANDLE cp, DWORD size)
	{
	DWORD BytesWritten;
	WriteFile(cp, &size,4, &BytesWritten, NULL);
	return BytesWritten;
	}

int Consume(HANDLE ComPort, char* EatMe, int TimeOut)
	{
	char* teatme = EatMe;
	while (*teatme &&  TimeOut)
		{
		char Byte;
		DWORD dwBytesRead;
		ReadFile(ComPort, &Byte, 1, &dwBytesRead, NULL);
		if (dwBytesRead == 1)
			{
			if (Byte == *teatme)
				teatme++;
			else
				teatme = EatMe;
			}
		else
			TimeOut--;
		}
	if (*teatme)
		return 0;
	return 1;
	}
void SendFile(HANDLE comport, char* filename)
	{
	printf("PC: Sending File %s to PI\n", filename);
	FILE *F = fopen(filename, "rb");
	if (F)
		{
		double st = GetTime();
		fseek(F, 0, SEEK_END);
		DWORD fsize = ftell(F);
		DWORD ofsize = fsize;
		fseek(F, 0, SEEK_SET);
		char* Data = (char*)malloc(fsize);
		fread(Data, 1, fsize, F);
		fclose(F);
		printf("PC: file size is %2.1fKb\n", (float)fsize / (1024));
		DWORD crc = crc32(Data, fsize);
		//printf("file crc is %u\n", crc);
		WriteString(comport, "PICOFILE");
		// We Send 1024 bytes of data 

		WriteDWORD(comport, strlen(filename));
		WriteDWORD(comport, fsize);
		WriteDWORD(comport, fsize);
		WriteDWORD(comport, crc);
		WriteDWORD(comport, (DWORD)'RAW ');
		WriteString(comport, filename);
		char temp[1024];
		memset(temp, 0,1024);
		WriteBytes(comport,temp, 1024 - (20 + strlen(filename)));

		char *tdata = Data;

		printf(ANSIYELLOW"PC: File Transfer: "ANSIWHITE);
		while (fsize)
			{
			float percent = 100 - 100 * ((float)fsize / ofsize);
			printf(ANSIGREEN"%5.1f%%\b\b\b\b\b\b"ANSIGREEN, percent);
			if (fsize >= 1024)
				{
				if (WriteBytes(comport, tdata, 1024) != 1024 || !Consume(comport, "PIOK", 50))
					{
					printf(ANSIRED"\nPC: Error Sending File Chunk\n"ANSIWHITE);
					goto cleanup;
					}
				tdata += 1024;
				fsize -= 1024;
				}
			else
				{
				if (WriteBytes(comport, tdata, fsize) != fsize || !Consume(comport, "PIOK", 500))
					{
					printf(ANSIRED"\nPC: Error Sending File Chunk\n"ANSIWHITE);
					goto cleanup;
					}
				fsize = 0;
				st = GetTime() - st;
				printf(ANSIGREEN"100%%  in %2.1f secs\n\n"ANSIWHITE, st);
				}
			}
	cleanup:
		free(Data);
		}
	else
		printf("PC: error file %s not found\n", filename);
	}
HANDLE EnumComPorts()
	{
	char Res[1024];
	static char ComString[8];
	for (int a = 0; a < 50; a++)
		{
		sprintf(ComString, "COM%d", a);
		if (QueryDosDevice(ComString, Res, 1024)>0)
			{
			HANDLE ComPort = OpenPort(ComString);
			PortConfig(ComPort, UartSpeed);
			WriteString(ComPort, "PICOHELO");
			if (Consume(ComPort, "PIOK", 5))
				{
				printf("PC: Found PI on %s %s\n", ComString, Res);
				return ComPort;
				}
			CloseHandle(ComPort);
			}
		}
	return 0;
	}

int main(int nArgC, char **ppArgV)
	{
	char *pArg0 = *ppArgV++;
	nArgC--;
	system("cls");
	printf(ANSIGREEN"FlashCom V1.00 Serial PI Comms Utility (C) 2020 S.D.Smith\n\n"ANSIWHITE);

	HANDLE ComPort = EnumComPorts();
	if (!ComPort)
		{
		printf(ANSIRED"PC: PI not found :(\n"ANSIWHITE);
		return 0;
		}
	printf("PC: Uart speed is %d * 115200 = %2.1fKb/s\n", 8, (float)UartSpeed / (10 * 1024));

	char* filesend = 0;
	int Reboot = 0;
	for (int a = 0; a < nArgC; a++)
		{
		if (FileExists(ppArgV[a]))
			filesend = ppArgV[a];
		if (strstr(ppArgV[a], "reboot") > 0)
			Reboot = 1;
		}

	if (filesend)
		SendFile(ComPort, filesend);
	char Buf[256] = { 0 };
	while (1)
		{

		DWORD dwBytesRead = 0;
		ReadFile(ComPort, Buf, 255, &dwBytesRead, NULL);
		Buf[dwBytesRead] = 0;
	//	if (dwBytesRead > 0)
			printf("%s", Buf);
		if (Reboot)
			{
			printf("PC: Rebooting PI\n");
			WriteString(ComPort, "PICORBOO");
			Reboot = 0;
			}
		}
	CloseHandle(ComPort);
	return 0;
	}
