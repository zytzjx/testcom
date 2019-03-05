// testcom.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <Windows.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <vector>

#include "syscfg.h"

int _tmain(int argc, TCHAR *argv[])
{
	/*
	std::ifstream file("E:\\Works\\Testcode\\testcom\\Release\\syscfgdata.dat", std::ios::binary | std::ios::ate);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	if (file.read(buffer.data(), size))
	{
		syscfg_init((uint8_t *)buffer.data(), size);
		int argc = 0;
		struct cmd_arg args;
		do_syscfg(argc, &args);
		
	}
//*/
//*
	TCHAR comName[MAX_PATH] = { 0 };
	_stprintf_s(comName, _T("\\\\.\\%s"), argv[1]);

	HANDLE h = CreateFile(comName,//_T("\\\\.\\COM4"),
		GENERIC_READ | GENERIC_WRITE,           // access (read-write) mode
		0,                  // share mode
		NULL,               // address of security descriptor
		OPEN_EXISTING,      // how to create
		0,                  // file attributes
		NULL                // handle of file with attributes to copy
	);
	if (h == INVALID_HANDLE_VALUE)
	{
		//  Handle the error.
		printf("CreateFile failed with error %d.\n", GetLastError());
		return (1);
	}
	DWORD  ModemStat = 0;
	while (true)
	{
		BOOL b = GetCommModemStatus(
			h,
			&ModemStat
		);
		if (!b) {
			printf(" failed with error %d.\n", GetLastError());
		}
		printf("STATUS %x.\n", ModemStat);

		Sleep(1000);

		/*DCB dcb;
		dcb.DCBlength = sizeof(DCB);

		b = GetCommState(h, &dcb);
		if (!b) {
			printf(" failed with error %d.\n", GetLastError());
		}*/
	}

	CloseHandle(h);
//*/
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
