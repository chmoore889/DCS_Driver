#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#include "Server_Lib.h"
#include "Internal.h"

#pragma comment (lib, "Ws2_32.lib")

static HANDLE threadHandle;
static HANDLE hRunMutex;

static void Listen_And_Handle(void* socket_ptr);

static int handle_recv(SOCKET ClientSocket);
static int make_socket_nonblocking(SOCKET socket);

int Start_Server(const char* port) {
	if (threadHandle != NULL || hRunMutex != NULL) {
		return THREAD_ALREADY_EXISTS;
	}

	WSADATA wsaData;

	struct addrinfo* addrResult = NULL;
	struct addrinfo hints;

	SOCKET ListenSocket = INVALID_SOCKET;

	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return NETWORK_INIT_ERROR;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, port, &hints, &addrResult);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return NETWORK_INIT_ERROR;
	}

	// Create a SOCKET
	ListenSocket = socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(addrResult);
		WSACleanup();
		return NETWORK_INIT_ERROR;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, addrResult->ai_addr, (int)addrResult->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(addrResult);
		closesocket(ListenSocket);
		WSACleanup();
		return NETWORK_INIT_ERROR;
	}

	freeaddrinfo(addrResult);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return NETWORK_INIT_ERROR;
	}

	//Initialize a set mutex for stopping the thread later.
	hRunMutex = CreateMutexW(NULL, true, NULL);
	if (hRunMutex == NULL) {
		closesocket(ListenSocket);
		WSACleanup();
		return THREAD_START_ERROR;
	}

	// Start thread to listen for connections
	SOCKET* heapSock = malloc(sizeof(ListenSocket));
	if (heapSock == NULL) {
		closesocket(ListenSocket);
		WSACleanup();
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		return MEMORY_ALLOCATION_ERROR;
	}
	*heapSock = ListenSocket;

	threadHandle = (HANDLE)_beginthread(Listen_And_Handle, 0, (void*)heapSock);
	if (threadHandle == NULL || PtrToLong(threadHandle) == -1L) {
		threadHandle = NULL;

		closesocket(ListenSocket);
		WSACleanup();

		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		return THREAD_START_ERROR;
	}

	return NO_DCS_ERROR;
}

int Stop_Server(void) {
	//Release the run mutex to stop the thread.
	ReleaseMutex(hRunMutex);

	//Wait for thread to close.
	WaitForSingleObject(threadHandle, INFINITE);

	WaitForSingleObject(hRunMutex, INFINITE);
	CloseHandle(hRunMutex);
	hRunMutex = NULL;

	return NO_DCS_ERROR;
}

static void Listen_And_Handle(void* socket_ptr) {
	SOCKET ListenSocket = *(SOCKET*)socket_ptr;
	free(socket_ptr);

	int iResult = 0;

	//Make socket non-blocking
	iResult = make_socket_nonblocking(ListenSocket);
	if (iResult != NO_ERROR) {
		printf("ioctlsocket failed with error: %ld\n", iResult);
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}

	while (WaitForSingleObject(hRunMutex, 50) == WAIT_TIMEOUT) {
		SOCKET ClientSocket = INVALID_SOCKET;

		// Accept a client socket
		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			const int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK) {
				continue;
			}

			printf("accept failed with error: %d\n", err);
			closesocket(ListenSocket);
			WSACleanup();
			return;
		}
		printf("Connected\n");

		iResult = make_socket_nonblocking(ClientSocket);
		if (iResult != NO_ERROR) {
			printf("ioctlsocket failed with error: %ld\n", iResult);
			closesocket(ClientSocket);
			continue;
		}

		bool recv_failed = false;
		while (WaitForSingleObject(hRunMutex, 50) == WAIT_TIMEOUT) {
			iResult = handle_recv(ClientSocket);
			if (iResult != NO_DCS_ERROR) {
				recv_failed = true;
				break;
			}
		}

		//If recv didn't fail, the thread should be ended.
		if (!recv_failed) {
			break;
		}
	}

	//Cleanup
	closesocket(ListenSocket);
	WSACleanup();

	ReleaseMutex(hRunMutex);
	_endthread();
}

static int handle_recv(SOCKET ClientSocket) {
	char* receivedData;
	unsigned __int32 receivedDataSize;

	//Get expected frame length
	int iResult = recv(ClientSocket, (char*)&receivedDataSize, sizeof(receivedDataSize), 0);

	if (iResult > 0) {
		if (iResult != sizeof(receivedDataSize)) {
			printf("There was a problem receiving the frame length\n");
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);

			return 1;
		}

		receivedData = malloc(receivedDataSize);
		if (receivedData == NULL) {
			closesocket(ClientSocket);
			return 1;
		}

		iResult = recv(ClientSocket, receivedData, receivedDataSize, 0);

		if (iResult < 0) {
			int err = WSAGetLastError();
			if (err != WSAEWOULDBLOCK) {
				printf("recv failed with error: %d\n", err);
				closesocket(ClientSocket);

				return 1;
			}
		}
		if (iResult == 0) {
			//Should never occur due to non-blocking socket.

			printf("Connection closed\n");
			closesocket(ClientSocket);

			return 1;
		}
		if (iResult != receivedDataSize) {
			printf("Unexpected amount of data received\n");
			closesocket(ClientSocket);

			return 1;
		}

		hexDump("Received", receivedData, receivedDataSize);

		char* to_send;
		unsigned __int32 to_send_size;
		iResult = process_recv(receivedData, receivedDataSize, &to_send, &to_send_size);
		printf("Process recv: %d\n", iResult);
		free(receivedData);

		hexDump("To send", to_send, to_send_size);

		if (iResult == NO_DCS_ERROR) {
			const int iSendResult = send(ClientSocket, to_send, to_send_size, 0);
			if (iSendResult == SOCKET_ERROR) {
				printf("send failed with error: %d\n", WSAGetLastError());
				closesocket(ClientSocket);

				free(to_send);

				return 1;
			}
			free(to_send);
		}
	}
	else if (iResult == 0) {
		//Should never occur due to non-blocking socket.

		printf("Connection closed\n");
		closesocket(ClientSocket);

		return 1;
	}
	else if (iResult < 0) {
		//Socket should be set to non-blocking. This handles the would block error as success. Fails normally otherwise.
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK) {
			//printf("recv failed with error: %d\n", err);
			closesocket(ClientSocket);

			return 1;
		}
	}
	return NO_DCS_ERROR;
}

static int make_socket_nonblocking(SOCKET socket) {
	u_long iMode = 1;
	return ioctlsocket(socket, FIONBIO, &iMode);
}