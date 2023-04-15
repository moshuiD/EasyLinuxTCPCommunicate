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

class TCPServerExcept :public exception
{
public:
	explicit TCPServerExcept(const string& info) :m_Info(info) {}
	~TCPServerExcept() = default;
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

	int GetHeartBeatPack(char* buff)
	{
		buff = Flag::HeartBeat;
		return 1;
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
			int FullPackLen = (sizeof(char) + sizeof(int) + len);
			//flag's byte + len's bytes.Use pointer to do it.
			pPackFlag += FullPackLen;
			it += FullPackLen;
			pPackLen += FullPackLen;
			szPack += FullPackLen;
		}
		return ret;
	}
};

template<class MsgPack = BaseMessagePack<BaseObfManager>>
class TCPServer
{
public:
	explicit TCPServer(function<void(const pair<const string&, int>&)>handle, int port = 1234, int recvLen = 1024/*bytes*/) :
		m_Port(port), m_RecvBuffLen(recvLen), m_SocketAddr(), m_HandleFunc(handle)
	{
		m_RecvBuffer = new char[recvLen];
		m_ServerID = socket(AF_INET, SOCK_STREAM, 0);
		if (m_ServerID == -1) {
			throw TCPServerExcept("Socket create failed.");
		}
		m_SocketAddr.sin_family = AF_INET;
		m_SocketAddr.sin_addr.s_addr = INADDR_ANY;
		m_SocketAddr.sin_port = htons(m_Port);
		if (bind(m_ServerID, reinterpret_cast<sockaddr*>(&m_SocketAddr), sizeof(m_SocketAddr)) == -1) {
			throw TCPServerExcept("Bind failed.");
		}

		if (listen(m_ServerID, 3) == -1) {
			throw TCPServerExcept("Listen failed.");
		}

		//设置非阻塞
		int flags = fcntl(m_ServerID, F_GETFL, 0);
		if (flags == -1) {
			throw TCPServerExcept("Set socket non-blocking failed.");
		}
		if (fcntl(m_ServerID, F_SETFL, flags | O_NONBLOCK) == -1) {
			throw TCPServerExcept("Set socket non-blocking failed.");
		}

		m_RecvThread = thread(&TCPServer::RecvLoop, this);
		m_AcceptThread = thread(&TCPServer::AcceptLoop, this);
	};

	~TCPServer()
	{
		m_IsAcceptThreadRunning = false;
		if (m_AcceptThread.joinable()) {
			m_AcceptThread.join();
		}
		m_IsRecvThreadRunning = false;
		if (m_RecvThread.joinable()) {
			m_RecvThread.join();
		}

		close(m_ServerID);
		for (const auto& clientID : m_ClientIDs)
		{
			close(clientID);
		}
		delete[] m_RecvBuffer;
	}

	TCPServer operator= (const TCPServer&) = delete;
	TCPServer operator= (TCPServer&&) = delete;
	TCPServer(const TCPServer&) = delete;
	TCPServer(TCPServer&&) = delete;
	inline vector<int> GetClientIDsView() const noexcept
	{
		lock_guard lock(m_ClientIDsLock);
		return m_ClientIDs;
	}
	inline bool SendMessage(int clientID, string& msg) const
	{
		char buff[m_RecvBuffLen]{};
		int len = m_MsgPacker.GetMessagePack(msg, buff);
		return send(clientID, buff, len, 0) == -1;
	}
private:
	const int m_Port;
	const int m_RecvBuffLen;
	sockaddr_in m_SocketAddr;
	int m_ServerID;
	bool m_IsAcceptThreadRunning = true;
	thread m_AcceptThread;
	bool m_IsRecvThreadRunning = true;
	thread m_RecvThread;
	vector<int> m_ClientIDs;
	mutable mutex m_ClientIDsLock;
	char* m_RecvBuffer;
	const function<void(const pair<const string&, int>&)> m_HandleFunc;
	MsgPack m_MsgPacker;

	void AcceptLoop()
	{
		Log("AcceptLoop start");
		while (m_IsAcceptThreadRunning)
		{
			int addrlen = sizeof(m_SocketAddr);
			int clientID = accept(m_ServerID, reinterpret_cast<sockaddr*>(&m_SocketAddr), reinterpret_cast<socklen_t*>(&addrlen));
			if (clientID != -1) {
				Log("Client connected ID:", clientID);
				lock_guard lock(m_ClientIDsLock);
				m_ClientIDs.push_back(clientID);
			}
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
		Log("AcceptLoop stop");
	}
	void RecvLoop()
	{
		Log("RecvLoop start");
		while (m_IsRecvThreadRunning)
		{
			lock_guard lock(m_ClientIDsLock);
			vector<int> needDeleteItem;
			for (const auto& clientID : m_ClientIDs)
			{
				if (CheckValidClient(clientID)) {
					int len = recv(clientID, m_RecvBuffer, m_RecvBuffLen, MSG_DONTWAIT);
					if (len == 0) {
						Log("Client remove ID:", clientID);
						close(clientID);
						needDeleteItem.push_back(clientID);
					}
					else if (len != -1) {
						Log("GetMsg:", m_RecvBuffer);
						for (const auto& msg : m_MsgPacker.ParsePack(m_RecvBuffer, len))
						{
							m_HandleFunc({ string(msg),clientID });
						}
					}
				}
				else {
					needDeleteItem.push_back(clientID);
				}
			}
			m_ClientIDs.erase(std::remove_if(m_ClientIDs.begin(), m_ClientIDs.end(),
				[&](const auto& item) {
					return std::find(needDeleteItem.begin(), needDeleteItem.end(), item) != needDeleteItem.end();
				}),
				m_ClientIDs.end());
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
		Log("RecvLoop stop");
	}

	inline bool CheckValidClient(int clientID)
	{
		sockaddr_in peerAddr{};
		socklen_t peerLen = sizeof(peerAddr);
		if (getpeername(clientID, (sockaddr*)&peerAddr, &peerLen) == -1) {
			return false;
		}
		else {
			return true;
		}
	}
};