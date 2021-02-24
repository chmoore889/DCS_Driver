#pragma once
#include <WinSock2.h>

enum Endianess
{
	LITTLE_ENDIAN = 1,
	BIG_ENDIAN = 0,
};

//Sets endianess of packaged/received data
//0 is network byte order(big endian)
//1 is little endian
#define ENDIANESS_OUTPUT LITTLE_ENDIAN
#define ENDIANESS_INPUT LITTLE_ENDIAN

//True if the current system is big endian
#define IS_BIG_ENDIAN (!*(unsigned char *)&(unsigned __int16){1})

//Swaps endianess of 16 bit field
#define Swap16(data) \
( (((data) >> 8) & 0x00FF) | (((data) << 8) & 0xFF00) ) 

//Swaps endianess of 32 bit field
#define Swap32(data)   \
( (((data) >> 24) & 0x000000FF) | (((data) >>  8) & 0x0000FF00) | \
  (((data) <<  8) & 0x00FF0000) | (((data) << 24) & 0xFF000000) ) 

//Converts host long to ENDIANESS_OUTPUT endianess.
u_long htool(u_long hlong);

//Converts host short to ENDIANESS_OUTPUT endianess.
u_short htoos(u_short hshort);

//Converts host float to ENDIANESS_OUTPUT endianess.
float htoof(float value);

//Converts ENDIANESS_INPUT endianess long to host endianess.
u_long itohl(u_long ilong);

//Converts ENDIANESS_INPUT endianess short to host endianess.
u_short itohs(u_short ishort);

//Converts ENDIANESS_INPUT endianess float to host endianess.
float itohf(float value);


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
#define GET_ERROR_MESSAGE 254
#define CHECK_NET_CONNECTION 254
#define STOP_DCS 255
#define COMMAND_ACK 255

//Type Ids
#define COMMAND_ID 0x00000000
#define DATA_ID 0x00000001

#define FRAME_VERSION 0xFF01

#define HEADER_SIZE 2
#define TYPE_ID_SIZE 4
#define DATA_ID_SIZE 4
#define CHECKSUM_SIZE 1

//Standard console output colors for creating colored stdout
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


typedef unsigned __int32 Data_ID;

typedef struct Transmission_Data_Type {
	unsigned int size; //Size of the transmission buffer
	char* pFrame; //Pointer to the transmission buffer
	struct Transmission_Data_Type* pNextItem; //Pointer for the FIFO
} Transmission_Data_Type;

//This function is called by the function Get_DCS_Status. It calls the function
//Send_DCS_Command to send the “Get DCS Status” command to the DCS. The Status data will
//be received by the function Receive_DCS_Status.
int Send_Get_DSC_Status(void);
int Receive_DCS_Status(char* pDataBuf);

int Send_Correlator_Setting(Correlator_Setting_Type* pCorrelator_Setting);

//This function is called by the function Get_Correlator_Setting. It calls the function
//Send_DCS_Command to send the “Get Correlator Settings” command to the DCS. The data will
//be received by the function Receive_Correlator_Setting.
int Send_Get_Correlator_Setting(void);
int Receive_Correlator_Setting(char* pDataBuf);

int Send_Analyzer_Setting(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num);

//This function is called by the function Get_Analyzer_Setting. It calls the function
//Send_DCS_Command to send the “Get Analyzer Settings” command to the DCS. The data will
//be received by the function Receive_Analyzer_Setting.
int Send_Get_Analyzer_Setting(void);
int Receive_Analyzer_Setting(char* pDataBuf);

int Send_Start_Measurement(int Interval, int* pCha_IDs, int Cha_Num);

int Send_Stop_Measurement(void);

int Send_Enable_DCS(bool bCorr, bool bAnalyzer);

int Send_Get_Simulated_Correlation(void);
int Receive_Simulated_Correlation(char* pDataBuf);

int Send_Optical_Param(Optical_Param_Type* pOpt_Param, int Cha_Num);

int Send_Analyzer_Prefit_Param(Analyzer_Prefit_Param_Type* pAnalyzer_Prefit_Param);

//This function is called by the function Get_Analyzer_Prefit_Param. It calls the function
//Send_DCS_Command to send the “Get Analyzer Prefit Param” command to the DCS. The data will
//be received by the function Receive_Analyzer_Prefit_Param.
int Send_Get_Analyzer_Prefit_Param(void);
int Receive_Analyzer_Prefit_Param(char* pDataBuf);

int Receive_Error_Message(char* pDataBuf);
int Receive_Command_ACK(char* pDataBuf);
int Receive_BFI_Data(char* pDataBuf);

//This function generates the frame to be sent to the remote DCS.
int Send_DCS_Command(Data_ID data_ID, char* pDataBuf, unsigned int BufferSize);

//Checks given checksum from a full DCS frame
//Returns true if checksum is valid
unsigned __int8 compute_checksum(char* pDataBuf, unsigned int size);
bool check_checksum(char* pDataBuf, size_t size);

//Prints out data at addr in hex format in debug build.
void hexDump(const char* desc, const void* addr, const int len);