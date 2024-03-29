#define _CRTDBG_MAP_ALLOC
#define _WINSOCKAPI_
#include <stdlib.h>
#include <crtdbg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "DCS_Driver.h"
#include "COM_Task.h"

#define DEFAULT_PORT "50000"
#define HOST_NAME "localhost"

#define TEST_ARRAY_LEN 6
#define FUNC_TO_TEST 2

void Get_DCS_Status_CB(bool bCorr, bool bAnalyzer, int DCS_Cha_Num) {
	printf("DCS Status:\n");
	printf("%s\n", bCorr ? "true" : "false");
	printf("%s\n", bAnalyzer ? "true" : "false");
	printf("%d\n", DCS_Cha_Num);
}

void Get_Correlator_Setting_CB(Correlator_Setting* pCorrelator_Setting) {
	printf("Correlator Setting:\n");
	printf("%f\n%d\n%d\n", pCorrelator_Setting->Corr_Time, pCorrelator_Setting->Data_N, pCorrelator_Setting->Scale);
}

void Get_Analyzer_Setting_CB(Analyzer_Setting* pAnalyzer_Setting, int Cha_Num) {
	printf("Analyzer Settings:\n");
	for (int x = 0; x < Cha_Num; x++) {
		printf("Setting #%d\n", x);
		printf("Alpha: %f\n", pAnalyzer_Setting[x].Alpha);
		printf("Beta: %f\n", pAnalyzer_Setting[x].Beta);
		printf("Db: %f\n", pAnalyzer_Setting[x].Db);
		printf("Distance: %f\n", pAnalyzer_Setting[x].Distance);
		printf("mua0: %f\n", pAnalyzer_Setting[x].mua0);
		printf("musp: %f\n", pAnalyzer_Setting[x].musp);
		printf("Wavelength: %f\n\n", pAnalyzer_Setting[x].Wavelength);
	}
}

void Get_Analyzer_Prefit_Param_CB(Analyzer_Prefit_Param* pAnalyzer_Prefit) {
	printf("Analyzer Prefit Params:\n");
	printf("Precut: %d\n", pAnalyzer_Prefit->Precut);
	printf("PostCut: %d\n", pAnalyzer_Prefit->PostCut);
	printf("Min_Intensity: %f\n", pAnalyzer_Prefit->Min_Intensity);
	printf("Max_Intensity: %f\n", pAnalyzer_Prefit->Max_Intensity);
	printf("FitLimt: %f\n", pAnalyzer_Prefit->FitLimt);
	printf("lightLeakage: %f\n", pAnalyzer_Prefit->lightLeakage);
	printf("earlyLeakage: %f\n", pAnalyzer_Prefit->earlyLeakage);
	printf("Model: %s\n", pAnalyzer_Prefit->Model ? "true" : "false");
}

void Get_Simulated_Correlation_CB(Simulated_Correlation* Simulated_Corr) {
	printf("Simulated Correlation:\n");
	printf("Precut: %d\n", Simulated_Corr->Precut);
	printf("Cha_ID: %d\n", Simulated_Corr->Cha_ID);
	printf("Data_Num: %d\n", Simulated_Corr->Data_Num);

	printf("Values: [\n");
	for (int x = 0; x < Simulated_Corr->Data_Num; x++) {
		printf("\t%f\n", Simulated_Corr->pCorrBuf[x]);
	}
	printf("]\n");
}

void Get_BFI_Data(BFI_Data* pBFI_Data, int Cha_Num) {
	printf("BFI Data:\n");
	for (int x = 0; x < Cha_Num; x++) {
		printf("Settings %d\n", x);
		printf("BFI: %f\n", pBFI_Data[x].BFI);
		printf("Beta: %f\n", pBFI_Data[x].Beta);
		printf("rMSE: %f\n", pBFI_Data[x].rMSE);
	}
}

void Get_Error_Message_CB(char* pMessage, unsigned __int32 Size) {
	unsigned __int32 strSize = Size + 1;
	char* errorString = calloc(strSize, sizeof(char));
	if (errorString == NULL) {
		return;
	}

	memcpy(errorString, pMessage, Size);

	printf("%s", errorString);

	free(errorString);
}

void Get_BFI_Corr_Ready_CB(bool bReady) {
	printf("BFI Corr Ready: ");
	printf("%s\n", bReady ? "true" : "false");
}

void Get_Corr_Intensity_Data_CB(Corr_Intensity_Data* pCorr_Intensity_Data, int Cha_Num, float* pDelayBuf, int Delay_Num) {
	printf("Corr Intensity Data:\n");
	for (int x = 0; x < Cha_Num; x++) {
		printf("Data %d:\n", pCorr_Intensity_Data[x].Cha_ID);
		printf("\tIntensity: %f\n", pCorr_Intensity_Data[x].intensity);
		printf("\tCorrelation:\n");
		for (int y = 0; y < pCorr_Intensity_Data[x].Data_Num; y++) {
			printf("\t\t%f\n", pCorr_Intensity_Data[x].pCorrBuf[y]);
		}
	}

	printf("Delays: [");
	for (int x = 0; x < Delay_Num; x++) {
		printf("%f, ", pDelayBuf[x]);
	}
	printf("\b\b]\n");
}

