#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <time.h>
#include <process.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

#include "DCS_Driver.h"
#include "Internal.h"
#include "COM_Task.h"

// Pointer to the transmission FIFO head
static Transmission_Data_Type* pTrans_FIFO_Head = NULL;
// Pointer to the transmission FIFO tail
static Transmission_Data_Type* pTrans_FIFO_Tail = NULL;

static Received_Data_Item* pRecv_Data_FIFO_Head = NULL;
static Received_Data_Item* pRecv_Data_FIFO_Tail = NULL;

//Handle of the COM task thread.
static HANDLE threadHandle;
//Handle of the mutex for destroying the COM task.
static HANDLE hRunMutex;
//Handle of the mutex for adding to the FIFO queue.
static HANDLE hFIFOMutex;

static HANDLE hRecvDataMutex;
static inline void set_Recv_mutex();
static inline void release_Recv_mutex();

//Removes the first item in the FIFO and returns its pointer.
static Transmission_Data_Type* Dequeue_Trans_FIFO(void);

int Enqueue_Recv_FIFO(Received_Data_Item* pTransmission);

//Work done by the COM task thread.
static void COM_Task(void* address);

//Initializes the handle for hFIFOMutex.
static int init_FIFO_mutex(void);
//Releases the handle for hFIFOMutex.
static int close_FIFO_mutex(void);
//Takes control of the hFIFOMutex.
static inline void set_FIFO_mutex(void);
//Releases control of the hFIFOMutex.
static inline void release_FIFO_mutex(void);

//Sends data passed to function and releases it when finished. Returns <0 on error.
static int send_data(SOCKET ConnectSocket, Transmission_Data_Type* data_to_send);

//Receives data from socket. Returns >0 on fatal error, <0 on non-fatal error.
static int recv_data(SOCKET ConnectSocket);

//Processes the raw data from the DCS. Takes a pointer to a DCS frame *excluding* the prepended frame size.
static int process_recv(char* buff, unsigned __int32 buffLen);

//Callbacks to call when the host receives data from the DCS.
static Receive_Callbacks callbacks;
//Whether the received data should be stored on the heap to be manually emptied.
static bool should_store;
//Handle of the mutex for controlling addition to the [callbacks] struct.
static HANDLE hCallbacksMutex;

//Initializes the handle for hCallbacksMutex.
static int init_Callback_mutex(void);
//Releases the handle for hCallbacksMutex.
static int close_Callback_mutex(void);
//Thread-safe method of setting the callbacks variable.
static void set_Callbacks(Receive_Callbacks local_callbacks, bool local_should_store);
//Thread-safe method of retrieving the callbacks variable.
static void get_Callbacks(Receive_Callbacks* local_callbacks, bool* local_should_store);

//Initializes the handle for hRecvDataMutex.
static int init_Recv_mutex(void);
//Releases the handle for hRecvDataMutex.
static int close_Recv_mutex(void);

//Clears out data from the recv FIFO.
static void clear_Recv_FIFO(void);
//Clears out data from the trans FIFO.
static void clear_Trans_FIFO(void);

static double last_Response_Time = 0.0;
//Checks if difference between the last response and the current time is greater than CHECK_CONNECTION_FREQ.
//Sends keep-alive command to maintain connection.
static void check_Timer(void);
//Resets last response time. Meant to be called when a response is receieved.
static void reset_Timer(void);

