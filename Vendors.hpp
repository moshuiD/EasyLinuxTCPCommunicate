#pragma once
#include <utility>
#include <string>
#include <cstring>
#include <vector>

using std::pair;
using std::string;
using std::vector;

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
		pair<char*, int> tempPack = m_ObfManager.GetObfMsg(const_cast<char*>(msg.c_str()));
		buff[0] = Flag::NormalMsg;
		memcpy(buff + 5, tempPack.first,tempPack.second);
		*(buff + 1) = tempPack.second;
		return tempPack.second + 5;
	}

	int GetHeartBeatPack(char* buff)
	{
		buff[0] = Flag::HeartBeat;
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
