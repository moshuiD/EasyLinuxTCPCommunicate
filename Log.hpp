#pragma once 
#include <ctime>
#include <cstring>
#include <string>
#include <iostream>
#define Log(msg, ...) Log::PrintLog(Log::GetLocalTime(),"F:",__FILE__,"L:", __LINE__, "Func:",__func__,"M:", msg, ##__VA_ARGS__);
namespace Log
{
	using std::string;
	inline string GetLocalTime()
	{
		tm* tm_now;
		time_t now = time(nullptr);
		tm_now = localtime(&now);
		char LogTime[20];
		strftime(LogTime, sizeof(LogTime), "%H:%M:%S", tm_now);
		return string(LogTime);
	}
	inline void PrintLog()
	{
		std::cout << std::endl;
	}
	template<class v1, class ...Args>
	inline void PrintLog(v1 msg, Args... args)
	{
		std::cout << msg << ' ';
		PrintLog(args...);
	}
}
