#pragma once

//Data IDs
/*The following are data IDs used in the communication between the
host and the remote DCS. GET IDs are for the data from the DCS*/
#define GET_DCS_STATUS 0
#define SET_CORRELATOR_SETTING 1
#define GET_CORRELATOR_SETTING 2
#define SET_ANALYZER_SETTING 3
#define GET_ANALYZER_SETTING 4
#define START_MEASUREMENT 5
#define STOP_MEASUREMENT 6
#define ENABLE_CORR_ANALYZER 7
#define GET_SIMULATED_DATA 8
#define SET_ANALYZER_PREFIT_PARAM 9
#define GET_ANALYZER_PREFIT_PARAM 10
#define SET_OPTICAL_PARAM 11
#define CHECK_NET_CONNECTION 254
#define GET_ERROR_MESSAGE 254
#define STOP_DCS 255
#define COMMAND_ACK 255

//Type Ids
#define COMMAND_ID 0x00000000
#define DATA_ID 0x00000001

#define FRAME_VERSION 0xFF01

#define HEADER_SIZE 2
#define TYPE_ID_SIZE 4
#define DATA_ID_SIZE 4

typedef unsigned __int32 Data_ID;

typedef struct Transmission_Data_Type {
	int size; //Size of the transmission buffer
	char* pFrame; //Pointer to the transmission buffer
	struct Transmission_Data_Type* pNextItem; //Pointer for the FIFO
} Transmission_Data_Type;

//This function is called by the function Get_DCS_Status. It calls the function
//Send_DCS_Command to send the “Get DCS Status” command to the DCS. The Status data will
//be received by the function Receive_DCS_Status.
int Send_Get_DSC_Status();
int Receive_DCS_Status(char* pDataBuf);

int Send_Correlator_Setting(Correlator_Setting_Type* pCorrelator_Setting);

//This function is called by the function Get_Correlator_Setting. It calls the function
//Send_DCS_Command to send the “Get Correlator Settings” command to the DCS. The data will
//be received by the function Receive_Correlator_Setting.
int Send_Get_Correlator_Setting();
int Receive_Correlator_Setting(char* pDataBuf);

int Send_Analyzer_Setting(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num);

//This function is called by the function Get_Analyzer_Setting. It calls the function
//Send_DCS_Command to send the “Get Analyzer Settings” command to the DCS. The data will
//be received by the function Receive_Analyzer_Setting.
int Send_Get_Analyzer_Setting();
int Receive_Analyzer_Setting(char* pDataBuf);

int Send_Start_Measurement(int Interval, int* pCha_IDs, int Cha_Num);

int Send_Stop_Measurement();

int Send_Enable_DCS(bool bCorr, bool bAnalyzer);

int Send_Get_Simulated_Correlation();

int Send_Optical_Param(Optical_Param_Type* pOpt_Param, int Cha_Num);

int Send_Analyzer_Prefit_Param(Analyzer_Prefit_Param_Type* pAnalyzer_Prefit_Param);

//This function is called by the function Get_Analyzer_Prefit_Param. It calls the function
//Send_DCS_Command to send the “Get Analyzer Prefit Param” command to the DCS. The data will
//be received by the function Receive_Analyzer_Prefit_Param.
int Send_Get_Analyzer_Prefit_Param();
int Receive_Analyzer_Prefit_Param(char* pDataBuf);

//This function generates the frame to be sent to the remote DCS.
int Send_DCS_Command(Data_ID data_ID, char* pDataBuf, unsigned int BufferSize);

//Checks given checksum from a full DCS frame
//Returns true if checksum is valid
unsigned __int8 compute_checksum(char* pDataBuf, unsigned int size);
bool check_checksum(char* pDataBuf, size_t size);

int send_data_and_handle(Transmission_Data_Type* data_to_send);
int process_recv(char* buff, unsigned int buffLen);