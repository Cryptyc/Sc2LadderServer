#pragma once

#include <string>
#include "Types.h"

void StartBotProcess(const BotConfig &Agent, const std::string& CommandLine, void **ProcessId);

void SleepFor(int seconds);

void KillSc2Process(unsigned long pid);

bool MoveReplayFile(const char* lpExistingFileName, const char* lpNewFileName);

void KillBotProcess(void *ProcessStruct);
