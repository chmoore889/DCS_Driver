#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
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

//Handle of the COM task thread.
static HANDLE threadHandle;
//Handle of the mutex for destroying the COM task.
static HANDLE hRunMutex;
//Handle of the mutex for adding to the FIFO queue.
static HANDLE hFIFOMutex;

//Removes the first item in the FIFO and returns its pointer.
static Transmission_Data_Type* Dequeue_Trans_FIFO(void);

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
//Handle of the mutex for controlling addition to the [callbacks] struct.
static HANDLE hCallbacksMutex;

//Initializes the handle for hCallbacksMutex.
static int init_Callback_mutex(void);
//Releases the handle for hCallbacksMutex.
static int close_Callback_mutex(void);
//Thread-safe method of setting the callbacks variable.
static void set_Callbacks(Receive_Callbacks local_callbacks);
//Thread-safe method of retrieving the callbacks variable.
static Receive_Callbacks get_Callbacks();

__declspec(dllexport) int Initialize_COM_Task(DCS_Address address, Receive_Callbacks local_callbacks) {
	//Set the callback functions for receiving data whether the COM task exists or not.
	//If the callbacks mutex isn't NULL, the COM task is already running so use the thread-safe callback setter.
	if (hCallbacksMutex != NULL) {
		set_Callbacks(local_callbacks);
	}
	//Otherwise, the thread-safe callback setter cannot be used and it's safe to set it directly.
	else {
		callbacks = local_callbacks;
	}

	//Return if a COM task already exists.
	if (threadHandle != NULL || hRunMutex != NULL) {
		return THREAD_ALREADY_EXISTS;
	}

	//Initialize a set mutex for stopping the thread later.
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

	result = init_Callback_mutex();
	if (result != NO_DCS_ERROR) {
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		close_FIFO_mutex();
		return result;
	}

	//Start the COM task thread, calling the COM_Task function.
	threadHandle = (HANDLE)_beginthread(COM_Task, 0, &address);
	if (threadHandle == NULL || PtrToLong(threadHandle) == -1L) {
		CloseHandle(hRunMutex);
		hRunMutex = NULL;
		close_FIFO_mutex();
		close_Callback_mutex();
		threadHandle = NULL;
		return THREAD_START_ERROR;
	}

	return NO_DCS_ERROR;
}

