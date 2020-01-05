
#include <windows.h>
//#include <timeapi.h>
#include <stdio.h>
#include <string.h>
const int UartSpeed = 115200 * 8;
HANDLE     hStdout;
#define ConsumeTimeout 5
#define BLUE  1
#define GREEN 2
#define CYAN 3
#define RED 4
#define PURPLE 5
#define YELLOW 6
#define WHITE 7
#define ORIGIN "PC: "

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

void TextColor(int col)
	{
	SetConsoleTextAttribute(hStdout, col | FOREGROUND_INTENSITY);
	}

HANDLE OpenPort(char* Name)
	{

	//	system("mode com1: baud=9600 parity=n data=8 stop=1 to=off xon=off");
	HANDLE hSerial;
	hSerial = CreateFile(Name, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hSerial == INVALID_HANDLE_VALUE)
		{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
			{
			//serial port does not exist. Inform user.
			}
		TextColor(RED);
		fprintf(stderr, "Error opening: %s\n", Name);
		TextColor(WHITE);
		return 0;

		//some other error occurred. Inform user.
		}
	return hSerial;
	}

int FileExists(char* filename)
	{
	FILE *F;
	F = fopen(filename, "rb");
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
	if (!GetCommState(port, &dcbSerialParams))
		{
		TextColor(RED);
		//fprintf(stderr, "Error Getting Com State\n");
		TextColor(WHITE);
		//error getting state
		}
	dcbSerialParams.BaudRate = (DWORD)Speed;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;
	if (!SetCommState(port, &dcbSerialParams))
		{
		TextColor(RED);
		//fprintf(stderr, "Error Setting Com State\n");
		TextColor(WHITE);
		//error setting serial port state
		}

	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 50;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;

	timeouts.WriteTotalTimeoutConstant = 50;
	timeouts.WriteTotalTimeoutMultiplier = 10;
	if (!SetCommTimeouts(port, &timeouts))
		{
		TextColor(RED);
		//fprintf(stderr, "Error Setting Timeouts\n");
		TextColor(WHITE);
		}
	}
void ClosePort(HANDLE port)
	{

	}

double GetTime()
	{
	__int64 time1 = 0, time2 = 0, freq = 0;
	QueryPerformanceCounter((LARGE_INTEGER *)&time1);
	QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
	return (double)time1 / freq;
	}
void uSleep(int waitTime)
	{
	__int64 time1 = 0, time2 = 0, freq = 0;
	QueryPerformanceCounter((LARGE_INTEGER *)&time1);
	QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
	do
		{
		QueryPerformanceCounter((LARGE_INTEGER *)&time2);
		} while ((time2 - time1) < waitTime);
	}
void Write(HANDLE cp, char* m)
	{
	DWORD BytesWritten;
	WriteFile(cp, m, strlen(m), &BytesWritten, NULL);
	}
void WriteBytes(HANDLE cp, char* buf, int size)
	{
	DWORD BytesWritten;
	WriteFile(cp, buf, size, &BytesWritten, NULL);
	}


int Consume(HANDLE ComPort, char* EatMe, int TimeOut)
	{
	char* teatme = EatMe;
	int count = 0;
	while (*teatme && count < TimeOut)
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
			count++;
		}
	if (*teatme)
		return 0;
	return 1;
	}
