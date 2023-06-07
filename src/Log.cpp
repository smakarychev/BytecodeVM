#include "Log.h"

#include <windows.h>

#include "Types.h"

std::ostream& Logger::LogPrefix(std::ostream& s)
{
    s << TimeStamp() << std::format(" {:<7}", "INFO:");
    return s;
}

std::ostream& Logger::WarnPrefix(std::ostream& s)
{
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE); 
    SetConsoleTextAttribute(handle, FOREGROUND_GREEN|FOREGROUND_RED);
    s << TimeStamp() << std::format(" {:<7}", "WARN:");
    return s;
}

std::ostream& Logger::ErrorPrefix(std::ostream& s)
{
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE); 
    SetConsoleTextAttribute(handle, FOREGROUND_RED);
    s << TimeStamp() << std::format(" {:<7}", "ERROR:");
    return s;
}

std::ostream& Logger::FatalPrefix(std::ostream& s)
{
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE); 
    SetConsoleTextAttribute(handle, BACKGROUND_RED);
    s << TimeStamp() << std::format(" {:<7}", "FATAL:");
    return s;
}

std::ostream& Logger::Postfix(std::ostream& s)
{
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE); 
    SetConsoleTextAttribute(handle, FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE);
    s << "\n";
    return s;
}

std::string Logger::TimeStamp()
{
    SYSTEMTIME lt;
    GetLocalTime(&lt);
    return std::format("[{:0>2}:{:0>2}:{:0>2}]", lt.wHour, lt.wMinute, lt.wSecond);
}