__declspec(dllexport) int Destroy_COM_Task() {
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
		return 1;
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
			printf("Connection closed\n");
		}
		else if (iResult < 0) {
			//Socket should be set to non-blocking. This handles the would block error as success. Fails normally otherwise.
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK) {
				//No error here. No data available so break from loop.
				break;
			}
			else {
				printf("recv failed with error: %d\n", err);
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

//Function run by the COM task thread. Initiates connection to the DCS
//and then continuously sends and receives data until Destroy_COM_Task is called.
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

	//Resolve the server address and port.
	iResult = getaddrinfo(dcs_address->address, dcs_address->port, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		_endthread();
		return;
	}

	//Attempt to connect to an address until one succeeds.
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

	//Make the socket non-blocking for convenience when receiving data in the COM thread.
	u_long iMode = 1;
	iResult = ioctlsocket(ConnectSocket, FIONBIO, &iMode);
	if (iResult != NO_ERROR) {
		printf("ioctlsocket failed with error: %ld\n", iResult);
	}

	//Repeat while RunMutex is still taken by the main thread. Clean up and exit when it's released.
	while (WaitForSingleObject(hRunMutex, 50) == WAIT_TIMEOUT) {
		//If data is waiting in the queue, send it, one at a time.
		Transmission_Data_Type* data_to_send = Dequeue_Trans_FIFO();
		if (data_to_send != NULL) {
			iResult = send_data(ConnectSocket, data_to_send);
			if (iResult < 0) {
				printf(ANSI_COLOR_RED"Sending Error\n"ANSI_COLOR_RESET);
				_endthread();
				return;
			}
		}

		//Received and process data from the DCS.
		iResult = recv_data(ConnectSocket);

		if (iResult > 0) {
			printf(ANSI_COLOR_RED"Fatal Receive Error\n"ANSI_COLOR_RESET);

			closesocket(ConnectSocket);
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

int Command_Rsp_Count;
int Command_Sent;
bool Command_Ack;


//TODO: finish this
int Check_Command_Response(int Option, int Command_Code) {
	if (Option == COMMAND_RSP_RESET) {
		Command_Rsp_Count = MAX_COMMAND_RESPONSE_TIME;
		Command_Ack = true;
		return 0;
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

	default:
		printf(ANSI_COLOR_RED"Invalid Data ID: %d\n"ANSI_COLOR_RESET, data_id);
		err = FRAME_INVALID_DATA;
	}

	free(pDataBuff);

	return err;
}

////////////////
//Callbacks/////
////////////////

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

static void set_Callbacks(Receive_Callbacks local_callbacks) {
	WaitForSingleObject(hCallbacksMutex, INFINITE);
	callbacks = local_callbacks;
	ReleaseMutex(hCallbacksMutex);
}

static Receive_Callbacks get_Callbacks() {
	WaitForSingleObject(hCallbacksMutex, INFINITE);
	Receive_Callbacks local_callbacks = callbacks;
	ReleaseMutex(hCallbacksMutex);

	return local_callbacks;
}

//Each of the following functions are internallly called callbacks. They will call the
//user-defined callback it was passed in Initialize_COM_Task.

void Get_DCS_Status_CB(bool bCorr, bool bAnalyzer, int DCS_Cha_Num) {
	Receive_Callbacks local_callbacks = get_Callbacks();

	if (local_callbacks.Get_DCS_Status_CB != NULL) {
		local_callbacks.Get_DCS_Status_CB(bCorr, bAnalyzer, DCS_Cha_Num);
	}
}

void Get_Correlator_Setting_CB(Correlator_Setting_Type* pCorrelator_Setting) {
	Receive_Callbacks local_callbacks = get_Callbacks();

	if (local_callbacks.Get_Correlator_Setting_CB != NULL) {
		local_callbacks.Get_Correlator_Setting_CB(pCorrelator_Setting);
	}
}

void Get_Analyzer_Setting_CB(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num) {
	Receive_Callbacks local_callbacks = get_Callbacks();

	if (local_callbacks.Get_Analyzer_Setting_CB != NULL) {
		local_callbacks.Get_Analyzer_Setting_CB(pAnalyzer_Setting, Cha_Num);
	}
}

void Get_Analyzer_Prefit_Param_CB(Analyzer_Prefit_Param_Type* pAnalyzer_Prefit) {
	Receive_Callbacks local_callbacks = get_Callbacks();

	if (local_callbacks.Get_Analyzer_Prefit_Param_CB != NULL) {
		local_callbacks.Get_Analyzer_Prefit_Param_CB(pAnalyzer_Prefit);
	}
}

void Get_Simulated_Correlation_CB(Simulated_Corr_Type* Simulated_Corr) {
	Receive_Callbacks local_callbacks = get_Callbacks();

	if (local_callbacks.Get_Simulated_Correlation_CB != NULL) {
		local_callbacks.Get_Simulated_Correlation_CB(Simulated_Corr);
	}
}

void Get_BFI_Data(BFI_Data_Type* pBFI_Data, int Cha_Num) {
	Receive_Callbacks local_callbacks = get_Callbacks();

	if (local_callbacks.Get_BFI_Data != NULL) {
		local_callbacks.Get_BFI_Data(pBFI_Data, Cha_Num);
	}
}

void Get_Error_Message_CB(char* pMessage, unsigned __int32 Size) {
	Receive_Callbacks local_callbacks = get_Callbacks();

	if (local_callbacks.Get_Error_Message_CB != NULL) {
		local_callbacks.Get_Error_Message_CB(pMessage, Size);
	}
}

void Get_BFI_Corr_Ready_CB(bool bReady) {
	Receive_Callbacks local_callbacks = get_Callbacks();

	if (local_callbacks.Get_BFI_Corr_Ready_CB != NULL) {
		local_callbacks.Get_BFI_Corr_Ready_CB(bReady);
	}
}

void Get_Corr_Intensity_Data_CB(Corr_Intensity_Data_Type* pCorr_Intensity_Data, int Cha_Num, float* pDelayBuf, int Delay_Num) {
	Receive_Callbacks local_callbacks = get_Callbacks();

	if (local_callbacks.Get_Corr_Intensity_Data_CB != NULL) {
		local_callbacks.Get_Corr_Intensity_Data_CB(pCorr_Intensity_Data, Cha_Num, pDelayBuf, Delay_Num);
	}
}