int Initialize_COM_Task(DCS_Address address, Receive_Callbacks local_callbacks, bool local_should_store) {
	//Set the callback functions for receiving data whether the COM task exists or not.
	//If the callbacks mutex isn't NULL, the COM task is already running so use the thread-safe callback setter.
	if (hCallbacksMutex != NULL) {
		set_Callbacks(local_callbacks, local_should_store);
	}
	//Otherwise, the thread-safe callback setter cannot be used and it's safe to set it directly.
	else {
		callbacks = local_callbacks;
		should_store = local_should_store;
	}

	//Return if a COM task already exists.
	if (threadHandle != NULL || hRunMutex != NULL) {
		return THREAD_ALREADY_EXISTS;
	}

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
		return NETWORK_INIT_ERROR;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	//Resolve the server address and port.
	iResult = getaddrinfo(address.address, address.port, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return NETWORK_INIT_ERROR;
	}

	//Attempt to connect to an address until one succeeds.
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		//Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("Socket failed with error: %d\n", WSAGetLastError());
			WSACleanup();
			return NETWORK_INIT_ERROR;
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
		return NETWORK_INIT_ERROR;
	}

	//Check_Command_Response(COMMAND_RSP_RESET, 0);

	//Make the socket non-blocking for convenience when receiving data in the COM thread.
	u_long iMode = 1;
	iResult = ioctlsocket(ConnectSocket, FIONBIO, &iMode);
	if (iResult != NO_ERROR) {
		printf("ioctlsocket failed with error: %ld\n", iResult);
		WSACleanup();
		return NETWORK_INIT_ERROR;
	}

	reset_Timer();
	Check_Command_Response(reset, 0);

	//Initialize a set mutex for stopping the thread later.
	hRunMutex = CreateMutexW(NULL, true, NULL);
	if (hRunMutex == NULL) {
		return THREAD_START_ERROR;
	}

	iResult = init_FIFO_mutex();
	if (iResult != NO_DCS_ERROR) {
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		return iResult;
	}

	iResult = init_Callback_mutex();
	if (iResult != NO_DCS_ERROR) {
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		close_FIFO_mutex();
		return iResult;
	}

	iResult = init_Recv_mutex();
	if (iResult != NO_DCS_ERROR) {
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		close_FIFO_mutex();
		close_Callback_mutex();
		return iResult;
	}

	//Start the COM task thread, calling the COM_Task function.
	SOCKET* heapSock = malloc(sizeof(ConnectSocket));
	if (heapSock == NULL) {
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		close_FIFO_mutex();
		close_Callback_mutex();
		close_Recv_mutex();
		return MEMORY_ALLOCATION_ERROR;
	}
	*heapSock = ConnectSocket;

	threadHandle = (HANDLE)_beginthread(COM_Task, 0, (void*)heapSock);
	if (threadHandle == NULL || PtrToLong(threadHandle) == -1L) {
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		close_FIFO_mutex();
		close_Callback_mutex();
		close_Recv_mutex();
		threadHandle = NULL;
		return THREAD_START_ERROR;
	}

	return NO_DCS_ERROR;
}

