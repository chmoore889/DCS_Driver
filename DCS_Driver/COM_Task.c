#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <process.h>
#include <ws2tcpip.h>

#include "DCS_Driver.h"
#include "Internal.h"

// Pointer to the transmission FIFO head
static Transmission_Data_Type* pTrans_FIFO_Head = NULL;
// Pointer to the transmission FIFO tail
static Transmission_Data_Type* pTrans_FIFO_Tail = NULL;

static HANDLE threadHandle;
static HANDLE hRunMutex;
static HANDLE hFIFOMutex;

//Removes the first item in the FIFO and returns its pointer.
static Transmission_Data_Type* Dequeue_Trans_FIFO(void);

//Work done by the COM task thread.
static void COM_Task(void* address);

static int init_FIFO_mutex(void);
static int close_FIFO_mutex(void);
static inline void set_FIFO_mutex(void);
static inline void release_FIFO_mutex(void);

//Sends data passed to function and releases it when finished.
static int send_data(SOCKET ConnectSocket, Transmission_Data_Type* data_to_send);

//Receives data from socket. Returns >0 on fatal error, <0 on non-fatal error.
static int recv_data(SOCKET ConnectSocket);

static int process_recv(char* buff, unsigned __int32 buffLen);

int Initialize_COM_Task(DCS_Address address) {
	if (threadHandle != NULL || hRunMutex != NULL) {
		return THREAD_ALREADY_EXISTS;
	}

	//Initialize a set mutex
	hRunMutex = CreateMutexW(NULL, true, NULL);
	if (hRunMutex == NULL) {
		return THREAD_START_ERROR;
	}

	int result = init_FIFO_mutex();
	if (result != NO_DCS_ERROR) {
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		return result;
	}

	threadHandle = (HANDLE)_beginthread(COM_Task, 0, &address);
	if (threadHandle == NULL || PtrToLong(threadHandle) == -1L) {
		CloseHandle(hRunMutex);
		close_FIFO_mutex();
		threadHandle = NULL;
		return THREAD_START_ERROR;
	}

	return NO_DCS_ERROR;
}

int Destroy_COM_Task() {
	if (hRunMutex != NULL) {
		ReleaseMutex(hRunMutex);

		WaitForSingleObject(threadHandle, INFINITE);

		WaitForSingleObject(hRunMutex, INFINITE);
		CloseHandle(hRunMutex);

		close_FIFO_mutex();

		threadHandle = NULL;
		hRunMutex = NULL;
	}
	return NO_DCS_ERROR;
}

static int send_data(SOCKET ConnectSocket, Transmission_Data_Type* data_to_send) {
	//Prepend frame size (excluding itself) to the frame
	char* tmp = realloc(data_to_send->pFrame, data_to_send->size + sizeof(data_to_send->size));
	if (tmp == NULL) {
		free(data_to_send->pFrame);
		return 1;
	}

	data_to_send->pFrame = tmp;

	//Shift pFrame sizeof(data_to_send->size) bytes
	memmove(&data_to_send->pFrame[sizeof(data_to_send->size)], data_to_send->pFrame, data_to_send->size);

#pragma warning (disable: 6386)
	//Write prepended data_to_send->size on the packet
	memcpy(data_to_send->pFrame, &data_to_send->size, sizeof(data_to_send->size));
#pragma warning (default: 6386)

	hexDump("Data packet", data_to_send->pFrame, data_to_send->size + sizeof(data_to_send->size));
	int iResult = send(ConnectSocket, data_to_send->pFrame, data_to_send->size + sizeof(data_to_send->size), 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
	}
	free(data_to_send->pFrame);
	free(data_to_send);

	printf("Bytes Sent: %d\n", iResult);

	return iResult;
}

