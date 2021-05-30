#include "DCS_Driver.h"
#include "Internal.h"

int Get_DCS_Status(void) {
	return Send_Get_DCS_Status();
}

int Set_Correlator_Setting(Correlator_Setting* pCorr_Setting) {
	return Send_Correlator_Setting(pCorr_Setting);
}

int Get_Correlator_Setting(void) {
	return Send_Get_Correlator_Setting();
}

 int Set_Analyzer_Setting(Analyzer_Setting* pAnalyzer_Setting, int Cha_Num) {
	return Send_Analyzer_Setting(pAnalyzer_Setting, Cha_Num);
}

 int Get_Analyzer_Setting(void) {
	return Send_Get_Analyzer_Setting();
}

 int Start_DCS_Measurement(int interval, int* pCha_IDs, int Cha_Num) {
	return Send_Start_Measurement(interval, pCha_IDs, Cha_Num);
}

 int Stop_DCS_Measurement(void) {
	return Send_Stop_Measurement();
}

 int Enable_DCS(bool bCorr, bool bAnalyzer) {
	return Send_Enable_DCS(bCorr, bAnalyzer);
}

 int Get_Simulated_Correlation(void) {
	return Send_Get_Simulated_Correlation();
}

 int Set_Optical_Param(Optical_Param_Type* pOpt_Param, int Cha_Num) {
	return Send_Optical_Param(pOpt_Param, Cha_Num);
}

 int Set_Analyzer_Prefit_Param(Analyzer_Prefit_Param* pAnalyzer_Prefit_Param) {
	return Send_Analyzer_Prefit_Param(pAnalyzer_Prefit_Param);
}

 int Get_Analyzer_Prefit_Param(void) {
	return Send_Get_Analyzer_Prefit_Param();
}

 Receive_Callbacks Null_Receive_Callbacks(void) {
	Receive_Callbacks callbacks = { 0 };
	return callbacks;
}