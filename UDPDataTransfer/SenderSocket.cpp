#include "SenderSocket.h"



SenderSocket::SenderSocket()
{
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock == INVALID_SOCKET) {
		cout << "Socket initialization failed with error: " << WSAGetLastError() << endl;
		closesocket(sock);
		WSACleanup();
		exit(-1);
	}
}


SenderSocket::~SenderSocket()
{
}
