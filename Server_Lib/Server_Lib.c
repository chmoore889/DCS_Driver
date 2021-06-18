#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <stdbool.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#include "Server_Lib.h"
#include "Internal.h"
#include "Store.h"

#pragma comment (lib, "Ws2_32.lib")

static HANDLE threadHandle;
static HANDLE hRunMutex;

static void Listen_And_Handle(void* socket_ptr);

static int make_socket_nonblocking(SOCKET socket);

static Transmission_Data_Type* Dequeue_Trans_FIFO(void);

// Pointer to the transmission FIFO head
static Transmission_Data_Type* pTrans_FIFO_Head = NULL;
// Pointer to the transmission FIFO tail
static Transmission_Data_Type* pTrans_FIFO_Tail = NULL;

static void clear_Trans_FIFO(void);

//Sends data passed to function and releases it when finished. Returns <0 on error.
static int send_data(SOCKET ConnectSocket, Transmission_Data_Type* data_to_send);

//Receives data from socket. Returns >0 on fatal error, <0 on non-fatal error.
static int recv_data(SOCKET ConnectSocket);

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

	iResult = init_Store();
	if (iResult != NO_DCS_ERROR) {
		closesocket(ListenSocket);
		WSACleanup();
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		return THREAD_START_ERROR;
	}

	// Start thread to listen for connections
	SOCKET* heapSock = malloc(sizeof(ListenSocket));
	if (heapSock == NULL) {
		closesocket(ListenSocket);
		WSACleanup();
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		close_Store();
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
	clear_Trans_FIFO();
	Cleanup_Logs();

	//If the COM task isn't already stopped, free all the task's resources.
	if (hRunMutex != NULL) {
		//Release the run mutex to stop the thread.
		ReleaseMutex(hRunMutex);

		//Wait for thread to close.
		WaitForSingleObject(threadHandle, INFINITE);

		WaitForSingleObject(hRunMutex, INFINITE);
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
	}

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
		struct sockaddr addr;
		int addr_len = sizeof(addr);

		// Accept a client socket
		ClientSocket = accept(ListenSocket, &addr, &addr_len);
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
		//printf("Connected\n");
		struct sockaddr_in* addr_in = (struct sockaddr_in*)&addr;
		char ipStr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(addr_in->sin_addr), ipStr, INET_ADDRSTRLEN);
		unsigned short port = ntohs(addr_in->sin_port);
		
		char connectionMessage[50];
		_snprintf_s(connectionMessage, sizeof(connectionMessage), _TRUNCATE, "Connected to %s:%u", ipStr, port);
		Add_Log(connectionMessage);

		iResult = make_socket_nonblocking(ClientSocket);
		if (iResult != NO_ERROR) {
			printf("ioctlsocket failed with error: %ld\n", iResult);
			closesocket(ClientSocket);
			continue;
		}

		bool recv_failed = false;
		while (WaitForSingleObject(hRunMutex, 50) == WAIT_TIMEOUT) {
			Handle_Measurement();

			Transmission_Data_Type* data_to_send = Dequeue_Trans_FIFO();
			if (data_to_send != NULL) {
				iResult = send_data(ClientSocket, data_to_send);
				if (iResult < 0) {
					recv_failed = true;
					break;
				}
			}

			iResult = recv_data(ClientSocket);
			if (iResult > 0) {
				recv_failed = true;
				break;
			}
		}

		//If recv didn't fail, the thread should be ended.
		if (!recv_failed) {
			break;
		}
		Add_Log("Disconnected");
	}

	//Cleanup
	closesocket(ListenSocket);
	WSACleanup();

	ReleaseMutex(hRunMutex);
	_endthread();
}

static int send_data(SOCKET ConnectSocket, Transmission_Data_Type* data_to_send) {
	//Allocate more memory in preparation for prepending frame size (excluding itself) to the frame.
	char* tmp = realloc(data_to_send->pFrame, data_to_send->size + sizeof(data_to_send->size));
	if (tmp == NULL) {
		free(data_to_send->pFrame);
		free(data_to_send);
		return MEMORY_ALLOCATION_ERROR;
	}

	data_to_send->pFrame = tmp;

	//Shift pFrame sizeof(data_to_send->size) bytes to give space for the prepended frame size.
	memmove(&data_to_send->pFrame[sizeof(data_to_send->size)], data_to_send->pFrame, data_to_send->size);

#pragma warning (disable: 6386)
	//Write prepended data_to_send->size on the packet.
	memcpy(data_to_send->pFrame, &data_to_send->size, sizeof(data_to_send->size));
#pragma warning (default: 6386)

	hexDump("Data packet", data_to_send->pFrame, data_to_send->size + sizeof(data_to_send->size));

	//Send the data over the socket. data_to_send->size was not modified when prepending the frame size so it is added here.
	int iResult = send(ConnectSocket, data_to_send->pFrame, data_to_send->size + sizeof(data_to_send->size), 0);
	if (iResult == SOCKET_ERROR) {
		//printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
	}
	free(data_to_send->pFrame);
	free(data_to_send);

	//printf("Bytes Sent: %d\n", iResult);

	return iResult;
}