int Destroy_COM_Task() {
	clear_Recv_FIFO();

	clear_Trans_FIFO();

	//If the COM task isn't already stopped, free all the task's resources.
	if (hRunMutex != NULL) {
		//Release the run mutex to stop the thread.
		ReleaseMutex(hRunMutex);

		//Wait for thread to close.
		WaitForSingleObject(threadHandle, INFINITE);

		//Take back control of the run mutex so it can be closed.
		WaitForSingleObject(hRunMutex, INFINITE);
		CloseHandle(hRunMutex);

		//Close other mutexes.
		close_FIFO_mutex();
		close_Callback_mutex();
		close_Recv_mutex();

		//Deference threads to indicate they don't exist.
		threadHandle = NULL;
		hRunMutex = NULL;
	}

	return NO_DCS_ERROR;
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
		WSACleanup();
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
				WSACleanup();
				return 1;
			}

			frame_data = tmp;

			memcpy(&frame_data[frame_data_size - iResult], socket_buffer, iResult);
		}
		else if (iResult == 0) {
			//Should never occur due to non-blocking socket.

			//printf("Connection closed\n");
			closesocket(ConnectSocket);
			WSACleanup();

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
				WSACleanup();

				free(frame_data);
				return 1;
			}
		}
	} while (iResult > 0);

	if (frame_data_size > 0) {
		hexDump("recv", frame_data, frame_data_size);
		reset_Timer();
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

//Function run by the COM task thread. Initiates connection to the DCS
//and then continuously sends and receives data until Destroy_COM_Task is called.
static void COM_Task(void* socket_ptr) {
	SOCKET ConnectSocket = *(SOCKET*)socket_ptr;
	free(socket_ptr);

	int iResult = 0;

	//Repeat while RunMutex is still taken by the main thread. Clean up and exit when it's released.
	while (WaitForSingleObject(hRunMutex, 50) == WAIT_TIMEOUT) {
		int commandResp = Check_Command_Response(check, 0);
		if (commandResp == 2) {
			char message[] = "Error (0000): Command response timed out";
			Get_Error_Message_CB(message, (unsigned int)strlen(message));

			_endthread();
			return;
		}
		else if(commandResp == 0) {
			check_Timer();

			//If data is waiting in the queue, send it, one at a time.
			Transmission_Data_Type* data_to_send = Dequeue_Trans_FIFO();
			if (data_to_send != NULL) {
				Check_Command_Response(set, data_to_send->command_code);

				iResult = send_data(ConnectSocket, data_to_send);
				if (iResult < 0) {
					char message[50];
					//printf(ANSI_COLOR_RED"Sending Error\n"ANSI_COLOR_RESET);

					int errorCode = WSAGetLastError();
					if (errorCode == 0) {
						_snprintf_s(message, sizeof(message), _TRUNCATE, "Error (0000): Connection closed");
					}
					else {
						_snprintf_s(message, sizeof(message), _TRUNCATE, "Error (0000): send failed with error %d", errorCode);
					}
					Get_Error_Message_CB(message, (unsigned int)strlen(message));

					_endthread();
					return;
				}
			}
		}

		//Received and process data from the DCS.
		iResult = recv_data(ConnectSocket);

		if (iResult > 0) {
			//printf(ANSI_COLOR_RED"Fatal Receive Error\n"ANSI_COLOR_RESET);

			char message[50];

			int errorCode = WSAGetLastError();
			if (errorCode == 0) {
				_snprintf_s(message, sizeof(message), _TRUNCATE, "Error (0000): Connection closed");
			}
			else {
				_snprintf_s(message, sizeof(message), _TRUNCATE, "Error (0000): recv failed with error %d", WSAGetLastError());
			}
			Get_Error_Message_CB(message, (unsigned int) strlen(message));

			_endthread();
			return;
		}

		//Check_Command_Response(COMMAND_RSP_CHECK, 0);
	}

	//Cleanup
	closesocket(ConnectSocket);
	WSACleanup();

	ReleaseMutex(hRunMutex);
	_endthread();
}

int Check_Command_Response(Command_Option Option, Data_ID Command_Code) {
	static int Command_Rsp_Count = MAX_COMMAND_RESPONSE_TIME;
	static Data_ID Command_Sent;
	static bool Command_Ack = true;

	switch (Option) {
		case reset:
			Command_Rsp_Count = MAX_COMMAND_RESPONSE_TIME;
			Command_Ack = true;
			return 0;
		case set:
			Command_Ack = false;
			Command_Sent = Command_Code;
			return 0;
		case check:
			if (Command_Ack) {
				return 0;
			}
			if (Command_Rsp_Count > 0) {
				Command_Rsp_Count--;
				return 1;
			}
			return 2;
		case validate:
			if (Command_Code == Command_Sent) {
				return Check_Command_Response(reset, Command_Code);
			}
			return 1;
	}
	return 0;
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

	//Verify checksum.
	if (!check_checksum(buff, buffLen)) {
		printf("Checksum error\n");
		return FRAME_CHECKSUM_ERROR;
	}

	unsigned __int32 index = 0; //Index to track position in buff.

	//Ensure header is correct
	Frame_Version header;
	memcpy(&header, &buff[index], sizeof(header));
	index += sizeof(header);

	header = itohs(header);
	if (header != FRAME_VERSION) {
		printf("Invalid header\n");
		return FRAME_VERSION_ERROR;
	}

	//Ensure type id is correct.
	Type_ID type_id;
	memcpy(&type_id, &buff[index], sizeof(type_id));
	index += sizeof(type_id);

	type_id = itohl(type_id);
	if (type_id != DATA_ID) {
		printf("Invalid Type ID\n");
		return FRAME_INVALID_DATA;
	}

	//Get data id to later call correct callbacks based on data id.
	Data_ID data_id;
	memcpy(&data_id, &buff[index], sizeof(data_id));
	index += sizeof(data_id);

	data_id = itohl(data_id);

	//Obtain just the data portion of the frame and place in pDataBuff.
	unsigned int pDataBuffLen = buffLen - sizeof(data_id) - sizeof(type_id) - sizeof(header) - sizeof(Checksum);
	char* pDataBuff = malloc(pDataBuffLen);
	if (pDataBuff == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(pDataBuff, &buff[index], pDataBuffLen);
	index += pDataBuffLen;

	//Call the correct callbacks based on data id with pDataBuff.
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

		case GET_BFI_DATA:
			err = Receive_BFI_Data(pDataBuff);
			break;

		case GET_BFI_CORR_READY:
			err = Receive_BFI_Corr_Ready(pDataBuff);
			break;

		case GET_CORR_INTENSITY:
			err = Receive_Corr_Intensity_Data(pDataBuff);
			break;

		case GET_INTENSITY:
			err = Receive_Intensity_Data(pDataBuff);
			break;

		case GET_ERROR_ID:
			err = Receive_Error_Code(pDataBuff);
			break;

		default:
			printf(ANSI_COLOR_RED"Invalid Data ID: 0x%08X\n"ANSI_COLOR_RESET, data_id);
			err = FRAME_INVALID_DATA;
	}

	free(pDataBuff);

	return err;
}

int Enqueue_Recv_FIFO(Received_Data_Item* pRecv) {
	pRecv->pNextItem = NULL;

	set_Recv_mutex();

	//If FIFO is empty, this element is both the head and tail.
	if (pRecv_Data_FIFO_Head == NULL) {
		pRecv_Data_FIFO_Head = pRecv;
		pRecv_Data_FIFO_Tail = pRecv;
	}
	//Otherwise, add to the end of FIFO.
	else {
		pRecv_Data_FIFO_Tail->pNextItem = pRecv;
		pRecv_Data_FIFO_Tail = pRecv;
	}

	release_Recv_mutex();

	return NO_DCS_ERROR;
}

static int init_Recv_mutex() {
	if (hRecvDataMutex != NULL) {
		return THREAD_ALREADY_EXISTS;
	}

	hRecvDataMutex = CreateMutexW(NULL, false, NULL);
	if (hRecvDataMutex == NULL) {
		return THREAD_START_ERROR;
	}
	return NO_DCS_ERROR;
}

static int close_Recv_mutex() {
	if (hRecvDataMutex == NULL) {
		return NO_DCS_ERROR;
	}

	int result = CloseHandle(hRecvDataMutex);

	hRecvDataMutex = NULL;

	return result;
}

static inline void set_Recv_mutex() {
	if (hRecvDataMutex == NULL) {
		return;
	}

	WaitForSingleObject(hRecvDataMutex, INFINITE);
}

static inline void release_Recv_mutex() {
	if (hRecvDataMutex == NULL) {
		return;
	}

	ReleaseMutex(hRecvDataMutex);
}

static int init_Callback_mutex() {
	if (hCallbacksMutex != NULL) {
		return THREAD_ALREADY_EXISTS;
	}

	hCallbacksMutex = CreateMutexW(NULL, false, NULL);
	if (hCallbacksMutex == NULL) {
		return THREAD_START_ERROR;
	}
	return NO_DCS_ERROR;
}

static int close_Callback_mutex() {
	if (hCallbacksMutex == NULL) {
		return NO_DCS_ERROR;
	}

	int result = CloseHandle(hCallbacksMutex);

	hCallbacksMutex = NULL;

	return result;
}

static void set_Callbacks(Receive_Callbacks local_callbacks, bool local_should_store) {
	WaitForSingleObject(hCallbacksMutex, INFINITE);
	callbacks = local_callbacks;
	should_store = local_should_store;
	ReleaseMutex(hCallbacksMutex);
}

static void get_Callbacks(Receive_Callbacks* local_callbacks, bool* local_should_store) {
	WaitForSingleObject(hCallbacksMutex, INFINITE);
	*local_callbacks = callbacks;
	*local_should_store = should_store;
	ReleaseMutex(hCallbacksMutex);
}

static void clear_Recv_FIFO(void) {
	set_Recv_mutex();
	Received_Data_Item* item = pRecv_Data_FIFO_Head;
	while (item != NULL) {
		if (item->data_type == Corr_Intensity_Data_Type) {
#pragma warning (disable: 6001)
			Array_Data array_data[2] = { 0 };
			memcpy(array_data, item->data, sizeof(*array_data) * 2);

			Corr_Intensity_Data* data = array_data[0].ptr;
			for (int x = 0; x < array_data[0].length; x++) {
				free(data[x].pCorrBuf);
			}

			free(array_data[0].ptr);
			free(array_data[1].ptr);
#pragma warning (default: 6001)
		}
		else if (item->data_type > After_This_Are_Arrays) {
			Array_Data array_data = { 0 };
			memcpy(&array_data, item->data, sizeof(array_data));

			free(array_data.ptr);
		}

		Received_Data_Item* tmp_item = item;
		item = item->pNextItem;

		free(tmp_item->data);
		free(tmp_item);
	}
	pRecv_Data_FIFO_Head = NULL;
	pRecv_Data_FIFO_Tail = NULL;
	release_Recv_mutex();
}

static void clear_Trans_FIFO(void) {
	set_FIFO_mutex();
	Transmission_Data_Type* trans_data = pTrans_FIFO_Head;
	while (trans_data != NULL) {
		Transmission_Data_Type* tmp_item = trans_data;
		trans_data = trans_data->pNextItem;

		free(tmp_item->pFrame);
		free(tmp_item);
	}
	pTrans_FIFO_Head = NULL;
	pTrans_FIFO_Tail = NULL;
	release_FIFO_mutex();
}

static void check_Timer(void) {
	const clock_t currTime = clock();
	const double currTimeSec = currTime / CLOCKS_PER_SEC;

	if ((currTimeSec - last_Response_Time) >= CHECK_CONNECTION_FREQ) {
		Send_Check_Network();
		//printf("Checking Network\n");
	}
}

static void reset_Timer(void) {
	const clock_t currTime = clock();
	last_Response_Time = currTime / CLOCKS_PER_SEC;
}

//////////////////////////////////////////////////////////////////////////////////////
//Each of the following functions are internallly called callbacks. They will call the
//user-defined callback it was passed in Initialize_COM_Task.
//////////////////////////////////////////////////////////////////////////////////////

#define GETTER_FUNCTION(arg) int Get_##arg##_Data(arg ## * output) {\
	set_Recv_mutex();\
	Received_Data_Item* item = pRecv_Data_FIFO_Head;\
	Received_Data_Item* prev_item = NULL;\
	while (item != NULL) {\
		if (item->data_type == arg ## _Type) {\
			memcpy(output, item->data, sizeof(*output));\
\
			if (prev_item != NULL) {\
				prev_item->pNextItem = item->pNextItem;\
				if (prev_item->pNextItem == NULL) {\
					pRecv_Data_FIFO_Tail = prev_item;\
				}\
			}\
			else {\
				pRecv_Data_FIFO_Head = item->pNextItem;\
				if (pRecv_Data_FIFO_Head == NULL) {\
					pRecv_Data_FIFO_Tail = NULL;\
				}\
			}\
\
			release_Recv_mutex();\
\
			free(item->data);\
			free(item);\
			return NO_DCS_ERROR;\
		}\
\
		prev_item = item;\
		item = item->pNextItem;\
	}\
	release_Recv_mutex();\
	return 1;\
}

