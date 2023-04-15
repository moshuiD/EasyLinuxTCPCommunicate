#pragma once
#include "Log.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

class TCPClientExcept :public exception
{
public:
	explicit TCPClientExcept(const string& info) :m_Info(info) {}
	~TCPClientExcept() = default;
	const char* what() const noexcept
	{
		return m_Info.c_str();
	};
private:
	const string m_Info;
};

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
	/* ______________________________
	* | 1 byte | 4 bytes |    ...    |
	* |________|_________|___________|
	* |  Flag  | PackLen | obfed msg |
	* |________|_________|___________|
	*/
	enum Flag : char
	{
		NormalMsg = 0,
		HeartBeat = 1,
	};
public:
	BaseMessagePack() = default;
	~BaseMessagePack() = default;
	/// <summary>
	/// 获取一个封装的包
	/// </summary>
	/// <param name="msg">要发送的文本</param>
	/// <param name="buff">封装好的包</param>
	/// <returns>完整的包大小</returns>
	int GetMessagePack(const string& msg, char* buff)
	{
		pair<char*, int> tempPack = m_ObfManager.GetObfMsg(msg.c_str());
		buff = Flag::NormalMsg;
		buff + 5 = tempPack.first;
		*buff + 1 = tempPack.second;
		return tempPack.second + 5;
	}

	vector<string> ParsePack(char* buff, int getLen)
	{
		vector<string> ret;
		char* pPackFlag = buff;
		char* pPackLen = buff + 1;
		char* szPack = buff + sizeof(char) + sizeof(int);
		char* endPack = buff + getLen;
		for (char* it = buff; it < endPack; )
		{
			int len = *reinterpret_cast<int*>(pPackLen);
			if (*pPackFlag == Flag::NormalMsg) {
				ret.emplace_back(m_ObfManager.GetDecMsg(szPack));
			}
			else if (*pPackFlag == Flag::HeartBeat) {
				Log("HeartBeat Pack");
			}
			else {
				Log("Broked Data", "Len:", len, "All Len", getLen);
				break;
			}
			//flag's byte + len's bytes
			pPackFlag += (sizeof(char) + sizeof(int) + len);
			it += (sizeof(char) + sizeof(int) + len);
			pPackLen += (sizeof(char) + sizeof(int) + len);
			szPack += (sizeof(char) + sizeof(int) + len);
		}
		return ret;
	}
};

template<class MsgPack = BaseMessagePack<BaseObfManager>>
class TCPClient
{
public:
	explicit TCPClient(function<void(const string&)>handleFunc, const char* ip, int port, int recvBuffLen = 1024/*bytes*/) :
		m_HandleFunc(handleFunc), m_Port(port), m_RecvBuffLen(recvBuffLen), m_IP(ip)
	{
		m_RecvBuff = new char[m_RecvBuffLen];
		m_ClientID = socket(AF_INET, SOCK_STREAM, 0);
		if (m_ClientID == -1) {
			throw TCPClientExcept("Socket create failed.");
		}

		m_SockAddr.sin_family = AF_INET;
		m_SockAddr.sin_port = htons(m_Port);
		inet_pton(AF_INET, m_IP, &m_SockAddr.sin_addr);

		if (connect(m_ClientID, &m_SockAddr, sizeof(m_SockAddr)) == -1) {
			throw TCPClientExcept("Connection failed.");
		}
		m_RecvThread = thread(&TCPClient::RecvMsgLoop, this);
	}
	~TCPClient()
	{
		m_IsRecvThreadRunning = false;
		if (m_RecvThread.joinable()) {
			m_RecvThread.join();
		}

		delete[] m_RecvBuff;
	}
	bool SendMessage(string& msg) const
	{
		char buff[m_RecvBuffLen]{};
		int len = m_MsgPacker.GetMessagePack(msg, buff);
		return send(m_ClientID, buff, len, 0) == -1;
	}
private:
	const function<void(const string&)> m_HandleFunc;
	const int m_Port;
	const int m_RecvBuffLen;
	const char* m_IP;
	int m_ClientID;
	sockaddr_in m_SockAddr{};
	bool m_IsRecvThreadRunning = true;
	char* m_RecvBuff;
	thread m_RecvThread;

	void RecvMsgLoop()
	{
		Log("RecvLoop start");
		while (m_IsRecvThreadRunning)
		{
			int len = recv(m_ClientID, m_RecvBuff, m_RecvBuffLen, MSG_DONTWAIT);
			if (len == 0) {
				Log("Server remove ID:", m_ClientID);
				close(m_ClientID);
			}
			else if (len != -1) {
				Log("GetMsg:", m_RecvBuff);
				for (const auto& msg : m_MsgPacker.ParsePack(m_RecvBuff, len))
				{
					m_HandleFunc(string(msg));
				}
			}
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
};
