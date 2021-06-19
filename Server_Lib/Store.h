#pragma once
#include <stdbool.h>

#include "Internal.h"

//Initializes the handle for hStoreMutex.
int init_Store(void);
//Releases the handle for hStoreMutex.
int close_Store(void);

typedef struct {
	bool bCorr;
	bool bAnalyzer;
	int DCS_Cha_Num;
} DCS_Status;

typedef struct {
	bool bCorr;
	bool bAnalyzer;
} Measurement_Output;

typedef struct {
	bool measurement_going;
	int interval;
	int Cha_Num;
	int ids[2];
} Measurement_Status;

int Set_Correlator_Setting_Data(Correlator_Setting input);
int Set_Analyzer_Setting_Data(Analyzer_Setting* pAnalyzer_Setting, int Cha_Num);
int Set_Analyzer_Prefit_Param_Data(Analyzer_Prefit_Param input);
int Set_Optical_Param_Data(Optical_Param_Type* input, int Cha_Num);
int Set_Measurement_Output_Data(bool bCorr, bool bAnalyzer);
int Start_Measurement(int interval, int Cha_Num, int* ids);
int Stop_Measurement(void);

/// <summary>
/// Adds a message to the logs linked list, which can be accessed through Get_Logs.
/// </summary>
/// <param name="log">The string to log.</param>
/// <returns>Standard DCS return code.</returns>
int Add_Log(const char* log);
int Cleanup_Logs(void);

__declspec(dllexport) int Get_DCS_Status_Data(DCS_Status* output);
__declspec(dllexport) int Get_Correlator_Setting_Data(Correlator_Setting* output);
__declspec(dllexport) int Get_Analyzer_Setting_Data(Analyzer_Setting** pAnalyzer_Setting, int* Cha_Num);
__declspec(dllexport) int Get_Analyzer_Prefit_Param_Data(Analyzer_Prefit_Param* output);
__declspec(dllexport) int Get_Measurement_Output_Data(bool* bCorr, bool* bAnalyzer);
__declspec(dllexport) int Get_Logs(char** pMessage, unsigned __int32* length);
int Get_Measurement_Status(Measurement_Status* status);