#include "SenderSocket.h"



SenderSocket::SenderSocket()
{
	sock = socket(AF_INET, SOCK_DGRAM, 0); //TODO: check and may be remove IPPROTO_UDP 

	if (sock == INVALID_SOCKET) {
		cout << "Socket initialization failed with error: " << WSAGetLastError() << endl;
		closesocket(sock);
		WSACleanup();
		exit(-1);
	}
}

int SenderSocket::OpenTrial(char *host, int port_no, int senderWindow, LinkProperties *lp) {
	printf("OpenTrial called, with host %s\n", host);

	struct sockaddr_in local;
	memset(&local, 0, sizeof(local));

	local.sin_family = AF_INET;
	local.sin_port = htons(0);
	local.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
		cout << "Binder failed! " << WSAGetLastError() << endl;
		//TODO: do something here
		return -1;
	}
	cout << "Bind success" << endl;

	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));

	struct hostent *remote;
	char *address;
	in_addr addr;

	DWORD IP = inet_addr(host);

	if (IP == INADDR_NONE)
	{
		// if not a valid IP, then do a DNS lookup
		if ((remote = gethostbyname(host)) == NULL)
		{
			cout << "failed with " << WSAGetLastError() << endl;
			return -1;
		}
		else // take the first IP address and copy into sin_addr
		{
			memcpy((char *)&(server.sin_addr), remote->h_addr, remote->h_length);
			addr.s_addr = *(u_long *)remote->h_addr;
			address = inet_ntoa(addr);
		}
	}
	else
	{
		// if a valid IP, directly drop its binary version into sin_addr
		server.sin_addr.S_un.S_addr = IP;
		address = host;
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(port_no);

	int attemptCount = 0;

	char *buf_SendTo = new char[sizeof(SenderSynHeader)];
	SenderSynHeader senderSyncHeader;
	senderSyncHeader.sdh.flags.SYN = 1;
	senderSyncHeader.sdh.seq = 0;
	senderSyncHeader.lp = *lp;

	memcpy(buf_SendTo, &senderSyncHeader, sizeof(SenderSynHeader));
	
	int send_res = sendto(sock, (char *)buf_SendTo, sizeof(SenderSynHeader), 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
	
	printf("sendto_res: %d\n", send_res);
	if (send_res == SOCKET_ERROR) {
		printf("failure in sendto error %d\n", WSAGetLastError());
		return FAILED_SEND;
	}

	fd_set sockHolder;
	FD_ZERO(&sockHolder);
	FD_SET(sock, &sockHolder);

	// set timeout to 10 seconds 
	struct timeval timeout;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	
	int selectRes = select(0, &sockHolder, NULL, NULL, &timeout);
	printf("selectRes %d\n", selectRes);

	//char *answBuf = new char[sizeof(ReceiverHeader)];
	return 1;
}


int SenderSocket::open(const char *host, int port_no, int senderWindow, LinkProperties *lp) {
	printf("Open called, with host %s\n", host);

	//struct hostent *remote;
	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));

	in_addr addr;
	char *address;

	/*DWORD IP = inet_addr(host);

	if (IP == INADDR_NONE)
	{
		// if not a valid IP, then do a DNS lookup
		if ((remote = gethostbyname(host)) == NULL)
		{
			cout << "failed with " << WSAGetLastError() << endl;
			return -1;
		}
		else // take the first IP address and copy into sin_addr
		{
			memcpy((char *)&(server.sin_addr), remote->h_addr, remote->h_length);
			addr.s_addr = *(u_long *)remote->h_addr;
			address = inet_ntoa(addr);
		}
	}
	else
	{
		// if a valid IP, directly drop its binary version into sin_addr
		server.sin_addr.S_un.S_addr = IP;
		address = host;
	}*/
	
	// setup the port # and protocol type
	server.sin_family = AF_INET;
	server.sin_port = htons(0);
	server.sin_addr.s_addr = inet_addr(host);
	//inet_pton(AF_INET, host, &server.sin_addr);
	//server.sin_addr.s_addr = inet_addr(host);
	
	/*struct sockaddr_in local;
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(port_no);*/

	if (bind(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		cout << "Binder failed! " << WSAGetLastError()<<endl;
		//TODO: do something here
		return -1;
	}

	printf("bind successful\n");


	int attemptCount = 0;

	char *sendBuf = new char[sizeof(SenderSynHeader)];
	SenderSynHeader ssh, *sshp = (SenderSynHeader *)sendBuf;
	ssh.sdh.flags.SYN = 1;
	ssh.sdh.seq = 0;
	memcpy(&(ssh.lp), lp, sizeof(LinkProperties));

	memcpy(sendBuf, &ssh, sizeof(SenderSynHeader));

	char *answBuf = new char[sizeof(ReceiverHeader)];

	while (attemptCount++ < MAX_SYN_ATTEMPT_COUNT) {
		int sendto_res = sendto(sock, sendBuf, sizeof(SenderSynHeader), 0, (struct sockaddr *)&server, sizeof(server));
		printf("sendto_res: %d\n", sendto_res);
		if (sendto_res == SOCKET_ERROR) {
			printf("failure in sendto error %d\n", WSAGetLastError());
			return FAILED_SEND;
		}
		printf("sendto successful buf: %s\n", sendBuf);
		fd_set sockHolder;
		FD_ZERO(&sockHolder);
		FD_SET(sock, &sockHolder);

		// set timeout to 10 seconds 
		struct timeval timeout;
		timeout.tv_sec = 2; //TODO: change it to 10sec
		timeout.tv_usec = 0;

		int selectRes = select(0, &sockHolder, NULL, NULL, &timeout);
		printf("selectRes %d\n", selectRes);
		if (selectRes > 0) {

			struct sockaddr_in response;
			memset(&response, 0, sizeof(response));
			response.sin_family = AF_INET;
			response.sin_port = htons(port_no);
			response.sin_addr.s_addr = server.sin_addr.S_un.S_addr;
			int response_size = sizeof(response);

			int recv_res;
			if ((recv_res = recvfrom(sock, (char *)answBuf, sizeof(ReceiverHeader), 0,
				(struct sockaddr*)&response, &response_size)) == SOCKET_ERROR) {
				printf("Socket Error %d... Exiting!\n\n", WSAGetLastError());
				//TODO: check what code should be returned
				return -1;
			}

			printf("recv_res: %d\n", recv_res);
			return 0;
		}
	}

	return -1;
}


SenderSocket::~SenderSocket()
{
}
