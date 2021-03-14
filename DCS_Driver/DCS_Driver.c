#include "DCS_Driver.h"
#include "Internal.h"

__declspec(dllexport) int Get_DCS_Status(void) {
	return Send_Get_DSC_Status();
}

__declspec(dllexport) int Set_Correlator_Setting(Correlator_Setting_Type* pCorr_Setting) {
	return Send_Correlator_Setting(pCorr_Setting);
}

__declspec(dllexport) int Get_Correlator_Setting(void) {
	return Send_Get_Correlator_Setting();
}

__declspec(dllexport) int Set_Analyzer_Setting(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num) {
	return Send_Analyzer_Setting(pAnalyzer_Setting, Cha_Num);
}

__declspec(dllexport) int Get_Analyzer_Setting(void) {
	return Send_Get_Analyzer_Setting();
}

__declspec(dllexport) int Start_DCS_Measurement(int interval, int* pCha_IDs, int Cha_Num) {
	return Send_Start_Measurement(interval, pCha_IDs, Cha_Num);
}

__declspec(dllexport) int Stop_DCS_Measurement(void) {
	return Send_Stop_Measurement();
}

__declspec(dllexport) int Enable_DCS(bool bCorr, bool bAnalyzer) {
	return Send_Enable_DCS(bCorr, bAnalyzer);
}

__declspec(dllexport) int Get_Simulated_Correlation(void) {
	return Send_Get_Simulated_Correlation();
}

__declspec(dllexport) int Set_Optical_Param(Optical_Param_Type* pOpt_Param, int Cha_Num) {
	return Send_Optical_Param(pOpt_Param, Cha_Num);
}

__declspec(dllexport) int Set_Analyzer_Prefit_Param(Analyzer_Prefit_Param_Type* pAnalyzer_Prefit_Param) {
	return Send_Analyzer_Prefit_Param(pAnalyzer_Prefit_Param);
}

__declspec(dllexport) int Get_Analyzer_Prefit_Param(void) {
	return Send_Get_Analyzer_Prefit_Param();
}
