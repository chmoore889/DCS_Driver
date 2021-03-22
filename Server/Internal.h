#pragma once

#include <stdlib.h>
#include <stdbool.h>

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

#define Swap16(data) \
( (((data) >> 8) & 0x00FF) | (((data) << 8) & 0xFF00) ) 

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
#define CHECK_NET_CONNECTION 254
#define GET_ERROR_MESSAGE 254
#define STOP_DCS 255
#define COMMAND_ACK 255

//Type Ids
#define COMMAND_ID 0x00000000
#define DATA_ID 0x00000001

#define FRAME_VERSION 0xFF01

//Error Codes
#define NO_DCS_ERROR 0
#define FRAME_CHECKSUM_ERROR -1
#define FRAME_VERSION_ERROR -2
#define FRAME_INVALID_DATA -3
#define FRAME_DATA_CORRUPTION -4
#define MEMORY_ALLOCATION_ERROR -5
#define NETWORK_NOT_READY -6

#define HEADER_SIZE 2
#define TYPE_ID_SIZE 4
#define DATA_ID_SIZE 4
#define CHECKSUM_SIZE 1

typedef unsigned __int32 Data_ID;

typedef struct {
	int Data_N; //data number for correlation computation
	int Scale; //determine the number of correlation values (8*Scale)
	float Corr_Time; //The duration for the correlation
} Correlator_Setting;

typedef struct {
	float Alpha; // error threshold for fitting
	float Distance; // source detector separation in cm
	float Wavelength; // laser wavelength in nm
	float mua0; // light absorption coefficient (1/cm)
	float musp; // light scattering coefficient (1/cm)
	float Db; // initial Db value 5X10 -9
	float Beta; // initial Beta value 0.5
} Analyzer_Setting;

typedef struct {
	int Precut; // Start index of raw correlation
	int PostCut; // End index of raw correlation
	float Min_Intensity; // minimum intensity
	float Max_Intensity; // maximum intensity
	float FitLimt; // lower threshold to redefine the end
				   // index of the raw correlation
	float lightLeakage; // The last 10 correlation values must
						// be over this threshold
	float earlyLeakage; // The first 5 correlation values must
						// be below this threshold
	bool Model; // selection of DCS model, FALSE: semi-infinite
				// TRUE: infinite
} Analyzer_Prefit_Param;

typedef struct {
	int Precut; // Index corresponds to the correlation
				// delay for the first fitted
				// correlation value
	int Cha_ID; // Channel ID of the fitted correlation
	int Data_Num; // number of correlation values
	float* pCorrBuf; // Array of single precision values for the
					 // fitted correlation
} Simulated_Correlation;

typedef struct {
	int Cha_ID; // channel ID
	float mua0; // light absorption coefficient (1/cm)
	float musp; // light scattering coefficient (1/cm)
} Optical_Param_Type;

//Processes received response passed as received_data
//Outputs the data to respond with in internally allocated buffer to_send_data
//Outputs size of to_send_data in to_send_data_size
//Returns a DCS error code
int process_recv(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size);

unsigned __int8 compute_checksum(char* pDataBuf, unsigned int size);
bool check_checksum(char* pDataBuf, size_t size);