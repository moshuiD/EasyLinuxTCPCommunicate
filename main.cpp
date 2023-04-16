#include <cstdio>
#include <iostream>
#include "TCPServer.hpp"
#include "TCPClient.hpp"

int main()
{
	try
	{
		TCPServer T([](const pair<const string&, int >& msg) {
			printf("Server Get Pack: ClientID:%d Msg:%s \n", msg.second, msg.first.c_str());
			}, 1234, 1024);
		//TCPClient c([](const string& msg) {}, "127.0.0.1", 1234);
		while (true)
		{
			string msg;
			int clientID;
			printf("Put in msg:");
			std::cin >> msg;
			printf("Put in ClientID:");
			std::cin >> clientID;
			T.SendMessage(clientID, msg);
		}
	}
	catch (const exception& e)
	{
		std::cout << e.what() << std::endl;
	}

	return 0;
}