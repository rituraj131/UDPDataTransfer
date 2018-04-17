#include "common.h"
#include "SenderSocket.h"
#include "Checksum.h"
#include <thread>

bool isCloseCalled = false;
void statsThread(SenderSocket *, UINT64 *, DWORD, DWORD);
void workerThread(SenderSocket *);

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
		//system("pause");
		return -1;
	}

	LinkProperties lp;
	lp.RTT = RTT;
	lp.speed = 100000.0f * atof(argv[7]); // convert to megabits
	lp.pLoss[FORWARD_PATH] = atof(argv[5]);
	lp.pLoss[RETURN_PATH] = atof(argv[6]);
	lp.bufferSize = senderWindow;
	SenderSocket ss(senderWindow);
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
	DWORD statStartTime = timeGetTime();

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	thread statsThread(statsThread, &ss, &off, time, statStartTime);
	thread workerThread(workerThread, &ss);

	DWORD sendStartTime = timeGetTime();
	int count = 0;
	while (off < byteBufferSize)
	{
		// decide the size of next chunk
		int bytes = min(byteBufferSize - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));
		status = 0;
		ss.Send(charBuf + off, bytes);

		if (status != STATUS_OK) {
			printf("Main:\send failed with status %d\n", status);
			isCloseCalled = true;
			if (statsThread.joinable())
				statsThread.join();
			if (workerThread.joinable())
				workerThread.join();
			WSACleanup();
			system("pause");
			return -1;
		}

		off += bytes;
		count++;
	}
	
	float totalSendTime = timeGetTime() - sendStartTime; //in ms
	ss.allPacketsSent = true;
	
	WaitForSingleObject(ss.closingWorker, INFINITE);
	isCloseCalled = true;
	//printf("Main closing\n");
	Checksum cs;
	UINT32 crc32_Close = 1;
	if ((status = ss.Close(senderWindow, &lp, statStartTime, &crc32_Close)) != STATUS_OK) {
		printf("Main:\tdisconnect failed with status %d\n", status);
		if (statsThread.joinable())
			statsThread.join();
		if (workerThread.joinable())
			workerThread.join();
		WSACleanup();
		system("pause");
		return -1;
	}

	UINT32 crc32_recv = cs.CRC32((unsigned char *)charBuf, byteBufferSize);
	
	if (crc32_Close != crc32_recv) {
		printf("Checksum provided by receiver %X does not match to the checksum %X across the sent buffer\n", crc32_Close, crc32_recv);
	}

	float final_speed = (ss.nextSeq * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader))) / totalSendTime;
	printf("Main:\ttransfer finished in %0.3f sec, %0.3f Kbps checksum %X\n", (float)totalSendTime / 1000, final_speed, 
		crc32_recv);
	printf("Main:\testRTT %0.3f, ideal rate %0.3f Kbps\n", ss.prev_est_RTT,
		(MAX_PKT_SIZE)* 8/(ss.prev_est_RTT * 1000));

	if (statsThread.joinable())
		statsThread.join();
	if (workerThread.joinable())
		workerThread.join();

	WSACleanup();
	system("pause");
	return 0;
}

void workerThread(SenderSocket *ss) {
	ss->WorkerRun();
}

void statsThread(SenderSocket *ss, UINT64 *off, DWORD time, DWORD startThreadTime) {
	int lastBase = 0;
	//printf("starting stats thread\n");
	while (true) {
		Sleep(2000);
		//printf("stats isCloseCalled %d\n", isCloseCalled);
		if (isCloseCalled)
			break;

		float time_elapsed = (float)(timeGetTime() - time) / 1000;
		float data_send = (float)*off/ 1000000; //MB
		
		int packets_sent = ss->nextSeq - lastBase;
		lastBase = ss->nextSeq;
		float speed = (float)(packets_sent * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader))) / (2* 1000000);
		
		printf("[%2.0f] B\t%6u (%0.1f MB) N\t%6u T %d F %d W %d S %0.3f Mbps RTT %0.3f\n", time_elapsed, ss->nextSeq,
			data_send, ss->nextSeq +1, ss->retrasmitted_pkt_count, ss->fast_retransmit_count, ss->effectiveWindow,
			speed, ss->prev_est_RTT);
	}
}