static int recv_data(SOCKET ConnectSocket) {
	int iResult = 0;
	char socket_buffer[1024];
	char* frame_data = NULL;
	unsigned int frame_data_size = 0;

	//Receive until the peer closes the connection
	do {
		iResult = recv(ConnectSocket, socket_buffer, sizeof(socket_buffer), 0);
		if (iResult > 0) {
			frame_data_size += iResult;

			char* tmp = realloc(frame_data, frame_data_size);
			if (tmp == NULL) {
				free(frame_data);

				closesocket(ConnectSocket);
				WSACleanup();

				return 1;
			}

			frame_data = tmp;

			memcpy(&frame_data[frame_data_size - iResult], socket_buffer, iResult);
		}
		else if (iResult == 0) {
			printf("Connection closed\n");
		}
		else if (iResult < 0) {
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK) {
				break;
			}
			else {
				printf("recv failed with error: %d\n", WSAGetLastError());
			}
		}
	} while (iResult > 0);

	if (frame_data_size > 0) {
		hexDump("recv", frame_data, frame_data_size);
	}

	for (unsigned __int32 totLen = 0; totLen < frame_data_size;) {
		//Strip off 32 bit integer size from beginning of frame to make well-defined DCS frame `buff`
		unsigned __int32 frameLen;
		memcpy(&frameLen, &frame_data[totLen], sizeof(frameLen));

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

static void COM_Task(void* address) {
	DCS_Address* dcs_address = (DCS_Address*)address;

	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo* result = NULL,
		* ptr = NULL,
		hints;
	int iResult;

	//Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		_endthread();
		return;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	//Resolve the server address and port
	iResult = getaddrinfo(dcs_address->address, dcs_address->port, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		_endthread();
		return;
	}

	//Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		//Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("Socket failed with error: %d\n", WSAGetLastError());
			WSACleanup();

			_endthread();
			return;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			printf("Connection to server failed - failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		//printf("connected to server\n");
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();

		_endthread();
		return;
	}

	//Make the socket non-blocking.
	u_long iMode = 1;
	iResult = ioctlsocket(ConnectSocket, FIONBIO, &iMode);
	if (iResult != NO_ERROR) {
		printf("ioctlsocket failed with error: %ld\n", iResult);
	}

	//Repeat while RunMutex is still taken.
	while (WaitForSingleObject(hRunMutex, 50) == WAIT_TIMEOUT) {
		Transmission_Data_Type* data_to_send = Dequeue_Trans_FIFO();
		if (data_to_send != NULL) {
			iResult = send_data(ConnectSocket, data_to_send);
		}

		iResult = recv_data(ConnectSocket);

		if (iResult > 0) {
			printf(ANSI_COLOR_RED"Fatal Receive Error\n"ANSI_COLOR_RESET);
			WSACleanup();

			_endthread();
			return;
		}
	}

	//Cleanup
	closesocket(ConnectSocket);
	WSACleanup();

	ReleaseMutex(hRunMutex);
	_endthread();
}

int Enqueue_Trans_FIFO(Transmission_Data_Type* pTransmission) {
	pTransmission->pNextItem = NULL;

	set_FIFO_mutex();

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

	release_FIFO_mutex();

	return NO_DCS_ERROR;
}

static Transmission_Data_Type* Dequeue_Trans_FIFO() {
	set_FIFO_mutex();

	Transmission_Data_Type* pTransmission = pTrans_FIFO_Head;

	//If the original FIFO head isn't null, shift the queue.
	if (pTransmission != NULL) {
		pTrans_FIFO_Head = pTrans_FIFO_Head->pNextItem;
	}

	//If the new FIFO head is null, the queue is empty.
	if (pTrans_FIFO_Head == NULL) {
		pTrans_FIFO_Tail = NULL;
	}

	release_FIFO_mutex();

	return pTransmission;
}

static int init_FIFO_mutex() {
	if (hFIFOMutex != NULL) {
		return THREAD_ALREADY_EXISTS;
	}

	hFIFOMutex = CreateMutexW(NULL, false, NULL);
	if (hFIFOMutex == NULL) {
		return THREAD_START_ERROR;
	}
	return NO_DCS_ERROR;
}

static int close_FIFO_mutex() {
	if (hFIFOMutex == NULL) {
		return NO_DCS_ERROR;
	}

	int result = CloseHandle(hFIFOMutex);

	hFIFOMutex = NULL;

	return result;
}

static inline void set_FIFO_mutex() {
	if (hFIFOMutex == NULL) {
		return;
	}

	WaitForSingleObject(hFIFOMutex, INFINITE);
}

static inline void release_FIFO_mutex() {
	if (hFIFOMutex == NULL) {
		return;
	}

	ReleaseMutex(hFIFOMutex);
}

static int process_recv(char* buff, unsigned __int32 buffLen) {
	hexDump("process_recv", buff, buffLen);

	//Verify checksum
	if (!check_checksum(buff, buffLen)) {
		printf("Checksum error\n");
		return FRAME_CHECKSUM_ERROR;
	}

	//Ensure header is correct
	unsigned __int16 header;
	memcpy(&header, &buff[0], HEADER_SIZE);
	header = itohs(header);
	if (header != FRAME_VERSION) {
		printf("Invalid header\n");
		return FRAME_VERSION_ERROR;
	}


	//Ensure type id is correct
	unsigned __int32 type_id;
	memcpy(&type_id, &buff[2], TYPE_ID_SIZE);
	type_id = itohl(type_id);
	if (type_id != DATA_ID) {
		printf("Invalid Type ID\n");
		return FRAME_INVALID_DATA;
	}

	//Call correct callbacks based on data id
	Data_ID data_id;
	memcpy(&data_id, &buff[6], DATA_ID_SIZE);
	data_id = itohl(data_id);

	unsigned int pDataBuffLen = buffLen - DATA_ID_SIZE - TYPE_ID_SIZE - HEADER_SIZE - CHECKSUM_SIZE;
	char* pDataBuff = malloc(pDataBuffLen);
	if (pDataBuff == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(pDataBuff, &buff[10], pDataBuffLen);

	int err = NO_DCS_ERROR;
	switch (data_id) {
	case GET_DCS_STATUS:
		err = Receive_DCS_Status(pDataBuff);
		break;

	case GET_CORRELATOR_SETTING:
		err = Receive_Correlator_Setting(pDataBuff);
		break;

	case GET_ANALYZER_SETTING:
		err = Receive_Analyzer_Setting(pDataBuff);
		break;

	case GET_SIMULATED_DATA:
		err = Receive_Simulated_Correlation(pDataBuff);
		break;

	case GET_ANALYZER_PREFIT_PARAM:
		err = Receive_Analyzer_Prefit_Param(pDataBuff);
		break;

	case COMMAND_ACK:
		err = Receive_Command_ACK(pDataBuff);
		break;

	case GET_ERROR_MESSAGE:
		err = Receive_Error_Message(pDataBuff);
		break;

	default:
		printf(ANSI_COLOR_RED"Invalid Data ID\n"ANSI_COLOR_RESET);
		err = FRAME_INVALID_DATA;
	}

	free(pDataBuff);

	return err;
}