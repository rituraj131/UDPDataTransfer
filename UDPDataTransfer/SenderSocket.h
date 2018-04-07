#pragma once
#define MAGIC_PORT 22345 // receiver listens on this port
#define MAX_PKT_SIZE (1500-28) // maximum UDP packet size accepted by receiver 

// possible status codes from ss.Open, ss.Send, ss.Close
#define STATUS_OK 0 // no error
#define ALREADY_CONNECTED 1 // second call to ss.Open() without closing connection
#define NOT_CONNECTED 2 // call to ss.Send()/Close() without ss.Open()
#define INVALID_NAME 3 // ss.Open() with targetHost that has no DNS entry
#define FAILED_SEND 4 // sendto() failed in kernel
#define TIMEOUT 5 // timeout after all retx attempts are exhausted
#define FAILED_RECV 6 // recvfrom() failed in kernel

#define MAX_SYN_ATTEMPT_COUNT 3
#define MAX_FIN_ATTEMPT_COUNT 5

#include "common.h"
#include "TransferProp.h"

//status of SYN
#define SYN_STATUS_NONE 0
#define SYN_STATUS_STARTED 1
#define SYN_STATUS_COMPLETED_SUCCESSFULLY 2
#define SYN_STATUS_FAILED 3

class SenderSocket
{
	SOCKET sock;
	struct sockaddr_in sock_server;
public:
	float RTO;
	DWORD time;
	int send_seqnum;

	SenderSocket();
	int Open(char *, int, int, LinkProperties *);
	int Close(int, LinkProperties *);
	int Send(char *, int);
	~SenderSocket();
};

