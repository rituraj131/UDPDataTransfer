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

	RTO = 1.0f;
	time = timeGetTime();
	send_seqnum = timeout_packet_count = goodput = 0;
	prev_dev_RTT = 0.0f;
	prev_est_RTT = 1.0f;
	memset(&sock_server, 0, sizeof(struct sockaddr_in));
}

int SenderSocket::Open(char *host, int port_no, int senderWindow, LinkProperties *lp) {
	if (sock_server.sin_port != 0) {
		return ALREADY_CONNECTED;
	}

	struct sockaddr_in local;
	memset(&local, 0, sizeof(local));

	local.sin_family = AF_INET;
	local.sin_port = htons(0);
	local.sin_addr.s_addr = htonl(INADDR_ANY);

	if (::bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
		cout << "Binder failed with error: " << WSAGetLastError() << endl;
		return -1;
	}

	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));

	struct hostent *remote;
	char *address;
	in_addr addr;

	DWORD IP = inet_addr(host);

	if (IP == INADDR_NONE)
	{
		// if not a valid IP, then do a DNS lookup
		if ((remote = gethostbyname(host)) == NULL){
			printf("[%0.3f] --> target %s is invalid\n", (float)(timeGetTime() - time) / 1000, host);
			return INVALID_NAME;
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

	lp->bufferSize += MAX_SYN_ATTEMPT_COUNT;
	RTO = max(1.0f, 2*lp->RTT);

	char *buf_SendTo = new char[sizeof(SenderSynHeader)];
	SenderSynHeader senderSyncHeader;
	senderSyncHeader.sdh.flags.SYN = 1;
	senderSyncHeader.sdh.flags.reserved = 0;
	senderSyncHeader.sdh.seq = 0;
	senderSyncHeader.lp = *lp;

	memcpy(buf_SendTo, &senderSyncHeader, sizeof(SenderSynHeader));
	
	char *answBuf = new char[sizeof(ReceiverHeader)];

	struct timeval timeout;

	int attemptCount = 0;

	while (attemptCount++ < MAX_SYN_ATTEMPT_COUNT) {
		/*printf("[%0.3f] --> SYN %d (attempt %d of %d, RTO %0.3f) to %s\n", (float)(timeGetTime() - time)/1000,
			senderSyncHeader.sdh.seq, attemptCount, MAX_SYN_ATTEMPT_COUNT, RTO, address);*/
		
		DWORD sendToTime = timeGetTime();
		fd_set sockHolder;
		FD_ZERO(&sockHolder);
		FD_SET(sock, &sockHolder);

		if (sendto(sock, (char *)buf_SendTo, sizeof(SenderSynHeader), 0, (struct sockaddr *)&server,
			sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
			printf("failed sendto with %d\n", WSAGetLastError());
			return FAILED_SEND;
		}
		
		int milliseconds = RTO * 1000;
		timeout.tv_sec = milliseconds / 1000;
		timeout.tv_usec = (milliseconds % 1000) * 1000;
		
		if (select(0, &sockHolder, NULL, NULL, &timeout) > 0) {
			int response_size = sizeof(server);

			int recv_res;
			if ((recv_res = recvfrom(sock, (char *)answBuf, sizeof(ReceiverHeader), 0,
				(struct sockaddr*)&server, &response_size)) == SOCKET_ERROR) {
				printf("failed recvfrom with %d\n",WSAGetLastError());
				return FAILED_RECV;
			}

			ReceiverHeader *receiverHeader = (ReceiverHeader *)answBuf;
			if (receiverHeader->flags.ACK != 1) continue;
			memcpy(&sock_server, &server, sizeof(struct sockaddr_in));

			//RTO = 3.0f * (float)(timeGetTime() - sendToTime)/1000; not needed anymore!

			/*printf("[%0.3f] <-- SYN-ACK %d window %d; setting initial RTO to %0.3f\n", (float)(timeGetTime() - time) / 1000,
				senderSyncHeader.sdh.seq, receiverHeader->recvWnd, RTO);*/
			return STATUS_OK;
		}
	}
	timeout_packet_count++;
	return TIMEOUT;
}

int SenderSocket::Send(char *buf, int bytes) {
	if (sock_server.sin_port == INVALID_SOCKET) {//not yet Opened!
		return NOT_CONNECTED;
	}

	char *sendBuf = new char[bytes + sizeof(SenderDataHeader)];

	SenderDataHeader senderDataHeader;
	senderDataHeader.seq = send_seqnum;

	memcpy(sendBuf, &senderDataHeader, sizeof(SenderDataHeader));
	memcpy(sendBuf + sizeof(SenderDataHeader), buf, bytes);
	
	DWORD time_before_recv = timeGetTime();
	if (sendto(sock, (char *)sendBuf, bytes + sizeof(SenderDataHeader), 0, (struct sockaddr *)&sock_server,
		sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		printf("failed Send sendto with %d\n", WSAGetLastError());
		return FAILED_SEND;
	}

	int attempt_count = 0;

	while (attempt_count++ < MAX_NONSYN_ATTEMPT_COUNT) {
		if (attempt_count > 1)
			timeout_packet_count++;

		fd_set sockHolder;
		FD_ZERO(&sockHolder);
		FD_SET(sock, &sockHolder);

		struct timeval timeout;
		int milliseconds = RTO * 1000;
		timeout.tv_sec = milliseconds / 1000;
		timeout.tv_usec = (milliseconds % 1000) * 1000;

		int s_res = select(0, &sockHolder, NULL, NULL, &timeout);

		if (s_res > 0) {
			int recv_res;
			int response_size = sizeof(sock_server);
			char *answBuf = new char[sizeof(ReceiverHeader)];

			if ((recv_res = recvfrom(sock, (char *)answBuf, sizeof(ReceiverHeader), 0,
				(struct sockaddr*)&sock_server, &response_size)) == SOCKET_ERROR) {
				printf("failed recvfrom with %d\n", WSAGetLastError());
				return FAILED_RECV;
			}

			ReceiverHeader *receiverHeader = (ReceiverHeader *)answBuf;
			//printf("curr seq: %d, receiver ackseq: %d\n", send_seqnum, receiverHeader->ackSeq);
			if (receiverHeader->flags.ACK != 1) return FAILED_SEND;

			send_seqnum++;
			
			if (attempt_count == 1) {
				float sample_time = (float)(timeGetTime() - time_before_recv) / 1000; //curr sample time in sec
				float estimated_RTT = (float)(1 - ALPHA) * prev_est_RTT + ALPHA * sample_time;
				float dev_RTT = (float)(1 - BETA) * prev_dev_RTT + BETA * abs(sample_time - estimated_RTT);
				RTO = estimated_RTT + (float) 4 * max(dev_RTT, 0.010f);
				prev_dev_RTT = dev_RTT;
				prev_est_RTT = estimated_RTT;
			}

			return STATUS_OK;
		}
	}
	

	return TIMEOUT;
}

int SenderSocket::Close(int senderWindow, LinkProperties *lp, DWORD startTime, UINT32 *crc32_Close) {
	if (sock_server.sin_port == INVALID_SOCKET) {//not yet Opened!
		return NOT_CONNECTED;
	}

	lp->bufferSize += MAX_NONSYN_ATTEMPT_COUNT;

	char *buf_SendTo = new char[sizeof(SenderSynHeader)];
	SenderSynHeader senderSyncHeader;
	senderSyncHeader.sdh.flags.FIN = 1;
	senderSyncHeader.sdh.flags.reserved = 0;
	senderSyncHeader.sdh.seq = send_seqnum;
	senderSyncHeader.lp = *lp;

	memcpy(buf_SendTo, &senderSyncHeader, sizeof(SenderSynHeader));

	char *answBuf = new char[sizeof(ReceiverHeader)];

	struct timeval timeout;

	int attemptCount = 0;
	
	while (attemptCount++ < MAX_NONSYN_ATTEMPT_COUNT) {
		/*printf("[%0.3f] --> FIN %d (attempt %d of %d, RTO %0.3f)\n", (float)(timeGetTime() - time) / 1000,
		senderSyncHeader.sdh.seq, MAX_FIN_ATTEMPT_COUNT, attemptCount, RTO);*/

		if (sendto(sock, (char *)buf_SendTo, sizeof(SenderSynHeader), 0, (struct sockaddr *)&sock_server,
			sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
			printf("failed sendto with %d\n", WSAGetLastError());
			return FAILED_SEND;
		}
		
		fd_set sockHolder;
		FD_ZERO(&sockHolder);
		FD_SET(sock, &sockHolder);
		
		int milliseconds = RTO * 1000;
		timeout.tv_sec = milliseconds / 1000;
		timeout.tv_usec = (milliseconds % 1000) * 1000;

		int select_res = select(0, &sockHolder, NULL, NULL, &timeout);
		
		if (select_res > 0) {
			int response_size = sizeof(sock_server);

			int recv_res;
			if ((recv_res = recvfrom(sock, (char *)answBuf, sizeof(ReceiverHeader), 0,
				(struct sockaddr*)&sock_server, &response_size)) == SOCKET_ERROR) {
				printf("failed recvfrom with %d\n", WSAGetLastError());
				return FAILED_RECV;
			}

			ReceiverHeader *receiverHeader = (ReceiverHeader *)answBuf;
			if (receiverHeader->flags.ACK != 1) continue;

			printf("[%2.2f] <-- FIN-ACK %d window %X\n", (float)(timeGetTime() - startTime) / 1000, receiverHeader->ackSeq, receiverHeader->recvWnd);
			/*printf("[%0.3f] <-- FIN-ACK %d window %d\n", (float)(timeGetTime() - time) / 1000,
				senderSyncHeader.sdh.seq, receiverHeader->recvWnd);*/

			*crc32_Close = receiverHeader->recvWnd;

			return STATUS_OK;
		}
	}

	return TIMEOUT;
}

SenderSocket::~SenderSocket()
{
}
