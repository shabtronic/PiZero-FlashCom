/*
 * converttool.c
 *
 * Converts a web content file to a C string or array (with -b).
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
HANDLE     hStdout;

HANDLE OpenPort(char* Name)
	{

	//	system("mode com1: baud=9600 parity=n data=8 stop=1 to=off xon=off");
	HANDLE hSerial;
	hSerial = CreateFile(Name,	GENERIC_READ | GENERIC_WRITE,	0,	0,	OPEN_EXISTING,	FILE_ATTRIBUTE_NORMAL,	0);
	if (hSerial == INVALID_HANDLE_VALUE)
		{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
			{
			//serial port does not exist. Inform user.
			}
		fprintf(stderr, "\nError Opening Serial Port\n");
		SetConsoleTextAttribute(hStdout, 7 | FOREGROUND_INTENSITY);


		//some other error occurred. Inform user.
		}
	}
void PortConfig(HANDLE port, int Speed)
	{
	DCB dcbSerialParams = { 0 };
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
	if (!GetCommState(port, &dcbSerialParams))
		{
		SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_INTENSITY);

		fprintf(stderr, "\nError Getting Com State\n");
		SetConsoleTextAttribute(hStdout, 7 | FOREGROUND_INTENSITY);

		//error getting state
		}
	dcbSerialParams.BaudRate = (DWORD)Speed;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;
	if (!SetCommState(port, &dcbSerialParams))
		{
		SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_INTENSITY);

		fprintf(stderr, "\nError Setting Com State\n");
		SetConsoleTextAttribute(hStdout, 7 | FOREGROUND_INTENSITY);

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
		SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_INTENSITY);

		fprintf(stderr, "\nError Setting Timeouts\n");
		SetConsoleTextAttribute(hStdout, 7 | FOREGROUND_INTENSITY);

		//error occureed. Inform user
		}
	}
void ClosePort(HANDLE port)
	{

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
void Write(HANDLE cp,char* m)
	{
	DWORD BytesWritten;
	WriteFile(cp, m, strlen(m), &BytesWritten, NULL);

	}
int main(int nArgC, char **ppArgV)
	{
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE ComPort = OpenPort("COM7");
	//HANDLE ComPort = OpenPort(ppArgV[1]);
	PortConfig(ComPort, 115200);
	char *pArg0 = *ppArgV++;
	nArgC--;

	system("cls");
	SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	printf("FlashCom V1.00 Serial Pi Comms Transfer Utility (C) 2020 S.D.Smith\n\n", pArg0);
	SetConsoleTextAttribute(hStdout, 7 | FOREGROUND_INTENSITY);

	if (nArgC != 2)
		{
		SetConsoleTextAttribute(hStdout, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		fprintf(stderr, "Usage: FlashCom PortName[COM7] Speed[115200] [FileName] [Compress]\n\n", pArg0);
		SetConsoleTextAttribute(hStdout, 7 | FOREGROUND_INTENSITY);
		}
	SetConsoleTextAttribute(hStdout, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	printf("Port Com7 Open and waiting:\n\n", pArg0);
	SetConsoleTextAttribute(hStdout, 7 | FOREGROUND_INTENSITY);
	// Send Hello
	Write(ComPort, "PICMD:HELO");

	//Sleep(1000);
	int cc = 0;

	while (1)
		{
		char szBuff[16] = { 0 };
		DWORD dwBytesRead = 0;
		ReadFile(ComPort, szBuff, 1, &dwBytesRead, NULL);
		if (dwBytesRead == 1)
			{
			printf("%c", szBuff[0]);
			}
		uSleep(1);
		cc++;
		if (cc == 500)
			{
			printf("Sending File:\n");
			Write(ComPort, "PICMD:FILE");
			//Write(ComPort, "Kernel.img,12344,123454589,");
			}
		if (cc == 1000)
			{
			printf("Rebooting PI:\n");
			Write(ComPort, "PICMD:RBOO");
			}

		}
	ClosePort(ComPort);
	return 0;
	}