#define ARRAY_GETTER_FUNCTION(arg) int Get_##arg##_Data(arg ## ** output, int* number) {\
	set_Recv_mutex();\
	Received_Data_Item* item = pRecv_Data_FIFO_Head;\
	Received_Data_Item* prev_item = NULL;\
	while (item != NULL) {\
		if (item->data_type == arg ## _Type) {\
			Array_Data arr = { 0 };\
			memcpy(&arr, item->data, sizeof(arr));\
\
			*number = arr.length;\
			*output = arr.ptr;\
\
			if (prev_item != NULL) {\
				prev_item->pNextItem = item->pNextItem;\
				if (prev_item->pNextItem == NULL) {\
					pRecv_Data_FIFO_Tail = prev_item;\
				}\
			}\
			else {\
				pRecv_Data_FIFO_Head = item->pNextItem;\
				if (pRecv_Data_FIFO_Head == NULL) {\
					pRecv_Data_FIFO_Tail = NULL;\
				}\
			}\
\
			release_Recv_mutex();\
\
			free(item->data);\
			free(item);\
			return NO_DCS_ERROR;\
		}\
\
		prev_item = item;\
		item = item->pNextItem;\
	}\
	release_Recv_mutex();\
	return 1;\
}

void Get_DCS_Status_CB(bool bCorr, bool bAnalyzer, int DCS_Cha_Num) {
	Receive_Callbacks local_callbacks = { 0 };
	bool should_store = false;
	get_Callbacks(&local_callbacks, &should_store);

	if (local_callbacks.Get_DCS_Status_CB != NULL) {
		local_callbacks.Get_DCS_Status_CB(bCorr, bAnalyzer, DCS_Cha_Num);
	}

	if (should_store) {
		DCS_Status status = {
			.bCorr = bCorr,
			.bAnalyzer = bAnalyzer,
			.DCS_Cha_Num = DCS_Cha_Num
		};

		Received_Data_Item* data = malloc(sizeof(*data));
		if (data == NULL) {
			return;
		}

		data->data_type = DCS_Status_Type;
		data->data = malloc(sizeof(status));
		if (data->data == NULL) {
			free(data);
			return;
		}

		memcpy(data->data, &status, sizeof(status));

		Enqueue_Recv_FIFO(data);
	}
}

