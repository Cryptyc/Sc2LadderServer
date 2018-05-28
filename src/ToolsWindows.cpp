#include <Windows.h>

#include "Tools.h"

void StartBotProcess(const std::string& CommandLine)
{
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Executes the given command using CreateProcess() and WaitForSingleObject().
	// Returns FALSE if the command could not be executed or if the exit code could not be determined.
	PROCESS_INFORMATION processInformation = { 0 };
	STARTUPINFO startupInfo = { 0 };
	DWORD exitCode;
	startupInfo.cb = sizeof(startupInfo);
	LPSTR cmdLine = const_cast<char *>(CommandLine.c_str());

	// Create the process

	BOOL result = CreateProcess(NULL, cmdLine,
		NULL, NULL, FALSE,
		NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE,
		NULL, NULL, &startupInfo, &processInformation);


	if (!result)
	{
		// CreateProcess() failed
		// Get the error from the system
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);


		// Free resources created by the system
		LocalFree(lpMsgBuf);

		// We failed.
		exitCode = -1;
	}
	else
	{
		// Successfully created the process.  Wait for it to finish.
		WaitForSingleObject(processInformation.hProcess, INFINITE);

		// Get the exit code.
		result = GetExitCodeProcess(processInformation.hProcess, &exitCode);

		// Close the handles.
		CloseHandle(processInformation.hProcess);
		CloseHandle(processInformation.hThread);

		if (!result)
		{
			// Could not get exit code.
			exitCode = -1;
		}


		// We succeeded.
	}
}

void SleepFor(int seconds)
{
	Sleep(seconds * 1000);
}

void KillSc2Process(unsigned long pid)
{
	DWORD dwDesiredAccess = PROCESS_TERMINATE;
	BOOL  bInheritHandle = FALSE;
	HANDLE hProcess = OpenProcess(dwDesiredAccess, bInheritHandle, pid);
	if (hProcess == NULL)
		return;

	TerminateProcess(hProcess, 0);
	CloseHandle(hProcess);
}
