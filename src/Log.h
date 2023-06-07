#pragma once

#include <iostream>
#include <format>

class Logger
{
public:
    template <typename ...Args>
    using Fmt = const std::_Fmt_string<Args...>;

    template <typename ...Args>
    static void Log(Fmt<Args...> format, Args&& ...args);

    template <typename ...Args>
    static void Warn(Fmt<Args...> format, Args&& ...args);

    template <typename ...Args>
    static void Error(Fmt<Args...> format, Args&& ...args);

    template <typename ...Args>
    static void Fatal(Fmt<Args...> format, Args&& ...args);
private:
    static std::ostream& LogPrefix(std::ostream &s);
    static std::ostream& WarnPrefix(std::ostream &s);
    static std::ostream& ErrorPrefix(std::ostream &s);
    static std::ostream& FatalPrefix(std::ostream &s);
    static std::ostream& Postfix(std::ostream &s);
    static std::string TimeStamp();
};

template <typename ... Args>
void Logger::Log(Fmt<Args...> format, Args&& ...args)
{
    std::cout << LogPrefix << std::format(format, std::forward<Args>(args)...) << Postfix;
}

template <typename ... Args>
void Logger::Warn(Fmt<Args...> format, Args&& ...args)
{
    std::cout << WarnPrefix << std::format(format, std::forward<Args>(args)...) << Postfix;
}

template <typename ... Args>
void Logger::Error(Fmt<Args...> format, Args&& ...args)
{
    std::cout << ErrorPrefix << std::format(format, std::forward<Args>(args)...) << Postfix;
}

template <typename ... Args>
void Logger::Fatal(Fmt<Args...> format, Args&& ...args)
{
    std::cout << FatalPrefix << std::format(format, std::forward<Args>(args)...) << Postfix;
}

#define LOG_INFO(...)  ::Logger::Log(__VA_ARGS__)
#define LOG_WARN(...)  ::Logger::Warn(__VA_ARGS__)
#define LOG_ERROR(...) ::Logger::Error(__VA_ARGS__)
#define LOG_FATAL(...) ::Logger::Fatal(__VA_ARGS__)