void Get_Intensity_Data_CB(Intensity_Data* pIntensity_Data, int Cha_Num) {
	printf("Intensity Data:\n");
	for (int x = 0; x < Cha_Num; x++) {
		printf("Channel %d:\n", pIntensity_Data[x].Cha_ID);
		printf("\tIntensity: %f\n", pIntensity_Data[x].intensity);
	}
}

void Get_Error_Code_CB(unsigned __int32 code) {
	printf("Error code: %u\n", code);
}

int main(void) {
	//Needed to detect and output memory leaks in debug mode.
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	int result = 0;

	DCS_Address address = {
			.address = HOST_NAME,
			.port = DEFAULT_PORT,
	};
	Receive_Callbacks callbacks = {
		.Get_BFI_Data = Get_BFI_Data,
		.Get_DCS_Status_CB = Get_DCS_Status_CB,
		.Get_Correlator_Setting_CB = Get_Correlator_Setting_CB,
		.Get_Analyzer_Setting_CB = Get_Analyzer_Setting_CB,
		.Get_Analyzer_Prefit_Param_CB = Get_Analyzer_Prefit_Param_CB,
		.Get_Simulated_Correlation_CB = Get_Simulated_Correlation_CB,
		.Get_BFI_Data = Get_BFI_Data,
		.Get_Error_Message_CB = Get_Error_Message_CB,
		.Get_BFI_Corr_Ready_CB = Get_BFI_Corr_Ready_CB,
		.Get_Corr_Intensity_Data_CB = Get_Corr_Intensity_Data_CB,
		.Get_Intensity_Data_CB = Get_Intensity_Data_CB,
		.Get_Error_Code_CB = Get_Error_Code_CB,
	};
	result = Initialize_COM_Task(address, callbacks, true);
	if (result != NO_DCS_ERROR) {
		return result;
	}

#if FUNC_TO_TEST == 0
	result = Get_DCS_Status();
#endif // 0

#if FUNC_TO_TEST == 1
	Correlator_Setting correlator_settings = {
		.Corr_Time = 5.5,
		.Data_N = 4,
		.Scale = 10,
	};
	result = Set_Correlator_Setting(&correlator_settings);
#endif // 1

#if FUNC_TO_TEST == 2
	result = Get_Correlator_Setting();
#endif // 2

#if FUNC_TO_TEST == 3
	Analyzer_Setting analyzer_settings_arr[TEST_ARRAY_LEN];
	for (int x = 0; x < TEST_ARRAY_LEN; x++) {
		analyzer_settings_arr[x] = (Analyzer_Setting) {
			.Alpha = 0.00001f,
			.Beta = 0.5f,
			.Db = 0.0f,
			.Distance = 2.0f,
			.mua0 = 0.1f,
			.musp = 17.0f,
			.Wavelength = 783.0f,
		};
	}
	result = Set_Analyzer_Setting(analyzer_settings_arr, TEST_ARRAY_LEN);
#endif // 3

#if FUNC_TO_TEST == 4
	result = Get_Analyzer_Setting();
#endif // 4

#if FUNC_TO_TEST == 5
	result = Enable_DCS(true, false);

	int ids[] = { 1, 2,};
	result = Start_DCS_Measurement(5, ids, sizeof(ids) / sizeof(ids[0]));
	Sleep(5000);
	result = Stop_DCS_Measurement();
#endif // 5


#if FUNC_TO_TEST == 6
	result = Get_Simulated_Correlation();
#endif // 8

#if FUNC_TO_TEST == 7
	Optical_Param_Type optical_arr[TEST_ARRAY_LEN];
	for (int x = 0; x < TEST_ARRAY_LEN; x++) {
		optical_arr[x] = (Optical_Param_Type){
			.Cha_ID = x,
			.mua0 = (float) (6.3 - x / 100),
			.musp = (float) (5.8 - x / 100),
		};
	}
	result = Set_Optical_Param(optical_arr, TEST_ARRAY_LEN);
#endif // 9

#if FUNC_TO_TEST == 8
	Analyzer_Prefit_Param prefit = (Analyzer_Prefit_Param){
		.Precut = 5,
		.PostCut = 10,
		.Min_Intensity = 80.0,
		.Max_Intensity = 3.14159f,
		.FitLimt = 4.5f,
		.lightLeakage = 6.5f,
		.earlyLeakage = 5.5f,
		.Model = true,
	};
	result = Set_Analyzer_Prefit_Param(&prefit);
#endif // 10

#if FUNC_TO_TEST == 9
	result = Get_Analyzer_Prefit_Param();
#endif // 11

	//Sleep to give time for COM task to receive data and call callbacks.
	Sleep(3000);

	Error_Message** corr = calloc(sizeof * corr, 1);
	if (corr == NULL) {
		Destroy_COM_Task();
		return MEMORY_ALLOCATION_ERROR;
	}

	while (1) {
		int num = 0;

		int res = Get_Error_Message_Data(corr, &num);
		//printf("Ret val: %d\n", res);
		if (res != NO_DCS_ERROR) {
			break;
		}

		Get_Error_Message_CB(*corr, num);
		
		free(*corr);
	}

	free(corr);

	//Sleep(35000);

	Destroy_COM_Task();
	return result;
}