GETTER_FUNCTION(DCS_Status)

void Get_Correlator_Setting_CB(Correlator_Setting* pCorrelator_Setting) {
	Receive_Callbacks local_callbacks = { 0 };
	bool should_store = false;
	get_Callbacks(&local_callbacks, &should_store);

	if (local_callbacks.Get_Correlator_Setting_CB != NULL) {
		local_callbacks.Get_Correlator_Setting_CB(pCorrelator_Setting);
	}

	if (should_store) {
		Received_Data_Item* data = malloc(sizeof(*data));
		if (data == NULL) {
			return;
		}

		data->data_type = Correlator_Setting_Type;
		data->data = malloc(sizeof(*pCorrelator_Setting));
		if (data->data == NULL) {
			free(data);
			return;
		}

		memcpy(data->data, pCorrelator_Setting, sizeof(*pCorrelator_Setting));

		Enqueue_Recv_FIFO(data);
	}
}

GETTER_FUNCTION(Correlator_Setting)

void Get_Analyzer_Setting_CB(Analyzer_Setting* pAnalyzer_Setting, int Cha_Num) {
	Receive_Callbacks local_callbacks = { 0 };
	bool should_store = false;
	get_Callbacks(&local_callbacks, &should_store);

	if (local_callbacks.Get_Analyzer_Setting_CB != NULL) {
		local_callbacks.Get_Analyzer_Setting_CB(pAnalyzer_Setting, Cha_Num);
	}

	if (should_store) {
		Received_Data_Item* data = malloc(sizeof(*data));
		if (data == NULL) {
			return;
		}

		data->data_type = Analyzer_Setting_Type;
		Array_Data* arr = malloc(sizeof(*arr));
		if (arr == NULL) {
			free(data);
			return;
		}

		arr->length = Cha_Num;

		const size_t dataSize = sizeof(*pAnalyzer_Setting) * Cha_Num;
		arr->ptr = malloc(dataSize);
		if (arr->ptr == NULL) {
			free(data);
			free(arr);
			return;
		}

		memcpy(arr->ptr, pAnalyzer_Setting, dataSize);

		data->data = arr;

		Enqueue_Recv_FIFO(data);
	}
}

