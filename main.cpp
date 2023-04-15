#include <cstdio>
#include "iostream"
#include "TCPServer.hpp"
int main()
{
	{
		try {
			TCPServer T([](const pair<const string&,int >& msg) {
				printf("%s \n", msg.first.c_str());
				}, 1234, 1024);
			int a = 1;
			scanf("%d", &a);
		}
		catch (exception& e) {
			std::cout << e.what()<< std::endl;
		}
	}
	return 0;
}