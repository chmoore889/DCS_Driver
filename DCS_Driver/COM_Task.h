#pragma once

#include "Internal.h"
#include "DCS_Driver.h"

#define MAX_COMMAND_RESPONSE_TIME 1000
#define COMMAND_RSP_RESET 0
#define COMMAND_RSP_SET 1
#define COMMAND_RSP_CHECK 2
#define COMMAND_VALIDATE 3

//Adds the data to be transmitted to the transmission FIFO.
int Enqueue_Trans_FIFO(Transmission_Data_Type* pTransmission);

int Check_Command_Response(int Option, int Command_Code);

void Get_DCS_Status_CB(bool bCorr, bool bAnalyzer, int DCS_Cha_Num);

void Get_Correlator_Setting_CB(Correlator_Setting_Type* pCorrelator_Setting);

void Get_Analyzer_Setting_CB(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num);

void Get_Analyzer_Prefit_Param_CB(Analyzer_Prefit_Param_Type* pAnalyzer_Prefit);

void Get_Simulated_Correlation_CB(Simulated_Corr_Type* Simulated_Corr);

void Get_BFI_Data(BFI_Data_Type* pBFI_Data, int Cha_Num);

void Get_Error_Message_CB(char* pMessage, unsigned __int32 Size);