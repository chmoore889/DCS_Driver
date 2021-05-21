#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "Internal.h"
#include "COM_Task.h"

int Send_Get_DCS_Status(void) {
	return Send_DCS_Command(GET_DCS_STATUS, NULL, 0);
}

int Receive_DCS_Status(char* pDataBuf) {
	unsigned __int32 index = 0;

	bool bCorr; // TRUE if correlator is started, FALSE if the correlator is not started.
	bool bAnalyzer; // TRUE if analyzer is started, FALSE if the analyzer is not started.
	unsigned __int32 DCS_Cha_Num; // number of total DCS channels on the remote DCS.

	memcpy(&bCorr, &pDataBuf[index], sizeof(bCorr));
	index += sizeof(bCorr);

	memcpy(&bAnalyzer, &pDataBuf[index], sizeof(bAnalyzer));
	index += sizeof(bCorr);

	memcpy(&DCS_Cha_Num, &pDataBuf[index], sizeof(DCS_Cha_Num));
	index += sizeof(bCorr);

	//Change from network to host byte order
	DCS_Cha_Num = itohl(DCS_Cha_Num);

	//Call user-defined callback.
	Get_DCS_Status_CB(bCorr, bAnalyzer, DCS_Cha_Num);

	return NO_DCS_ERROR;
}

int Send_Correlator_Setting(Correlator_Setting* pCorrelator_Setting) {
	char* pDataBuf; //data buffer for the byte stream of the correlator setting data
	const unsigned __int32 BufferSize = sizeof(*pCorrelator_Setting); //data size of the buffer pDataBuf

	//Set the Data_N to either 16384 or 32768.
	if (pCorrelator_Setting->Data_N > 16384) {
		pCorrelator_Setting->Data_N = 32768;
	}
	else {
		pCorrelator_Setting->Data_N = 16384;
	}

	//Confining scale value between 1 and 10
	if (pCorrelator_Setting->Scale > 10) {
		pCorrelator_Setting->Scale = 10;
	}
	else if (pCorrelator_Setting->Scale < 1) {
		pCorrelator_Setting->Scale = 1;
	}

	pDataBuf = malloc(BufferSize);
	if (pDataBuf == NULL) {
		return MEMORY_ALLOCATION_ALIGNMENT;
	}

	//Copy each struct member into data buffer and change them to the output order
	unsigned __int32 Data_N = htool(pCorrelator_Setting->Data_N);
	memcpy(&pDataBuf[0], &Data_N, sizeof(Data_N));

	unsigned __int32 Scale = htool(pCorrelator_Setting->Scale);
	memcpy(&pDataBuf[4], &Scale, sizeof(Scale));

	unsigned __int32 SampleSize = htool((int)ceil(pCorrelator_Setting->Corr_Time / pCorrelator_Setting->Data_N / 200e-9));
	memcpy(&pDataBuf[8], &SampleSize, sizeof(SampleSize));

	int result = Send_DCS_Command(SET_CORRELATOR_SETTING, pDataBuf, BufferSize);
	free(pDataBuf);

	return result;
}

int Send_Get_Correlator_Setting(void) {
	return Send_DCS_Command(GET_CORRELATOR_SETTING, NULL, 0);
}