ARRAY_GETTER_FUNCTION(Analyzer_Setting)

void Get_Analyzer_Prefit_Param_CB(Analyzer_Prefit_Param* pAnalyzer_Prefit) {
	Receive_Callbacks local_callbacks = { 0 };
	bool should_store = false;
	get_Callbacks(&local_callbacks, &should_store);

	if (local_callbacks.Get_Analyzer_Prefit_Param_CB != NULL) {
		local_callbacks.Get_Analyzer_Prefit_Param_CB(pAnalyzer_Prefit);
	}

	if (should_store) {
		Received_Data_Item* data = malloc(sizeof(*data));
		if (data == NULL) {
			return;
		}

		data->data_type = Analyzer_Prefit_Param_Type;
		data->data = malloc(sizeof(*pAnalyzer_Prefit));
		if (data->data == NULL) {
			free(data);
			return;
		}

		memcpy(data->data, pAnalyzer_Prefit, sizeof(*pAnalyzer_Prefit));

		Enqueue_Recv_FIFO(data);
	}
}

GETTER_FUNCTION(Analyzer_Prefit_Param)

void Get_Simulated_Correlation_CB(Simulated_Correlation* Simulated_Corr) {
	Receive_Callbacks local_callbacks = { 0 };
	bool should_store = false;
	get_Callbacks(&local_callbacks, &should_store);

	if (local_callbacks.Get_Simulated_Correlation_CB != NULL) {
		local_callbacks.Get_Simulated_Correlation_CB(Simulated_Corr);
	}

	if (should_store) {
		Received_Data_Item* data = malloc(sizeof(*data));
		if (data == NULL) {
			return;
		}

		data->data_type = Simulated_Correlation_Type;
		data->data = malloc(sizeof(*Simulated_Corr));
		if (data->data == NULL) {
			free(data);
			return;
		}

		memcpy(data->data, Simulated_Corr, sizeof(*Simulated_Corr));

		Enqueue_Recv_FIFO(data);
	}
}

GETTER_FUNCTION(Simulated_Correlation)

