#include "common.h"
#include "SenderSocket.h"
#include "Checksum.h"
#include <thread>

bool isCloseCalled = false;
void statsThread(SenderSocket *, UINT64 *);
void printStatsOneLastTime(SenderSocket *, UINT64 *, DWORD);

int main(int argc, char **argv) {
	if (argc != 8) {
		cout << "Wrong Number of input parameters! Exiting..." << endl;
		return 0;
	}

	char *targetHost = argv[1];
	int power = atoi(argv[2]);
	int senderWindow = atoi(argv[3]);
	float RTT = atof(argv[4]);
	float lossProbabForward = atof(argv[5]);
	float lossProbabReverse = atof(argv[6]);
	int bottleNeckSpeed = atoi(argv[7]);

	printf("Main:\tsender W= %d, RTT %0.3f sec, loss %g / %g, link %d Mbps\n", senderWindow, RTT, lossProbabForward, lossProbabReverse, bottleNeckSpeed);
	printf("Main:\tinitializing DWORD array with 2^%d elements... ", power);

	DWORD time = timeGetTime();
	UINT64 dwordBufSize = (UINT64)1 << power;
	DWORD *dwordBuf = new DWORD[dwordBufSize]; // user-requested buffer
	for (UINT64 i = 0; i < dwordBufSize; i++) // required initialization
		dwordBuf[i] = i;

	printf("done in %d ms\n", timeGetTime() - time);

	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup failed with error : " << WSAGetLastError() << endl;
		WSACleanup();
		system("pause");
		return -1;
	}

	LinkProperties lp;
	lp.RTT = RTT;
	lp.speed = 1000000 * atof(argv[7]); // convert to megabits
	lp.pLoss[FORWARD_PATH] = atof(argv[5]);
	lp.pLoss[RETURN_PATH] = atof(argv[6]);
	lp.bufferSize = senderWindow;
	SenderSocket ss;
	int status;

	time = timeGetTime();

	if ((status = ss.Open(targetHost, MAGIC_PORT, senderWindow, &lp)) != STATUS_OK) {
		printf("Main:\tconnect failed with status %d\n", status);
		WSACleanup();
		system("pause");
		return -1;
	}

	printf("Main:\tconnected to %s in %0.3f sec, pkt size %d bytes\n", targetHost, 
		(float)(timeGetTime() - time)/1000, MAX_PKT_SIZE);
	time = timeGetTime();

	char *charBuf = (char*)dwordBuf; // this buffer goes into socket
	UINT64 byteBufferSize = dwordBufSize << 2; // convert to bytes

	UINT64 off = 0; // current position in buffer
	thread statsThread(statsThread, &ss, &off);
	while (off < byteBufferSize)
	{
		// decide the size of next chunk
		int bytes = min(byteBufferSize - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));
		// send chunk into socket 
		if ((status = ss.Send(charBuf + off, bytes)) != STATUS_OK) {
			printf("Main:\tsend failed with status %d\n", status);
			WSACleanup();
			system("pause");
			return -1;
		}

		off += bytes;
	}

	isCloseCalled = true;
	printStatsOneLastTime(&ss, &off, time);

	if ((status = ss.Close(senderWindow, &lp)) != STATUS_OK) {
		printf("Main:\tdisconnect failed with status %d\n", status);
		WSACleanup();
		system("pause");
		return -1;
	}

	Checksum cs;
	uint32_t crc32_recv = cs.CRC32((unsigned char *)charBuf, byteBufferSize);

	printf("Main:\ttransfer finished in %0.3f sec, checksum %X\n", (float)(timeGetTime() - time) / 1000, crc32_recv);

	if (statsThread.joinable())
		statsThread.join();

	WSACleanup();
	system("pause");
	return 0;
}

void statsThread(SenderSocket *ss, UINT64 *off) {
	DWORD startThreadTime = timeGetTime();

	while (true) {
		if (isCloseCalled)
			break;

		float speed = 0.000f;
		float RTT = 0.000f;
		DWORD currTime = timeGetTime() - startThreadTime;
		printf("[%0.3f] B\t%6u (%0.1f MB) N\t%6u T 0 F 0 W 1 S %0.3f Mbps RTT %0.3f\n", (float)currTime/1000, ss->send_seqnum, 
			*off/1000000, ss->send_seqnum+1, speed, RTT);

		Sleep(2000);
	}
}

void printStatsOneLastTime(SenderSocket *ss, UINT64 *off, DWORD time) {
	float speed = 0.000f;
	float RTT = 0.000f;
	printf("[%0.3f] B\t%6u (%0.1f MB) N\t%6u T 0 F 0 W 1 S %0.3f Mbps RTT %0.3f\n", (float)(timeGetTime() - time)/1000, ss->send_seqnum,
		*off / 1000000, ss->send_seqnum + 1, speed, RTT);
}
