#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdbool.h>
#include <stdio.h>

#include "DCS_Driver.h"

#define TEST_ARRAY_LEN 5

#define FUNC_TO_TEST 11

void Get_DCS_Status_CB(bool bCorr, bool bAnalyzer, int DCS_Cha_Num) {
	printf("%s\n", bCorr ? "true" : "false");
	printf("%s\n", bAnalyzer ? "true" : "false");
	printf("%d\n", DCS_Cha_Num);
}

void Get_Correlator_Setting_CB(Correlator_Setting_Type* pCorrelator_Setting) {
	printf("%f\n%d\n%d\n", pCorrelator_Setting->Corr_Time, pCorrelator_Setting->Data_N, pCorrelator_Setting->Scale);
}

void Get_Analyzer_Setting_CB(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num) {
	for (int x = 0; x < Cha_Num; x++) {
		printf("Settings %d\n", x);
		printf("Alpha: %f\n", pAnalyzer_Setting[x].Alpha);
		printf("Beta: %f\n", pAnalyzer_Setting[x].Beta);
		printf("Db: %f\n", pAnalyzer_Setting[x].Db);
		printf("Distance: %f\n", pAnalyzer_Setting[x].Distance);
		printf("mua0: %f\n", pAnalyzer_Setting[x].mua0);
		printf("musp: %f\n", pAnalyzer_Setting[x].musp);
		printf("Wavelength: %f\n\n", pAnalyzer_Setting[x].Wavelength);
	}
}

void Get_Analyzer_Prefit_Param_CB(Analyzer_Prefit_Param_Type* pAnalyzer_Prefit) {
	printf("Precut: %d\n", pAnalyzer_Prefit->Precut);
	printf("PostCut: %d\n", pAnalyzer_Prefit->PostCut);
	printf("Min_Intensity: %f\n", pAnalyzer_Prefit->Min_Intensity);
	printf("Max_Intensity: %f\n", pAnalyzer_Prefit->Max_Intensity);
	printf("FitLimt: %f\n", pAnalyzer_Prefit->FitLimt);
	printf("lightLeakage: %f\n", pAnalyzer_Prefit->lightLeakage);
	printf("earlyLeakage: %f\n", pAnalyzer_Prefit->earlyLeakage);
	printf("Model: %s\n", pAnalyzer_Prefit->Model ? "true" : "false");
}

void Get_Simulated_Correlation_CB(Simulated_Corr_Type* Simulated_Corr) {
	printf("Precut: %d\n", Simulated_Corr->Precut);
	printf("Cha_ID: %d\n", Simulated_Corr->Cha_ID);
	printf("Data_Num: %d\n", Simulated_Corr->Data_Num);

	printf("Values: [\n");
	for (int x = 0; x < Simulated_Corr->Data_Num; x++) {
		printf("\t%f\n", Simulated_Corr->pCorrBuf[x]);
	}
	printf("]\n");
}

int main(void) {
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	int result = 0;

#if FUNC_TO_TEST == 0
	result = Get_DCS_Status();
#endif // 0

#if FUNC_TO_TEST == 1
	Correlator_Setting_Type correlator_settings = {
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
	Analyzer_Setting_Type analyzer_settings_arr[TEST_ARRAY_LEN];
	for (int x = 0; x < TEST_ARRAY_LEN; x++) {
		analyzer_settings_arr[x] = (Analyzer_Setting_Type) {
			.Alpha = 10.1 - x,
			.Beta = 9.2 - x,
			.Db = 8.3 - x,
			.Distance = 7.4 - x,
			.mua0 = 6.6 - x,
			.musp = 5.7 - x,
			.Wavelength = 4.8 - x,
		};
	}
	result = Set_Analyzer_Setting(analyzer_settings_arr, TEST_ARRAY_LEN);
#endif // 3

#if FUNC_TO_TEST == 4
	result = Get_Analyzer_Setting();
#endif // 4

#if FUNC_TO_TEST == 5
	int ids[] = { 1, 5, 7, 9 };
	result = Start_DCS_Measurement(5, ids, sizeof(ids) / sizeof(ids[0]));
#endif // 5

#if FUNC_TO_TEST == 6
	result = Stop_DCS_Measurement();
#endif // 6

#if FUNC_TO_TEST == 7
	result = Enable_DCS(true, false);
#endif // 7

#if FUNC_TO_TEST == 8
	result = Get_Simulated_Correlation();
#endif // 8

#if FUNC_TO_TEST == 9
	Optical_Param_Type optical_arr[TEST_ARRAY_LEN];
	for (int x = 0; x < TEST_ARRAY_LEN; x++) {
		optical_arr[x] = (Optical_Param_Type){
			.Cha_ID = x + 2,
			.mua0 = (float) (6.3 - x),
			.musp = (float) (5.8 - x),
		};
	}
	result = Set_Optical_Param(optical_arr, TEST_ARRAY_LEN);
#endif // 9

#if FUNC_TO_TEST == 10
	Analyzer_Prefit_Param_Type prefit = (Analyzer_Prefit_Param_Type){
		.Precut = 5,
		.PostCut = 10,
		.Min_Intensity = 0.5,
		.Max_Intensity = 3.14159,
		.FitLimt = 4.5,
		.lightLeakage = 6.5,
		.earlyLeakage = 7.5,
		.Model = true,
	};
	result = Set_Analyzer_Prefit_Param(&prefit);
#endif // 10

#if FUNC_TO_TEST == 11
	result = Get_Analyzer_Prefit_Param();
#endif // 11

	return result;
}