void Get_BFI_Data(BFI_Data* pBFI_Data, int Cha_Num) {
	Receive_Callbacks local_callbacks = { 0 };
	bool should_store = false;
	get_Callbacks(&local_callbacks, &should_store);

	if (local_callbacks.Get_BFI_Data != NULL) {
		local_callbacks.Get_BFI_Data(pBFI_Data, Cha_Num);
	}

	if (should_store) {
		Received_Data_Item* data = malloc(sizeof(*data));
		if (data == NULL) {
			return;
		}

		data->data_type = BFI_Data_Type;
		Array_Data* arr = malloc(sizeof(*arr));
		if (arr == NULL) {
			free(data);
			return;
		}

		arr->length = Cha_Num;

		const size_t dataSize = sizeof(*pBFI_Data) * Cha_Num;
		arr->ptr = malloc(dataSize);
		if (arr->ptr == NULL) {
			free(data);
			free(arr);
			return;
		}

		memcpy(arr->ptr, pBFI_Data, dataSize);

		data->data = arr;

		Enqueue_Recv_FIFO(data);
	}
}

ARRAY_GETTER_FUNCTION(BFI_Data)

void Get_Error_Message_CB(Error_Message* pMessage, unsigned __int32 Size) {
	Receive_Callbacks local_callbacks = { 0 };
	bool should_store = false;
	get_Callbacks(&local_callbacks, &should_store);

	if (local_callbacks.Get_Error_Message_CB != NULL) {
		local_callbacks.Get_Error_Message_CB(pMessage, Size);
	}

	if (should_store) {
		Received_Data_Item* data = malloc(sizeof(*data));
		if (data == NULL) {
			return;
		}

		data->data_type = Error_Message_Type;
		Array_Data* arr = malloc(sizeof(*arr));
		if (arr == NULL) {
			free(data);
			return;
		}

		arr->length = Size;

		const size_t dataSize = sizeof(*pMessage) * Size;
		arr->ptr = malloc(dataSize);
		if (arr->ptr == NULL) {
			free(data);
			free(arr);
			return;
		}

		memcpy(arr->ptr, pMessage, dataSize);

		data->data = arr;

		Enqueue_Recv_FIFO(data);
	}
}

ARRAY_GETTER_FUNCTION(Error_Message)

void Get_Error_Code_CB(unsigned __int32 code) {
	Receive_Callbacks local_callbacks = { 0 };
	bool should_store = false;
	get_Callbacks(&local_callbacks, &should_store);

	if (local_callbacks.Get_Error_Code_CB != NULL) {
		local_callbacks.Get_Error_Code_CB(code);
	}
}

void Get_BFI_Corr_Ready_CB(bool bReady) {
	Receive_Callbacks local_callbacks = { 0 };
	bool should_store = false;
	get_Callbacks(&local_callbacks, &should_store);

	if (local_callbacks.Get_BFI_Corr_Ready_CB != NULL) {
		local_callbacks.Get_BFI_Corr_Ready_CB(bReady);
	}
}