int Receive_Correlator_Setting(char* pDataBuf) {
	Correlator_Setting* pCorrelator_Setting = malloc(sizeof(Correlator_Setting));
	if (pCorrelator_Setting == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	unsigned __int32 index = 0;//Keeps track of the current pDataBuf index

	//Copy each parameter from the input buffer and increment index.
	unsigned __int32 Data_N;
	unsigned __int32 Scale;
	unsigned __int32 Sample_Size;
	memcpy(&Data_N, &pDataBuf[index], sizeof(Data_N));
	index += sizeof(Data_N);

	memcpy(&Scale, &pDataBuf[index], sizeof(Scale));
	index += sizeof(Scale);

	memcpy(&Sample_Size, &pDataBuf[index], sizeof(Sample_Size));
	index += sizeof(Sample_Size);

	//Change network endianess to host.
	Data_N = itohl(Data_N);
	Scale = itohl(Scale);
	Sample_Size = itohl(Sample_Size);

	//Reverse Corr_Time calculation.
	float Corr_Time = (float)2e-7 * Sample_Size * Data_N;

	//Copy data to the struct and call user-defined callback.
	memcpy(&pCorrelator_Setting->Data_N, &Data_N, sizeof(Data_N));
	memcpy(&pCorrelator_Setting->Scale, &Scale, sizeof(Scale));
	memcpy(&pCorrelator_Setting->Corr_Time, &Corr_Time, sizeof(Corr_Time));

	Get_Correlator_Setting_CB(pCorrelator_Setting);

	//Cleanup dynamically allocated resources.
	free(pCorrelator_Setting);

	return NO_DCS_ERROR;
}

int Send_Analyzer_Setting(Analyzer_Setting* pAnalyzer_Setting, unsigned __int32 Cha_Num) {
	char* pDataBuf; //Data buffer for the byte stream of the analyzer setting data.
	unsigned __int32 index = 0;//Keeps track of the current pDataBuf index
	const unsigned __int32 BufferSize = sizeof(Cha_Num) + Cha_Num * sizeof(*pAnalyzer_Setting);

	pDataBuf = malloc(BufferSize);
	if (pDataBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

#pragma warning (disable: 6386 6385)
	unsigned __int32 network_Cha_Num = htool(Cha_Num);//[Cha_Num] in the output byte order.

	//Copy to the output buffer.
	memcpy(&pDataBuf[index], &network_Cha_Num, sizeof(network_Cha_Num));
	index += sizeof(network_Cha_Num);

	//Change analyzer settings to network byte order

	//Allocate memory for the output byte order struct.
	Analyzer_Setting* network_Analyzer = malloc(Cha_Num * sizeof(Analyzer_Setting));
	if (network_Analyzer == NULL) {
		free(pDataBuf);
		return MEMORY_ALLOCATION_ERROR;
	}

	//Loop over each channel and change each parameter to network byte order.
	for (unsigned __int32 x = 0; x < Cha_Num; x++) {
		network_Analyzer[x].Alpha = htoof(pAnalyzer_Setting[x].Alpha);
		network_Analyzer[x].Beta = htoof(pAnalyzer_Setting[x].Beta);
		network_Analyzer[x].Db = htoof(pAnalyzer_Setting[x].Db);
		network_Analyzer[x].Distance = htoof(pAnalyzer_Setting[x].Distance);
		network_Analyzer[x].mua0 = htoof(pAnalyzer_Setting[x].mua0);
		network_Analyzer[x].musp = htoof(pAnalyzer_Setting[x].musp);
		network_Analyzer[x].Wavelength = htoof(pAnalyzer_Setting[x].Wavelength);
	}

	//Copy array into the output buffer.
	memcpy(&pDataBuf[index], network_Analyzer, Cha_Num * sizeof(*network_Analyzer));
	index += Cha_Num * sizeof(*network_Analyzer);

	free(network_Analyzer);
#pragma warning (default: 6386 6385)

	int result = Send_DCS_Command(SET_ANALYZER_SETTING, pDataBuf, BufferSize);
	free(pDataBuf);

	return result;
}

int Send_Get_Analyzer_Setting(void) {
	return Send_DCS_Command(GET_ANALYZER_SETTING, NULL, 0);
}

int Receive_Analyzer_Setting(char* pDataBuf) {
	unsigned __int32 index = 0;//Keeps track of the current pDataBuf index.

	Analyzer_Setting* pAnalyzer_Setting;
	unsigned __int32 Cha_Num;

	memcpy(&Cha_Num, &pDataBuf[index], sizeof(Cha_Num));
	index += sizeof(Cha_Num);

	//Change to host byte order.
	Cha_Num = itohl(Cha_Num);

	//Allocate memory for the received analyzer settings.
	pAnalyzer_Setting = malloc(Cha_Num * sizeof(*pAnalyzer_Setting));
	if (pAnalyzer_Setting == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

#pragma warning (disable: 6386 6385)
	for (unsigned __int32 x = 0; x < Cha_Num; x++) {
		memcpy(&pAnalyzer_Setting[x], &pDataBuf[index], sizeof(*pAnalyzer_Setting));
		index += sizeof(*pAnalyzer_Setting);

		//Change network endianess to host for each struct member.
		pAnalyzer_Setting[x].Alpha = itohf(pAnalyzer_Setting[x].Alpha);
		pAnalyzer_Setting[x].Beta = itohf(pAnalyzer_Setting[x].Beta);
		pAnalyzer_Setting[x].Db = itohf(pAnalyzer_Setting[x].Db);
		pAnalyzer_Setting[x].Distance = itohf(pAnalyzer_Setting[x].Distance);
		pAnalyzer_Setting[x].mua0 = itohf(pAnalyzer_Setting[x].mua0);
		pAnalyzer_Setting[x].musp = itohf(pAnalyzer_Setting[x].musp);
		pAnalyzer_Setting[x].Wavelength = itohf(pAnalyzer_Setting[x].Wavelength);
	}
#pragma warning (default: 6386 6385)

	Get_Analyzer_Setting_CB(pAnalyzer_Setting, Cha_Num);
	free(pAnalyzer_Setting);

	return NO_DCS_ERROR;
}

int Send_Start_Measurement(__int32 Interval, unsigned __int32* pCha_IDs, unsigned __int32 Cha_Num) {
	char* pDataBuf;//Output buffer.
	unsigned __int32 index = 0;//Keeps track of the current pDataBuf index.
	const unsigned __int32 BufferSize = sizeof(Interval) + sizeof(Cha_Num) + Cha_Num * sizeof(*pCha_IDs);

	//Allocate output buffer.
	pDataBuf = malloc(BufferSize);
	if (pDataBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

#pragma warning (disable: 6386)
	int network_Interval = htool(Interval);
	memcpy(&pDataBuf[index], &network_Interval, sizeof(network_Interval));
	index += sizeof(network_Interval);

	int network_Cha_Num = htool(Cha_Num);
	memcpy(&pDataBuf[index], &network_Cha_Num, sizeof(network_Cha_Num));
	index += sizeof(network_Cha_Num);

	//Allocate output byte order channel IDs array.
	int* network_Cha_IDs = malloc(Cha_Num * sizeof(*network_Cha_IDs));
	if (network_Cha_IDs == NULL) {
		free(pDataBuf);
		return MEMORY_ALLOCATION_ERROR;
	}

	//Copy inputted array to [network_Cha_IDs] and change to output byte order in loop.
	memcpy(network_Cha_IDs, pCha_IDs, Cha_Num * sizeof(*network_Cha_IDs));
	for (unsigned __int32 x = 0; x < Cha_Num; x++) {
		network_Cha_IDs[x] = htool(network_Cha_IDs[x]);
	}

	//Copy output byte order array to final output buffer.
	memcpy(&pDataBuf[index], network_Cha_IDs, Cha_Num * sizeof(*pCha_IDs));
	index += Cha_Num * sizeof(*pCha_IDs);
#pragma warning (default: 6386)
	free(network_Cha_IDs);

	int result = Send_DCS_Command(START_MEASUREMENT, pDataBuf, BufferSize);

	free(pDataBuf);

	return result;
}

int Send_Stop_Measurement(void) {
	return Send_DCS_Command(STOP_MEASUREMENT, NULL, 0);
}

int Send_Enable_DCS(bool bCorr, bool bAnalyzer) {
	const unsigned __int32 BufferSize = sizeof(bCorr) + sizeof(bAnalyzer);

	unsigned __int32 index = 0;//Keeps track of the current pDataBuf index.

	//Allocate final output buffer.
	char* pDataBuf = malloc(BufferSize);
	if (pDataBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(&pDataBuf[index], &bAnalyzer, sizeof(bAnalyzer));
	index += sizeof(bAnalyzer);

	memcpy(&pDataBuf[index], &bCorr, sizeof(bCorr));
	index += sizeof(bCorr);

	int result = Send_DCS_Command(ENABLE_CORR_ANALYZER, pDataBuf, BufferSize);

	free(pDataBuf);

	return result;
}

int Send_Get_Simulated_Correlation(void) {
	return Send_DCS_Command(GET_SIMULATED_DATA, NULL, 0);
}

int Receive_Simulated_Correlation(char* pDataBuf) {
	unsigned __int32 index = 0;//Keeps track of the current pDataBuf index.

	Simulated_Correlation Simulated_Corr = { 0 };

	memcpy(&Simulated_Corr.Precut, &pDataBuf[index], sizeof(Simulated_Corr.Precut));
	index += sizeof(Simulated_Corr.Precut);

	memcpy(&Simulated_Corr.Cha_ID, &pDataBuf[index], sizeof(Simulated_Corr.Cha_ID));
	index += sizeof(Simulated_Corr.Cha_ID);

	memcpy(&Simulated_Corr.Data_Num, &pDataBuf[index], sizeof(Simulated_Corr.Data_Num));
	index += sizeof(Simulated_Corr.Data_Num);

	//Change first three values from input to host endianess
	Simulated_Corr.Precut = itohl(Simulated_Corr.Precut);
	Simulated_Corr.Cha_ID = itohl(Simulated_Corr.Cha_ID);
	Simulated_Corr.Data_Num = itohl(Simulated_Corr.Data_Num);

	//Allocate array for correlation values.
	Simulated_Corr.pCorrBuf = malloc(Simulated_Corr.Data_Num * sizeof(*Simulated_Corr.pCorrBuf));
	if (Simulated_Corr.pCorrBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(Simulated_Corr.pCorrBuf, &pDataBuf[index], Simulated_Corr.Data_Num * sizeof(*Simulated_Corr.pCorrBuf));
	index += Simulated_Corr.Data_Num * sizeof(*Simulated_Corr.pCorrBuf);

	//Change each float from network to host endianess
	for (int x = 0; x < Simulated_Corr.Data_Num; x++) {
#pragma warning (disable: 6386)
		Simulated_Corr.pCorrBuf[x] = itohf(Simulated_Corr.pCorrBuf[x]);
#pragma warning (default: 6386)
	}

	Get_Simulated_Correlation_CB(&Simulated_Corr);

	free(Simulated_Corr.pCorrBuf);

	return NO_DCS_ERROR;
}

int Send_Optical_Param(Optical_Param_Type* pOpt_Param, int Cha_Num) {
	char* pDataBuf;
	unsigned __int32 index = 0;//Keeps track of the current pDataBuf index.
	const unsigned __int32 BufferSize = sizeof(Cha_Num) + Cha_Num * sizeof(*pOpt_Param);

	pDataBuf = malloc(BufferSize);
	if (pDataBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

#pragma warning (disable: 6385 6386)
	unsigned __int32 network_Cha_Num = htool(Cha_Num);
	memcpy(&pDataBuf[index], &network_Cha_Num, sizeof(network_Cha_Num));
	index += sizeof(network_Cha_Num);

	//Allocate output array of optical parameters.
	Optical_Param_Type* pOpt_Param_Out = malloc(Cha_Num * sizeof(*pOpt_Param_Out));
	if (pOpt_Param_Out == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	//Set array of params to output endianess.
	for (int x = 0; x < Cha_Num; x++) {
		pOpt_Param_Out[x].Cha_ID = htool(pOpt_Param[x].Cha_ID);

		pOpt_Param_Out[x].mua0 = htoof(pOpt_Param[x].mua0);
		pOpt_Param_Out[x].musp = htoof(pOpt_Param[x].musp);
	}
	memcpy(&pDataBuf[index], pOpt_Param_Out, Cha_Num * sizeof(*pOpt_Param_Out));
	index += Cha_Num * sizeof(*pOpt_Param_Out);
	free(pOpt_Param_Out);
#pragma warning (default: 6385 6386)

	int result = Send_DCS_Command(SET_OPTICAL_PARAM, pDataBuf, BufferSize);

	free(pDataBuf);

	return result;
}

int Send_Analyzer_Prefit_Param(Analyzer_Prefit_Param* pAnalyzer_Prefit_Param) {
	char* pDataBuf;
	const unsigned __int32 BufferSize = sizeof(*pAnalyzer_Prefit_Param);

	//Allocate memory for output buffer.
	pDataBuf = malloc(BufferSize);
	if (pDataBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

#pragma warning (disable: 6386 6385)
	//Change params to network byte order
	Analyzer_Prefit_Param network_Analyzer = { 0 };

	network_Analyzer.Precut = htool(pAnalyzer_Prefit_Param->Precut);
	network_Analyzer.PostCut = htool(pAnalyzer_Prefit_Param->PostCut);

	network_Analyzer.Min_Intensity = htoof(pAnalyzer_Prefit_Param->Min_Intensity);
	network_Analyzer.Max_Intensity = htoof(pAnalyzer_Prefit_Param->Max_Intensity);
	network_Analyzer.FitLimt = htoof(pAnalyzer_Prefit_Param->FitLimt);
	network_Analyzer.earlyLeakage = htoof(pAnalyzer_Prefit_Param->earlyLeakage);
	network_Analyzer.lightLeakage = htoof(pAnalyzer_Prefit_Param->lightLeakage);
	network_Analyzer.Model = &pAnalyzer_Prefit_Param->Model;

	//Copy to final output buffer.
	memcpy(pDataBuf, &network_Analyzer, sizeof(network_Analyzer));
#pragma warning (default: 6386 6385)

	int result = Send_DCS_Command(SET_ANALYZER_PREFIT_PARAM, pDataBuf, BufferSize);
	free(pDataBuf);

	return result;
}

int Send_Get_Analyzer_Prefit_Param(void) {
	return Send_DCS_Command(GET_ANALYZER_PREFIT_PARAM, NULL, 0);
}

int Receive_Analyzer_Prefit_Param(char* pDataBuf) {
	Analyzer_Prefit_Param pAnalyzer_Prefit_Param;

#pragma warning (disable: 6386 6385)
	//Copy received data to struct.
	memcpy(&pAnalyzer_Prefit_Param, pDataBuf, sizeof(pAnalyzer_Prefit_Param));

	//Change input endianess to host.
	pAnalyzer_Prefit_Param.Precut = itohl(pAnalyzer_Prefit_Param.Precut);
	pAnalyzer_Prefit_Param.PostCut = itohl(pAnalyzer_Prefit_Param.PostCut);
	pAnalyzer_Prefit_Param.Min_Intensity = itohf(pAnalyzer_Prefit_Param.Min_Intensity);
	pAnalyzer_Prefit_Param.Max_Intensity = itohf(pAnalyzer_Prefit_Param.Max_Intensity);
	pAnalyzer_Prefit_Param.FitLimt = itohf(pAnalyzer_Prefit_Param.FitLimt);
	pAnalyzer_Prefit_Param.earlyLeakage = itohf(pAnalyzer_Prefit_Param.earlyLeakage);
	pAnalyzer_Prefit_Param.lightLeakage = itohf(pAnalyzer_Prefit_Param.lightLeakage);
#pragma warning (default: 6386 6385)

	Get_Analyzer_Prefit_Param_CB(&pAnalyzer_Prefit_Param);

	return NO_DCS_ERROR;
}

int Receive_Error_Message(char* pDataBuf) {
	//Read 4 byte prepended string size.
	unsigned __int32 strSize;
	memcpy(&strSize, pDataBuf, sizeof(strSize));

	//Allocate memory for string.
	char* pMessage = malloc(strSize);
	if (pMessage == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(pMessage, &pDataBuf[sizeof(strSize)], strSize);

	printf(ANSI_COLOR_RED);
	printf("Remote DCS Error!\n");
	Get_Error_Message_CB(pMessage, strSize);
	printf(ANSI_COLOR_RESET);

	free(pMessage);
	return NO_DCS_ERROR;
}

int Receive_Command_ACK(char* pDataBuf) {
	Data_ID commandId;
	memcpy(&commandId, pDataBuf, sizeof(commandId));
	printf(ANSI_COLOR_GREEN"Command Ack: 0x%02x\n"ANSI_COLOR_RESET, commandId);

	//Check_Command_Response(COMMAND_RSP_VALIDATE, commandId);

	return NO_DCS_ERROR;
}

int Receive_BFI_Data(char* pDataBuf) {
	//Number of channels to expect in following data.
	unsigned __int32 numChannels;
	memcpy(&numChannels, &pDataBuf[0], sizeof(numChannels));
	numChannels = itohl(numChannels);

	//Pointer to the memory storing the BFI data structure array.
	BFI_Data* pBFI_Data = malloc(numChannels * sizeof(*pBFI_Data));
	if (pBFI_Data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	for (unsigned __int32 x = 0; x < numChannels; x++) {
		//Find index of BFI data in the raw buffer.
		unsigned __int32 rawDataOffset = sizeof(numChannels) + x * sizeof(*pBFI_Data);

		//For each struct member, copy from the input buffer and change to host byte order.
		BFI_Data* currentBFI = &pBFI_Data[x];
		memcpy(&currentBFI->Cha_ID, &pDataBuf[rawDataOffset], sizeof(currentBFI->Cha_ID));
		currentBFI->Cha_ID = itohl(currentBFI->Cha_ID);
		rawDataOffset += sizeof(currentBFI->Cha_ID);

		memcpy(&currentBFI->BFI, &pDataBuf[rawDataOffset], sizeof(currentBFI->BFI));
		currentBFI->BFI = itohf(currentBFI->BFI);
		rawDataOffset += sizeof(currentBFI->BFI);

		memcpy(&currentBFI->Beta, &pDataBuf[rawDataOffset], sizeof(currentBFI->Beta));
		currentBFI->Beta = itohf(currentBFI->Beta);
		rawDataOffset += sizeof(currentBFI->Beta);

		memcpy(&currentBFI->rMSE, &pDataBuf[rawDataOffset], sizeof(currentBFI->rMSE));
		currentBFI->rMSE = itohf(currentBFI->rMSE);
		rawDataOffset += sizeof(currentBFI->rMSE);
	}

	//Call user-defined callback
	Get_BFI_Data(pBFI_Data, numChannels);
	free(pBFI_Data);

	return NO_DCS_ERROR;
}

int Receive_BFI_Corr_Ready(char* pDataBuf) {
	Get_BFI_Corr_Ready_CB(true);
	return NO_DCS_ERROR;
}

int Receive_Corr_Intensity_Data(char* pDataBuf) {
	//Keeps track of current index while reading pDataBuf.
	unsigned __int32 index = 0;

	//Number of channels to expect in following data.
	unsigned __int32 numChannels;
	memcpy(&numChannels, &pDataBuf[index], sizeof(numChannels));
	numChannels = itohl(numChannels);
	index += sizeof(numChannels);

	//Allocating memory for numChannels channels of data.
	Corr_Intensity_Data* pCorr_Intensity_Data = malloc(sizeof(*pCorr_Intensity_Data) * numChannels);
	if (pCorr_Intensity_Data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

#pragma warning (disable: 6386 6385 6001)
	//Read correlation data for each channel.
	for (unsigned __int32 x = 0; x < numChannels; x++) {
		//Read the Cha_ID.
		memcpy(&pCorr_Intensity_Data[x].Cha_ID, &pDataBuf[index], sizeof(pCorr_Intensity_Data[x].Cha_ID));
		pCorr_Intensity_Data[x].Cha_ID = itohl(pCorr_Intensity_Data[x].Cha_ID);
		index += sizeof(pCorr_Intensity_Data[x].Cha_ID);

		//Read the intensity.
		memcpy(&pCorr_Intensity_Data[x].intensity, &pDataBuf[index], sizeof(pCorr_Intensity_Data[x].intensity));
		pCorr_Intensity_Data[x].intensity = itohf(pCorr_Intensity_Data[x].intensity);
		index += sizeof(pCorr_Intensity_Data[x].intensity);

		//Read data_num.
		memcpy(&pCorr_Intensity_Data[x].Data_Num, &pDataBuf[index], sizeof(pCorr_Intensity_Data[x].Data_Num));
		pCorr_Intensity_Data[x].Data_Num = itohl(pCorr_Intensity_Data[x].Data_Num);
		index += sizeof(pCorr_Intensity_Data[x].Data_Num);

		//Allocate memory for the correlation array based on data_num.
		pCorr_Intensity_Data[x].pCorrBuf = malloc(pCorr_Intensity_Data[x].Data_Num * sizeof(*pCorr_Intensity_Data[x].pCorrBuf));
		if (pCorr_Intensity_Data[x].pCorrBuf == NULL) {
			//If allocation fails, loop backwards to free loop-allocated memory.
			for (int y = x - 1; y >= 0; y--) {
				free(pCorr_Intensity_Data[y].pCorrBuf);
			}
			free(pCorr_Intensity_Data);
			return MEMORY_ALLOCATION_ERROR;
		}

		//Read data into array.
		for (int y = 0; y < pCorr_Intensity_Data[x].Data_Num; y++) {
			memcpy(&pCorr_Intensity_Data[x].pCorrBuf[y], &pDataBuf[index], sizeof(*pCorr_Intensity_Data[x].pCorrBuf));
			pCorr_Intensity_Data[x].pCorrBuf[y] = itohf(pCorr_Intensity_Data[x].pCorrBuf[y]);
			index += sizeof(*pCorr_Intensity_Data[x].pCorrBuf);
		}
	}

	//Read in delay values.
	//Get number of values.
	unsigned __int32 Delay_Num;
	memcpy(&Delay_Num, &pDataBuf[index], sizeof(Delay_Num));
	Delay_Num = itohl(Delay_Num);
	index += sizeof(Delay_Num);

	//Allocate memory for actual values.
	float* pDelayBuf = malloc(Delay_Num * sizeof(*pDelayBuf));
	if (pDelayBuf == NULL) {
		//Free dynamically allocated resources.
		for (unsigned __int32 x = 0; x < numChannels; x++) {
			free(pCorr_Intensity_Data[x].pCorrBuf);
		}
		free(pCorr_Intensity_Data);
		return MEMORY_ALLOCATION_ERROR;
	}

	//Read in actual values.
	for (unsigned __int32 x = 0; x < Delay_Num; x++) {
		memcpy(&pDelayBuf[x], &pDataBuf[index], sizeof(*pDelayBuf));
		pDelayBuf[x] = itohf(pDelayBuf[x]);
		index += sizeof(*pDelayBuf);
	}

	Get_Corr_Intensity_Data_CB(pCorr_Intensity_Data, numChannels, pDelayBuf, Delay_Num);

	//Free dynamically allocated resources.
	for (unsigned __int32 x = 0; x < numChannels; x++) {
		free(pCorr_Intensity_Data[x].pCorrBuf);
	}
	free(pCorr_Intensity_Data);
	free(pDelayBuf);

#pragma warning (default: 6386 6385 6001)

	return NO_DCS_ERROR;
}

int Receive_Intensity_Data(char* pDataBuf) {
	unsigned __int32 index = 0;

	//Number of channels to expect in following data.
	unsigned __int32 numChannels;
	memcpy(&numChannels, &pDataBuf[index], sizeof(numChannels));
	numChannels = itohl(numChannels);
	index += sizeof(numChannels);

	//Allocating memory for numChannels channels of data.
	Intensity_Data* pIntensity_Data = malloc(sizeof(*pIntensity_Data) * numChannels);
	if (pIntensity_Data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	for (unsigned __int32 x = 0; x < numChannels; x++) {
#pragma warning (disable: 6386 6385)
		//Read the Cha_ID.
		memcpy(&pIntensity_Data[x].Cha_ID, &pDataBuf[index], sizeof(pIntensity_Data[x].Cha_ID));
		pIntensity_Data[x].Cha_ID = itohl(pIntensity_Data[x].Cha_ID);
		index += sizeof(pIntensity_Data[x].Cha_ID);

		//Read the intensity.
		memcpy(&pIntensity_Data[x].intensity, &pDataBuf[index], sizeof(pIntensity_Data[x].intensity));
		pIntensity_Data[x].intensity = itohf(pIntensity_Data[x].intensity);
		index += sizeof(pIntensity_Data[x].intensity);
#pragma warning (default: 6386 6385)
	}

	Get_Intensity_Data_CB(pIntensity_Data, numChannels);

	free(pIntensity_Data);

	return NO_DCS_ERROR;
}

int Send_DCS_Command(Data_ID data_ID, char* pDataBuf, const unsigned __int32 BufferSize) {
	//Allocate memory for [pTransmission].
	Transmission_Data_Type* pTransmission = malloc(sizeof(*pTransmission));
	if (pTransmission == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	//Transmission size = 2(Header) + 4(Type ID) + 4(Data ID) + BufferSize + 1 (Checksum)
	pTransmission->size = sizeof(Frame_Version) + sizeof(Type_ID) + sizeof(Data_ID) + BufferSize + 1;

	unsigned __int32 index = 0; //Index to track position in pFrame.
	pTransmission->pFrame = malloc(pTransmission->size);
	if (pTransmission->pFrame == NULL) {
		free(pTransmission);
		return MEMORY_ALLOCATION_ERROR;
	}

	//Change frame version to output byte order and copy to output buffer.
	const Frame_Version frame_version = htoos(FRAME_VERSION);
#pragma warning (disable: 6386)
	memcpy(&pTransmission->pFrame[index], &frame_version, sizeof(frame_version));
	index += sizeof(frame_version);
#pragma warning (default: 6386)

	//Change command type to output byte order and copy to output buffer.
	const Type_ID command = htool(COMMAND_ID);
	memcpy(&pTransmission->pFrame[index], &command, sizeof(command));
	index += sizeof(command);

	//Change data ID to output byte order and copy to output buffer.
	const Data_ID data_ID_out = htool(data_ID);
	memcpy(&pTransmission->pFrame[index], &data_ID, sizeof(data_ID));
	index += sizeof(data_ID);

	//Copy main data to output buffer.
	memcpy(&pTransmission->pFrame[index], pDataBuf, BufferSize);
	index += BufferSize;

	//Add checksum calculated from 2(Header) + 4(Type ID) + 4(Data ID) + BufferSize
	pTransmission->pFrame[pTransmission->size - 1] = compute_checksum(pTransmission->pFrame, pTransmission->size - 1);

	int result = Enqueue_Trans_FIFO(pTransmission);
	Check_Command_Response(COMMAND_RSP_SET, data_ID);
	return result;
}

Checksum compute_checksum(char* pDataBuf, unsigned __int32 size) {
	unsigned __int8 xor_sum = 0;
	for (unsigned int index = 0; index < size; index++) {
		xor_sum ^= pDataBuf[index];
	}

	return xor_sum;
}

bool check_checksum(char* pDataBuf, unsigned __int32 size) {
	unsigned __int8 xor_sum = 0;
	for (unsigned __int32 index = 0; index < size; index++) {
		xor_sum ^= pDataBuf[index];
	}

	return xor_sum == 0x00;
}


////////////////////////////////
//Endianess management functions
////////////////////////////////

/*6 endianess management functions are defined with 5 character function names.
The first 4 characters describe which direction the conversion is doing.
"htoo" refers to "host to output" & "itoh" refers to "input to host".
The last character refers to the data type. "l" is long, "s" is short,
"f" is float.*/

//Determines if the host byte order is the same as the intended output byte order.
static inline bool should_swap_htoo(void) {
	if (!IS_BIG_ENDIAN && ENDIANESS_OUTPUT == BIG_ENDIAN) {
		return true;
	}
	if (IS_BIG_ENDIAN && ENDIANESS_OUTPUT == LITTLE_ENDIAN) {
		return true;
	}
	return false;
}

//Determines if the input byte order is the same as the host byte order.
static inline bool should_swap_itoh(void) {
	if (!IS_BIG_ENDIAN && ENDIANESS_INPUT == BIG_ENDIAN) {
		return true;
	}
	if (IS_BIG_ENDIAN && ENDIANESS_INPUT == LITTLE_ENDIAN) {
		return true;
	}
	return false;
}

//Swaps the byte order of a float.
static inline float swap_float(const float inFloat) {
	float retVal = 0.0f;
	char* floatToConvert = (char*)&inFloat;
	char* returnFloat = (char*)&retVal;

	//Swap the bytes into a temporary buffer
	returnFloat[0] = floatToConvert[3];
	returnFloat[1] = floatToConvert[2];
	returnFloat[2] = floatToConvert[1];
	returnFloat[3] = floatToConvert[0];

	return retVal;
}

//Converts long from host to output byte order.
u_long htool(u_long hostlong) {
	if (should_swap_htoo()) {
		return Swap32(hostlong);
	}
	return hostlong;
}

//Converts short from host to output byte order.
u_short htoos(u_short hostshort) {
	if (should_swap_htoo()) {
		return Swap16(hostshort);
	}
	return hostshort;
}

//Converts float from host to output byte order.
float htoof(float value) {
	if (should_swap_htoo()) {
		return swap_float(value);
	}
	return value;
}

//Converts long from input to host byte order.
u_long itohl(u_long ilong) {
	if (should_swap_itoh()) {
		return Swap32(ilong);
	}
	return ilong;
}

//Converts short from input to host byte order.
u_short itohs(u_short ishort) {
	if (should_swap_itoh()) {
		return Swap16(ishort);
	}
	return ishort;
}

//Converts float from input to host byte order.
float itohf(float value) {
	if (should_swap_itoh()) {
		return swap_float(value);
	}
	return value;
}

//Convenience function for dumping data to stdout in debug builds, but nothing in release.
void hexDump(const char* desc, const void* addr, const unsigned __int32 len) {
#if defined(_DEBUG)
	unsigned __int32 i;
	unsigned char buff[17];
	const unsigned char* pc = (const unsigned char*)addr;

	// Output description if given.
	if (desc != NULL)
		printf("%s:\n", desc);

	// Length checks.
	if (len == 0) {
		printf("  ZERO LENGTH\n");
		return;
	}

	// Process every byte in the data.
	for (i = 0; i < len; i++) {
		// Multiple of 16 means new line (with line offset).
		if ((i % 16) == 0) {
			// Don't print ASCII buffer for the "zeroth" line.
			if (i != 0)
				printf("  %s\n", buff);
			// Output the offset.
			printf("  %04x ", i);
		}

		// Now the hex code for the specific character.
		printf(" %02x", pc[i]);

		// And buffer a printable ASCII character for later.
		if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
			buff[i % 16] = '.';
		else
			buff[i % 16] = pc[i];
		buff[(i % 16) + 1] = '\0';
	}

	// Pad out last line if not exactly 16 characters.
	while ((i % 16) != 0) {
		printf("   ");
		i++;
	}

	// And print the final ASCII buffer.
	printf("  %s\n", buff);
#endif
}