int SendFile(HANDLE comport, char* filename)
	{
	int res = 1;
	printf("PC: Sending File %s to PI\n", filename);
	FILE *F;
	F = fopen(filename, "rb");
	if (F)
		{
		double st = GetTime();
		fseek(F, 0, SEEK_END);
		int fsize = ftell(F);
		fseek(F, 0, SEEK_SET);
		char* Data = (char*)malloc(fsize);
		fread(Data, 1, fsize, F);
		printf("PC: file size is %2.1fKb\n", (float)fsize / (1024));
		unsigned int crc = crc32(Data, fsize);
		//printf("file crc is %u\n", crc);
		char sstring[1024];
		sprintf(sstring, "PICMD:FILE%s,%d,%d,%u,RAW,", filename, fsize, fsize, crc);
		Write(comport, sstring);
		char *tdata = Data;
		int chunk = 0;
		int ofsize = fsize;
		printf("PC: File Transfer: ");
		while (fsize)
			{
			float percent = 100 - 100 * ((float)fsize / ofsize);
			TextColor(GREEN);
			printf("%5.1f%%\b\b\b\b\b\b", percent);
			TextColor(WHITE);
			int BytesWritten = 0;
			if (fsize > 1024)
				{
				WriteFile(comport, tdata, 1024, &BytesWritten, NULL);
				tdata += 1024;
				fsize -= 1024;
				if (BytesWritten != 1024 || !Consume(comport, "FOK", 50))
					{
					if (BytesWritten != 1024)
						printf("PC: Error Sending File Chunk %d Bytes Written %d\n", chunk, BytesWritten);
					else
						printf("PC: Error Sending File Chunk %d no FOK received\n", chunk, BytesWritten);
					res = 0;
					goto cleanup;
					}
				}
			else
				{
				WriteFile(comport, tdata, fsize, &BytesWritten, NULL);
				if (BytesWritten != fsize || !Consume(comport, "FOK", 500))
					{
					if (BytesWritten != fsize)
						printf("PC: Error Sending File Chunk %d Bytes Written %d\n", chunk, BytesWritten);
					else
						printf("PC: Error Sending File Chunk %d no FOK received\n", chunk, BytesWritten);

					res = 0;
					goto cleanup;
					}
				fsize = 0;
				TextColor(GREEN);
				st = GetTime() - st;
				printf("100%%  in %2.1f secs\n\n", st);
				TextColor(WHITE);

				}
			chunk++;
			}
		//	printf("\n");
	cleanup:
		fclose(F);
		free(Data);
		}
	else
		printf("PC: error file %s not found\n", filename);
	return res;
	}
HANDLE EnumComPorts()
	{
	//	printf("Enum Com Ports:\n\n");
	char Res[1024];
	static char ComString[1024];
	for (int a = 0; a < 50; a++)
		{
		Res[0] = 0;

		sprintf(ComString, "COM%d", a);
		QueryDosDevice(ComString, Res, 1024);
		if (Res[0] != 0)
			{
			//printf("%s %s\n", ComString, Res);
			HANDLE ComPort = OpenPort(ComString);
			PortConfig(ComPort, UartSpeed);
			Write(ComPort, "PICMD:HELO");
			if (Consume(ComPort, "PI: We are Connected!", 5))
				{
				//	TextColor(YELLOW);
				printf("PC: Found PI on %s %s\n", ComString, Res);
				//	TextColor(WHITE);
				return ComPort;
				}
			ClosePort(ComPort);
			}
		}
	printf("\n");
	return 0;
	}

int main(int nArgC, char **ppArgV)
	{
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	system("cls");
	TextColor(GREEN);
	printf("FlashCom V1.00 Serial PI Comms Utility (C) 2020 S.D.Smith\n\n");
	TextColor(WHITE);
	//EnumComPorts();

	HANDLE ComPort = EnumComPorts();
	if (!ComPort)
		{
		printf("PC: PI not found :(\n");
		return 0;
		}
	//PortConfig(ComPort, UartSpeed);

//	TextColor(BLUE);
	fprintf(stderr, "PC: Uart speed is %d * 115200 = %2.1fKb/s\n", 8, (float)UartSpeed / (10 * 1024));
	//	TextColor(WHITE);

	char *pArg0 = *ppArgV++;
	nArgC--;
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
		{
		if (SendFile(ComPort, filesend))
			Sleep(400); // sleep a little so PI has a chance to come out of handler and write file
		}
	if (Reboot)
		{
		printf("PC: Rebooting PI\n");
		Write(ComPort, "PICMD:RBOO");
		}

	while (1)
		{
		char szBuff[16] = { 0 };
		DWORD dwBytesRead = 0;
		ReadFile(ComPort, szBuff, 1, &dwBytesRead, NULL);
		//TextColor(YELLOW);
		if (dwBytesRead == 1)
			printf("%c", szBuff[0]);
		//TextColor(WHITE);
		uSleep(1);
		}
	ClosePort(ComPort);
	return 0;
	}