void Get_Corr_Intensity_Data_CB(Corr_Intensity_Data* pCorr_Intensity_Data, int Cha_Num, float* pDelayBuf, int Delay_Num) {
	Receive_Callbacks local_callbacks = { 0 };
	bool should_store = false;
	get_Callbacks(&local_callbacks, &should_store);

	if (local_callbacks.Get_Corr_Intensity_Data_CB != NULL) {
		local_callbacks.Get_Corr_Intensity_Data_CB(pCorr_Intensity_Data, Cha_Num, pDelayBuf, Delay_Num);
	}

	if (should_store) {
		Received_Data_Item* data = malloc(sizeof(*data));
		if (data == NULL) {
			return;
		}

		data->data_type = Corr_Intensity_Data_Type;
		Array_Data* arr = malloc(sizeof(*arr) * 2);
		if (arr == NULL) {
			free(data);
			return;
		}

		arr[0].length = Cha_Num;
		arr[1].length = Delay_Num;

		const size_t corrDataSize = sizeof(*pCorr_Intensity_Data) * Cha_Num;
		arr[0].ptr = malloc(corrDataSize);
		if (arr[0].ptr == NULL) {
			free(data);
			free(arr);
			return;
		}

		//Make copy of pCorr_Intensity_Data
		Corr_Intensity_Data* pCorr_Intensity_Data_Copy = malloc(corrDataSize);
		if (pCorr_Intensity_Data_Copy == NULL) {
			free(data);
			free(arr[0].ptr);
			free(arr);
			return;
		}
		memcpy(pCorr_Intensity_Data_Copy, pCorr_Intensity_Data, corrDataSize);

		for (__int32 x = 0; x < Cha_Num; x++) {
#pragma warning (disable: 6385 6386)
			const size_t listSize = sizeof(*(pCorr_Intensity_Data_Copy[x].pCorrBuf)) * pCorr_Intensity_Data_Copy[x].Data_Num;
			pCorr_Intensity_Data_Copy[x].pCorrBuf = malloc(listSize);
			if (pCorr_Intensity_Data_Copy[x].pCorrBuf == NULL) {
				free(data);
				free(arr[0].ptr);
				free(arr);

				for (__int32 y = 0; y < x; y++) {
					free(pCorr_Intensity_Data_Copy[y].pCorrBuf);
				}
				free(pCorr_Intensity_Data_Copy);
				return;
			}

			memcpy(pCorr_Intensity_Data_Copy[x].pCorrBuf, pCorr_Intensity_Data[x].pCorrBuf, listSize);
#pragma warning (default: 6385 6386)
		}

		memcpy(arr[0].ptr, pCorr_Intensity_Data_Copy, corrDataSize);

		const size_t delayDataSize = sizeof(*pDelayBuf) * Delay_Num;
		arr[1].ptr = malloc(delayDataSize);
		if (arr[1].ptr == NULL) {
			free(data);
			free(arr[0].ptr);
			free(arr);

			for (__int32 x = 0; x < Cha_Num; x++) {
#pragma warning (disable: 6001)
				free(pCorr_Intensity_Data_Copy[x].pCorrBuf);
#pragma warning (default: 6001)
			}
			free(pCorr_Intensity_Data_Copy);
			return;
		}

		free(pCorr_Intensity_Data_Copy);

		memcpy(arr[1].ptr, pDelayBuf, delayDataSize);

		data->data = arr;

		Enqueue_Recv_FIFO(data);
	}
}

int Get_Corr_Intensity_Data_Data(Corr_Intensity_Data** output, int* number, float** pDelayBufOutput, int* Delay_Num_Output) {
	set_Recv_mutex();
	Received_Data_Item* item = pRecv_Data_FIFO_Head;
	Received_Data_Item* prev_item = NULL;
	while (item != NULL) {
		if (item->data_type == Corr_Intensity_Data_Type) {
			Array_Data arr[2] = { 0 };
			memcpy(arr, item->data, sizeof(*arr) * 2);

			*number = arr[0].length;
			*output = arr[0].ptr;

			*Delay_Num_Output = arr[1].length;
			*pDelayBufOutput = arr[1].ptr;

			if (prev_item != NULL) {
				prev_item->pNextItem = item->pNextItem;
				if (prev_item->pNextItem == NULL) {
					pRecv_Data_FIFO_Tail = prev_item;
				}
			}
			else {
				pRecv_Data_FIFO_Head = item->pNextItem;
				if (pRecv_Data_FIFO_Head == NULL) {
					pRecv_Data_FIFO_Tail = NULL;
				}
			}

			release_Recv_mutex();

			free(item->data);
			free(item);
			return NO_DCS_ERROR;
		}

		prev_item = item;
		item = item->pNextItem;
	}
	release_Recv_mutex();
	return 1;
}

void Get_Intensity_Data_CB(Intensity_Data* pIntensity_Data, int Cha_Num) {
	Receive_Callbacks local_callbacks = { 0 };
	bool should_store = false;
	get_Callbacks(&local_callbacks, &should_store);

	if (local_callbacks.Get_Intensity_Data_CB != NULL) {
		local_callbacks.Get_Intensity_Data_CB(pIntensity_Data, Cha_Num);
	}

	if (should_store) {
		Received_Data_Item* data = malloc(sizeof(*data));
		if (data == NULL) {
			return;
		}

		data->data_type = Intensity_Data_Type;
		Array_Data* arr = malloc(sizeof(*arr));
		if (arr == NULL) {
			free(data);
			return;
		}

		arr->length = Cha_Num;

		const size_t dataSize = sizeof(*pIntensity_Data) * Cha_Num;
		arr->ptr = malloc(dataSize);
		if (arr->ptr == NULL) {
			free(data);
			free(arr);
			return;
		}

		memcpy(arr->ptr, pIntensity_Data, dataSize);

		data->data = arr;

		Enqueue_Recv_FIFO(data);
	}
}

ARRAY_GETTER_FUNCTION(Intensity_Data)