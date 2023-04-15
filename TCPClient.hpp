#pragma once
#include "Log.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <exception>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <functional>
#include <utility>

using std::thread;
using std::string;
using std::exception;
using std::vector;
using std::mutex;
using std::lock_guard;
using std::function;
using std::pair;

class BaseObfManager
{
public:
	BaseObfManager() = default;
	~BaseObfManager() = default;
	[[nodiscard]] inline pair<char*, int> GetObfMsg(char* msg) noexcept
	{
		return { msg,strlen(msg) };
	}
	[[nodiscard]] inline char* GetDecMsg(char* msg) noexcept
	{
		return msg;
	}
};

template<class ObfManager = BaseObfManager>
class BaseMessagePack
{
private:
	ObfManager m_ObfManager;
	/* _____________________
	* | 4 bytes |    ...    |
	* |_________|___________|
	* | PackLen | obfed msg |
	* |_________|___________|
	*/
public:
	BaseMessagePack() = default;
	~BaseMessagePack() = default;
	int GetMessagePack(const string& msg, char* buff)
	{
		pair<char*, int> tempPack = m_ObfManager.GetObfMsg(msg.c_str());
		buff + 4 = tempPack.first;
		*buff = tempPack.second;
		return tempPack.second + 4;
	}

	vector<string> ParsePack(char* buff, int getLen)
	{
		vector<string> ret;
		int len = 0;
		while (true)
		{
			len += *reinterpret_cast<int*>(buff + len);
			if (len + 4 == getLen) {
				ret.emplace_back(m_ObfManager.GetDecMsg(buff + 4));
				break;
			}
			else if (len + 4 < getLen) {
				ret.emplace_back(m_ObfManager.GetDecMsg(buff + 4));
			}
			else {
				Log("Broked Data", "Len:", len, "All Len", getLen);
			}
			len += 4;//len's bytes
		}
		return ret;
	}
};
class TCPClient
{
public:
	TCPClient(function<void(const string&)>handleFunc, const char* ip, int port, int recvBuffLen) :
		m_HandleFunc(handleFunc), m_Port(port), m_RecvBuffLen(recvBuffLen)
	{
		m_ClientID = socket(AF_INET, SOCK_STREAM, 0);
	}
private:
	const int m_Port;
	const int m_RecvBuffLen;
	const function<void(const string&)> m_HandleFunc;
	int m_ClientID;
};
