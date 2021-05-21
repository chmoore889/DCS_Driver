#pragma once

#include "Internal.h"
#include "DCS_Driver.h"

#define MAX_COMMAND_RESPONSE_TIME 1000
#define COMMAND_RSP_RESET 0
#define COMMAND_RSP_SET 1
#define COMMAND_RSP_CHECK 2
#define COMMAND_RSP_VALIDATE 3

/// <summary>
/// Adds the data to be transmitted to the transmission FIFO.
/// </summary>
/// <param name="pTransmission">Structure containing the data to send.</param>
/// <returns>Standard DCS status code.</returns>
int Enqueue_Trans_FIFO(Transmission_Data_Type* pTransmission);

/// <summary>
/// Checks, sets, and timesout waiting for an acknowledgement of receiving a DCS command.
/// </summary>
/// <param name="Option">The actions being performed by this call to the function.
/// E.g. set, check, reset, validate.
/// </param>
/// <param name="Command_Code">The command being set or checked.</param>
/// <returns>Standard DCS status code.</returns>
int Check_Command_Response(int Option, Data_ID Command_Code);


/////////////////////////////////////////////////////////////////
//Internal callbacks for functions receiving data from the DCS.//
/////////////////////////////////////////////////////////////////

void Get_DCS_Status_CB(bool bCorr, bool bAnalyzer, int DCS_Cha_Num);
void Get_Correlator_Setting_CB(Correlator_Setting* pCorrelator_Setting);
void Get_Analyzer_Setting_CB(Analyzer_Setting* pAnalyzer_Setting, int Cha_Num);
void Get_Analyzer_Prefit_Param_CB(Analyzer_Prefit_Param* pAnalyzer_Prefit);
void Get_Simulated_Correlation_CB(Simulated_Correlation* Simulated_Corr);
void Get_BFI_Data(BFI_Data* pBFI_Data, int Cha_Num);
void Get_Error_Message_CB(char* pMessage, unsigned __int32 Size);
void Get_BFI_Corr_Ready_CB(bool bReady);
void Get_Corr_Intensity_Data_CB(Corr_Intensity_Data* pCorr_Intensity_Data, int Cha_Num, float* pDelayBuf, int Delay_Num);

typedef enum {
	DCS_Status_Type,
	Correlator_Setting_Type,
	Analyzer_Prefit_Param_Type,
	Simulated_Correlation_Type,
	Corr_Intensity_Data_Type,
	After_This_Are_Arrays,//Items after/greater than this one use `Array_Data` as their `data`.
	Analyzer_Setting_Type,
	BFI_Data_Type,
} Data_Item_Type;

typedef struct Received_Data_Item {
	void* data;
	Data_Item_Type data_type;
	struct Received_Data_Item* pNextItem;
} Received_Data_Item;

typedef struct {
	bool bCorr;
	bool bAnalyzer;
	int DCS_Cha_Num;
} DCS_Status;

typedef struct {
	void* ptr;
	int length;
} Array_Data;

__declspec(dllexport) int Get_DCS_Status_Data(DCS_Status* output);
__declspec(dllexport) int Get_Correlator_Setting_Data(Correlator_Setting* output);
__declspec(dllexport) int Get_Analyzer_Setting_Data(Analyzer_Setting** pAnalyzer_Setting, int* Cha_Num);
__declspec(dllexport) int Get_Analyzer_Prefit_Param_Data(Analyzer_Prefit_Param* output);
__declspec(dllexport) int Get_Simulated_Correlation_Data(Simulated_Correlation* output);
__declspec(dllexport) int Get_BFI_Data_Data(BFI_Data** pBFI_Data, int* Cha_Num);
