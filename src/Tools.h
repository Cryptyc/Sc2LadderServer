#pragma once

#include <string>

void StartBotProcess(const std::string& CommandLine);

void SleepFor(int seconds);

void KillSc2Process(unsigned long pid);
