#ifdef _WIN32

#include "Tools.h"
#include "LadderManager.h"
#include <Windows.h>


void StartBotProcess(const BotConfig &Agent, const std::string &CommandLine, unsigned long *ProcessId)
{
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Executes the given command using CreateProcess() and WaitForSingleObject().
	// Returns FALSE if the command could not be executed or if the exit code could not be determined.

	SECURITY_ATTRIBUTES securityAttributes;
	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.lpSecurityDescriptor = NULL;
	securityAttributes.bInheritHandle = TRUE;
	std::string logFile = Agent.RootPath + "/stderr.log";
	HANDLE stderrfile = CreateFile(logFile.c_str(),
		FILE_APPEND_DATA,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		&securityAttributes,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	HANDLE stdoutfile = NULL;
	if (Agent.Debug)
	{
		stdoutfile = CreateFile("stdout.log",
			FILE_APPEND_DATA,
			FILE_SHARE_WRITE | FILE_SHARE_READ,
			&securityAttributes,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
	}

	PROCESS_INFORMATION processInformation;
	STARTUPINFO startupInfo;
	DWORD flags = NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE; //CREATE_NO_WINDOW <-- also possible, but we don't see easily if bot is still running.

	ZeroMemory(&processInformation, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.dwFlags |= STARTF_USESTDHANDLES;
	startupInfo.hStdInput = INVALID_HANDLE_VALUE;
	startupInfo.hStdError = stderrfile;
	startupInfo.hStdOutput = stdoutfile;

	DWORD exitCode;
	LPSTR cmdLine = const_cast<char *>(CommandLine.c_str());
	// Create the process

	BOOL result = CreateProcess(NULL, cmdLine,
		NULL, NULL, TRUE,
		flags,
		NULL, Agent.RootPath.c_str(), &startupInfo, &processInformation);


	if (!result)
	{
		// Get the error from the system
		LPSTR lpMsgBuf;
		DWORD dw = GetLastError();
		size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
		std::string message(lpMsgBuf, size);
		PrintThread{} << "Starting bot: " << Agent.BotName << " with command:" << std::endl << CommandLine << std::endl << "...failed" << std::endl << "Error " << dw << " : " << message << std::endl;
		// Free resources created by the system
		LocalFree(lpMsgBuf);
		// We failed.
		exitCode = -1;
	}
	else
	{
		PrintThread{} << "Starting bot: " << Agent.BotName << " with command:" << std::endl << CommandLine << std::endl << "...success!" << std::endl;
		// Successfully created the process.  Wait for it to finish.
		*ProcessId = processInformation.dwProcessId;
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
	CloseHandle(stderrfile);
	if (Agent.Debug)
	{
		CloseHandle(stdoutfile);
	}
}

void StartExternalProcess(const std::string CommandLine)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	// set the size of the structures
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	LPSTR cmdLine = const_cast<char *>(CommandLine.c_str());
	// Create the process

	BOOL result = CreateProcess(NULL, cmdLine,
		NULL, NULL, TRUE,
		0,
		NULL, NULL, &si, &pi);

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
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

bool MoveReplayFile(const char* lpExistingFileName, const char* lpNewFileName) {
	return MoveFile(lpExistingFileName, lpNewFileName);
}

#endif