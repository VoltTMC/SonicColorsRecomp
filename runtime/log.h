#pragma once
#include <cstdio>
#include <format>
#include <string>

void Log_Open(const char* path);
// Path last passed to Log_Open (empty if logging not opened yet).
const std::string& Log_Path();
void Log_Write(int level, const std::string& s);

#define LOG_DEBUG(...) Log_Write(0, std::format(__VA_ARGS__))
#define LOG_INFO(...) Log_Write(1, std::format(__VA_ARGS__))
#define LOG_WARN(...) Log_Write(2, std::format(__VA_ARGS__))
#define LOG_ERROR(...) Log_Write(3, std::format(__VA_ARGS__))

[[noreturn]] void Fatal(const std::string& s);
#define FATAL(...) Fatal(std::format(__VA_ARGS__))
