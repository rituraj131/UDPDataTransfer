#pragma once
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <SDKDDKVer.h>
#include<WinSock2.h>
#include <Windows.h>
#include <stdio.h>
#include<iostream>
#include <string>
#include <ws2tcpip.h>
#include <algorithm>
#include <thread>

using namespace std;
