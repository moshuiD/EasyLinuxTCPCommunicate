#pragma once
#define DEBUG 1 //Is or not show Log
#if (DEBUG == 1)
#include "Log.hpp"
#endif
#if (DEBUG != 1)
#define Log(...)
#endif
#include "Vendors.hpp"//As is known to all,must under than #define.
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <exception>
#include <string>
#include <thread>
#include <chrono>
#include <functional>

using std::thread;
using std::string;
using std::exception;
using std::function;

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

template<class MsgPack = BaseMessagePack<BaseObfManager>>
class TCPClient
{
public:
	explicit TCPClient(function<void(const string&)>handleFunc, const char* ip, int port, int recvBuffLen = 1024/*bytes*/, int heartBeatDelay = 60/*s*/) :
		m_HandleFunc(handleFunc), m_Port(port), m_RecvBuffLen(recvBuffLen), m_IP(ip),m_HeartBeatDelay(heartBeatDelay)
	{
		m_RecvBuff = new char[m_RecvBuffLen];
		m_ClientID = socket(AF_INET, SOCK_STREAM, 0);
		if (m_ClientID == -1) {
			throw TCPClientExcept("Socket create failed.");
		}

		m_SockAddr.sin_family = AF_INET;
		m_SockAddr.sin_port = htons(m_Port);
		inet_pton(AF_INET, m_IP, &m_SockAddr.sin_addr);

		if (connect(m_ClientID,reinterpret_cast<sockaddr*>(&m_SockAddr), sizeof(m_SockAddr)) == -1) {
			throw TCPClientExcept("Connection failed.");
		}
		m_RecvThread = thread(&TCPClient::RecvMsgLoop, this);
		m_HeartBeatThread = thread(&TCPClient::HeartBeatLoop, this);
	}
	~TCPClient()
	{
		m_IsRecvThreadRunning = false;
		if (m_RecvThread.joinable()) {
			m_RecvThread.join();
		}
		m_IsHeartBeatThreadRunning = false;
		if (m_HeartBeatThread.joinable()) {
			m_HeartBeatThread.join();
		}
		delete[] m_RecvBuff;
	}
	TCPClient operator= (const TCPClient&) = delete;
	TCPClient operator= (TCPClient&&) = delete;
	TCPClient(const TCPClient&) = delete;
	TCPClient(TCPClient&&) = delete;

	inline bool SendMessage(string& msg)
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
	char* m_RecvBuff;
	bool m_IsRecvThreadRunning = true;
	thread m_RecvThread;
	thread m_HeartBeatThread;
	bool m_IsHeartBeatThreadRunning = true;
	const int m_HeartBeatDelay;
	MsgPack m_MsgPacker;
	void RecvMsgLoop()
	{
		Log("RecvLoop start");
		while (m_IsRecvThreadRunning)
		{
			memset(m_RecvBuff, 0, m_RecvBuffLen);
			int len = recv(m_ClientID, m_RecvBuff, m_RecvBuffLen, MSG_DONTWAIT);
			if (len == 0) {
				Log("Server remove ID:", m_ClientID);
				close(m_ClientID);
			}
			else if (len != -1) {
				Log("[+] GetMsg");
				for (const auto& msg : m_MsgPacker.ParsePack(m_RecvBuff, len))
				{
					m_HandleFunc(string(msg));
				}
			}
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
		Log("RecvLoop stop");
	}
	void HeartBeatLoop()
	{
		Log("HeartBeatLoop start");
		while (m_IsHeartBeatThreadRunning)
		{
			char buff[1]{};
			m_MsgPacker.GetHeartBeatPack(buff);
			send(m_ClientID, buff, 1, 0);
			std::this_thread::sleep_for(std::chrono::seconds(m_HeartBeatDelay));
		}
		Log("HeartBeatLoop stop");
	}
};