static int recv_data(SOCKET ConnectSocket) {
	int iResult = 0;
	char socket_buffer[1024];
	char* frame_data = NULL;
	unsigned int frame_data_size = 0;

	//Receives data waiting in buffer. Continues if no data available as socket is non-blocking.
	do {
		iResult = recv(ConnectSocket, socket_buffer, sizeof(socket_buffer), 0);
		if (iResult > 0) {
			//Data is available. Write it to the frame_data buffer.
			frame_data_size += iResult;

			char* tmp = realloc(frame_data, frame_data_size);
			if (tmp == NULL) {
				free(frame_data);

				closesocket(ConnectSocket);
				return 1;
			}

			frame_data = tmp;

			memcpy(&frame_data[frame_data_size - iResult], socket_buffer, iResult);
		}
		else if (iResult == 0) {
			//Should never occur due to non-blocking socket.

			//printf("Connection closed\n");
			closesocket(ConnectSocket);

			free(frame_data);
			return 1;
		}
		else if (iResult < 0) {
			//Socket should be set to non-blocking. This handles the would block error as success. Fails normally otherwise.
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK) {
				//No error here. No data available so break from loop.
				break;
			}
			else {
				//printf("recv failed with error: %d\n", err);
				closesocket(ConnectSocket);

				free(frame_data);
				return 1;
			}
		}
	} while (iResult > 0);

	if (frame_data_size > 0) {
		hexDump("recv", frame_data, frame_data_size);
	}

	//Loop over each frame that's available as multiple may have been received at once.
	for (unsigned __int32 totLen = 0; totLen < frame_data_size;) {
		//Strip off 32 bit integer size from beginning of frame to make well-defined DCS frame `buff`
		unsigned __int32 frameLen;
		memcpy(&frameLen, &frame_data[totLen], sizeof(frameLen));

		//Find pointer of received data and process it in process_recv.
		char* buff = &frame_data[sizeof(frameLen) + totLen];
		int tmpiResult = process_recv(buff, frameLen);
		if (tmpiResult != NO_DCS_ERROR) {
			iResult = tmpiResult;
		}

		totLen += sizeof(frameLen) + frameLen;
		//printf("%d", iResult);
	}
	free(frame_data);

	return iResult;
}

int Enqueue_Trans_FIFO(Transmission_Data_Type* pTransmission) {
	pTransmission->pNextItem = NULL;

	//If FIFO is empty, this element is both the head and tail.
	if (pTrans_FIFO_Head == NULL) {
		pTrans_FIFO_Head = pTransmission;
		pTrans_FIFO_Tail = pTransmission;
	}
	//Otherwise, add to the end of FIFO.
	else {
		pTrans_FIFO_Tail->pNextItem = pTransmission;
		pTrans_FIFO_Tail = pTransmission;
	}

	return NO_DCS_ERROR;
}

static Transmission_Data_Type* Dequeue_Trans_FIFO() {
	Transmission_Data_Type* pTransmission = pTrans_FIFO_Head;

	//If the original FIFO head isn't null, shift the queue.
	if (pTransmission != NULL) {
		pTrans_FIFO_Head = pTrans_FIFO_Head->pNextItem;
	}

	//If the new FIFO head is null, the queue is empty.
	if (pTrans_FIFO_Head == NULL) {
		pTrans_FIFO_Tail = NULL;
	}

	return pTransmission;
}


static void clear_Trans_FIFO(void) {
	Transmission_Data_Type* trans_data = pTrans_FIFO_Head;
	while (trans_data != NULL) {
		Transmission_Data_Type* tmp_item = trans_data;
		trans_data = trans_data->pNextItem;

		free(tmp_item->pFrame);
		free(tmp_item);
	}
	pTrans_FIFO_Head = NULL;
	pTrans_FIFO_Tail = NULL;
}

static int make_socket_nonblocking(SOCKET socket) {
	u_long iMode = 1;
	return ioctlsocket(socket, FIONBIO, &iMode);
}