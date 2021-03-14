#pragma once
#include <WinSock2.h>
#include "DCS_Driver.h"

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

//Host to output functions//

//Converts host long to ENDIANESS_OUTPUT endianess.
u_long htool(u_long hlong);

//Converts host short to ENDIANESS_OUTPUT endianess.
u_short htoos(u_short hshort);

//Converts host float to ENDIANESS_OUTPUT endianess.
float htoof(float value);

//Input to host functions//

//Converts ENDIANESS_INPUT endianess long to host endianess.
u_long itohl(u_long ilong);

//Converts ENDIANESS_INPUT endianess short to host endianess.
u_short itohs(u_short ishort);

//Converts ENDIANESS_INPUT endianess float to host endianess.
float itohf(float value);


//Data IDs
//The following are data IDs used in the communication between the
//host and the remote DCS. GET IDs are for asking for data from the DCS and receiving it.
//SET IDs are for setting DCS parameters.
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
#define GET_BFI_DATA 13
#define GET_CORR_INTENSITY 14
#define GET_BFI_CORR_READY 15
#define GET_ERROR_MESSAGE 254
#define CHECK_NET_CONNECTION 254
#define STOP_DCS 255
#define COMMAND_ACK 255

//Data Type Ids
#define COMMAND_ID 0x00000000
#define DATA_ID 0x00000001

#define FRAME_VERSION 0xFF01

//Standard console output colors for creating colored stdout
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

//Frame version of DCS frame is a 16 bit integer.
typedef unsigned __int16 Frame_Version;

//Type id of DCS frame is a 32 bit integer.
typedef unsigned __int32 Type_ID;

//Data id of DCS frame is a 32 bit integer.
typedef unsigned __int32 Data_ID;

//Checksum of DCS frame is an 8 bit integer.
typedef unsigned __int8 Checksum;

typedef struct Transmission_Data_Type {
	unsigned __int32 size; //Size of the transmission buffer
	char* pFrame; //Pointer to the transmission buffer
	struct Transmission_Data_Type* pNextItem; //Pointer to the next item in the queue.
} Transmission_Data_Type;

//This function is called by the function Get_DCS_Status. It calls the function
//Send_DCS_Command to send the “Get DCS Status” command to the DCS. The Status data will
//be received by the function Receive_DCS_Status.
int Send_Get_DSC_Status(void);
int Receive_DCS_Status(char* pDataBuf);

//Sends command to set the passed correlator settings.
int Send_Correlator_Setting(Correlator_Setting_Type* pCorrelator_Setting);

//This function is called by the function Get_Correlator_Setting. It calls the function
//Send_DCS_Command to send the “Get Correlator Settings” command to the DCS. The data will
//be received by the function Receive_Correlator_Setting.
int Send_Get_Correlator_Setting(void);
int Receive_Correlator_Setting(char* pDataBuf);

//Sends command to set the passed analyzer settings.
int Send_Analyzer_Setting(Analyzer_Setting_Type* pAnalyzer_Setting, unsigned __int32 Cha_Num);

//This function is called by the function Get_Analyzer_Setting. It calls the function
//Send_DCS_Command to send the “Get Analyzer Settings” command to the DCS. The data will
//be received by the function Receive_Analyzer_Setting.
int Send_Get_Analyzer_Setting(void);
int Receive_Analyzer_Setting(char* pDataBuf);

//Sends command to start a measurement with the passed parameters.
int Send_Start_Measurement(__int32 Interval, unsigned __int32* pCha_IDs, unsigned __int32 Cha_Num);

//Sends command to start a measurement with the passed parameters.
int Send_Stop_Measurement(void);

//Sends command to enable or disable different outputs of the DCS.
int Send_Enable_DCS(bool bCorr, bool bAnalyzer);


//This function is called by the function Get_Simulated_Correlation. It calls the function
//Send_DCS_Command to send the “Get Simulated Correlation” command to the DCS. The data will
//be received by the function Receive_Simulated_Correlation.
int Send_Get_Simulated_Correlation(void);
int Receive_Simulated_Correlation(char* pDataBuf);

//Sends command to set the passed optical paramters with the given array of [Cha_Num] length.
int Send_Optical_Param(Optical_Param_Type* pOpt_Param, int Cha_Num);

//Sends command to set the passed prefit paramters.
int Send_Analyzer_Prefit_Param(Analyzer_Prefit_Param_Type* pAnalyzer_Prefit_Param);

//This function is called by the function Get_Analyzer_Prefit_Param. It calls the function
//Send_DCS_Command to send the “Get Analyzer Prefit Param” command to the DCS. The data will
//be received by the function Receive_Analyzer_Prefit_Param.
int Send_Get_Analyzer_Prefit_Param(void);
int Receive_Analyzer_Prefit_Param(char* pDataBuf);

//Receives logging messages from the DCS device and calls user-defined callback.
int Receive_Error_Message(char* pDataBuf);

//Handles the acknowledgement frame from the DCS.
int Receive_Command_ACK(char* pDataBuf);

//Processes BFI data and calls user-defined callback with the data.
int Receive_BFI_Data(char* pDataBuf);

//Processes command that alerts client program that the BFI data is ready.
int Receive_BFI_Corr_Ready(char* pDataBuf);

//Processes correlation intensity data and calls user-defined callback with the data.
int Receive_Corr_Intensity_Data(char* pDataBuf);

//This function generates the frame to be sent to the remote DCS. 
int Send_DCS_Command(Data_ID data_ID, char* pDataBuf, const unsigned __int32 BufferSize);

//Computes a checksum from a given DCS frame.
unsigned __int8 compute_checksum(char* pDataBuf, unsigned __int32 size);

//Checks given checksum from a full DCS frame.
//Returns true if checksum is valid. False otherwise.
bool check_checksum(char* pDataBuf, unsigned __int32 size);

//Prints out data at addr in hex format only in debug build. NOP in release.
void hexDump(const char* desc, const void* addr, const unsigned __int32 len);