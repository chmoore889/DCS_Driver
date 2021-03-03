#define _CRTDBG_MAP_ALLOC
#include<stdlib.h>
#include<crtdbg.h>
#include<stdio.h>
#include<stdbool.h>
#include<memory.h>
#include<math.h>

#pragma comment (lib, "Ws2_32.lib")

#include "DCS_Driver.h"
#include "Internal.h"
#include "COM_Task.h"

#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORT "50000"
#define HOST_NAME "129.49.117.79"

//Convenience function for dumping data to stdout in debug builds, but nothing in release.
void hexDump(const char* desc, const void* addr, const int len) {
#if defined(_DEBUG)
	int i;
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
	else if (len < 0) {
		printf("  NEGATIVE LENGTH: %d\n", len);
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

int Get_DCS_Status(void) {
	return Send_Get_DSC_Status();
}

int Send_Get_DSC_Status(void) {
	return Send_DCS_Command(GET_DCS_STATUS, NULL, 0);
}

int Receive_DCS_Status(char* pDataBuf) {
	bool bCorr; // TRUE if correlator is started, FALSE if the correlator is not started.
	bool bAnalyzer; // TRUE if analyzer is started, FALSE if the analyzer is not started.
	int DCS_Cha_Num; // number of total DCS channels on the remote DCS.

	memcpy(&bCorr, &pDataBuf[0], sizeof(bCorr));
	memcpy(&bAnalyzer, &pDataBuf[1], sizeof(bAnalyzer));
	memcpy(&DCS_Cha_Num, &pDataBuf[2], sizeof(DCS_Cha_Num));

	//Change from network to host byte order
	DCS_Cha_Num = itohl(DCS_Cha_Num);

	Get_DCS_Status_CB(bCorr, bAnalyzer, DCS_Cha_Num);

	return NO_DCS_ERROR;
}

int Set_Correlator_Setting(Correlator_Setting_Type* pCorr_Setting) {
	return Send_Correlator_Setting(pCorr_Setting);
}

int Send_Correlator_Setting(Correlator_Setting_Type* pCorrelator_Setting) {
	char* pDataBuf; //data buffer for the byte stream of the correlator setting data
	int BufferSize = sizeof(*pCorrelator_Setting); //data size of the buffer pDataBuf

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

	//Copy struct into data buffer and change to network byte order
	int Data_N = htool(pCorrelator_Setting->Data_N);
	memcpy(&pDataBuf[0], &Data_N, sizeof(Data_N));

	int Scale = htool(pCorrelator_Setting->Scale);
	memcpy(&pDataBuf[4], &Scale, sizeof(Scale));

	int SampleSize = htool((int)ceil(pCorrelator_Setting->Corr_Time / pCorrelator_Setting->Data_N / 200e-9));
	memcpy(&pDataBuf[8], &SampleSize, sizeof(SampleSize));

	int result = Send_DCS_Command(SET_CORRELATOR_SETTING, pDataBuf, BufferSize);
	free(pDataBuf);

	return result;
}

int Get_Correlator_Setting(void) {
	return Send_Get_Correlator_Setting();
}

int Send_Get_Correlator_Setting(void) {
	return Send_DCS_Command(GET_CORRELATOR_SETTING, NULL, 0);
}

int Receive_Correlator_Setting(char* pDataBuf) {
	Correlator_Setting_Type* pCorrelator_Setting = malloc(sizeof(Correlator_Setting_Type));
	if (pCorrelator_Setting == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	int Data_N;
	int Scale;
	int Sample_Size;
	memcpy(&Data_N, &pDataBuf[0], sizeof(Data_N));
	memcpy(&Scale, &pDataBuf[4], sizeof(Scale));
	memcpy(&Sample_Size, &pDataBuf[8], sizeof(Sample_Size));

	//Change network endianess to host
	Data_N = itohl(Data_N);
	Scale = itohl(Scale);
	Sample_Size = itohl(Sample_Size);

	memcpy(&pCorrelator_Setting->Data_N, &Data_N, sizeof(Data_N));
	memcpy(&pCorrelator_Setting->Scale, &Scale, sizeof(Scale));

	//Reverse Corr_Time calculation
	float Corr_Time = (float)2e-7 * Sample_Size * Data_N;
	memcpy(&pCorrelator_Setting->Corr_Time, &Corr_Time, sizeof(Corr_Time));

	Get_Correlator_Setting_CB(pCorrelator_Setting);
	free(pCorrelator_Setting);

	return NO_DCS_ERROR;
}

int Set_Analyzer_Setting(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num) {
	return Send_Analyzer_Setting(pAnalyzer_Setting, Cha_Num);
}

int Send_Analyzer_Setting(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num) {
	char* pDataBuf; //data buffer for the byte stream of the analyzer setting data
	int BufferSize = sizeof(Cha_Num) + Cha_Num * sizeof(*pAnalyzer_Setting);

	pDataBuf = malloc(BufferSize);
	if (pDataBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

#pragma warning (disable: 6386 6385)
	int network_Cha_Num = htool(Cha_Num);
	memcpy(&pDataBuf[0], &network_Cha_Num, sizeof(Cha_Num));

	//Change analyzer settings to network byte order
	Analyzer_Setting_Type* network_Analyzer = malloc(Cha_Num * sizeof(Analyzer_Setting_Type));
	if (network_Analyzer == NULL) {
		free(pDataBuf);
		return MEMORY_ALLOCATION_ERROR;
	}

	for (int x = 0; x < Cha_Num; x++) {
		network_Analyzer[x].Alpha = htoof(pAnalyzer_Setting[x].Alpha);
		network_Analyzer[x].Beta = htoof(pAnalyzer_Setting[x].Beta);
		network_Analyzer[x].Db = htoof(pAnalyzer_Setting[x].Db);
		network_Analyzer[x].Distance = htoof(pAnalyzer_Setting[x].Distance);
		network_Analyzer[x].mua0 = htoof(pAnalyzer_Setting[x].mua0);
		network_Analyzer[x].musp = htoof(pAnalyzer_Setting[x].musp);
		network_Analyzer[x].Wavelength = htoof(pAnalyzer_Setting[x].Wavelength);
	}

	memcpy(&pDataBuf[4], network_Analyzer, Cha_Num * sizeof(*network_Analyzer));
	free(network_Analyzer);
#pragma warning (default: 6386 6385)

	int result = Send_DCS_Command(SET_ANALYZER_SETTING, pDataBuf, BufferSize);
	free(pDataBuf);

	return result;
}

int Get_Analyzer_Setting(void) {
	return Send_Get_Analyzer_Setting();
}

int Send_Get_Analyzer_Setting(void) {
	return Send_DCS_Command(GET_ANALYZER_SETTING, NULL, 0);
}

int Receive_Analyzer_Setting(char* pDataBuf) {
	Analyzer_Setting_Type* pAnalyzer_Setting;
	int Cha_Num;

	memcpy(&Cha_Num, &pDataBuf[0], sizeof(Cha_Num));
	Cha_Num = itohl(Cha_Num);

	pAnalyzer_Setting = malloc(Cha_Num * sizeof(*pAnalyzer_Setting));
	if (pAnalyzer_Setting == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

#pragma warning (disable: 6386 6385)
	for (int x = 0; x < Cha_Num; x++) {
		memcpy(&pAnalyzer_Setting[x], &pDataBuf[sizeof(Cha_Num) + x * sizeof(*pAnalyzer_Setting)], sizeof(*pAnalyzer_Setting));

		//Change network endianess to host
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

int Start_DCS_Measurement(int interval, int* pCha_IDs, int Cha_Num) {
	return Send_Start_Measurement(interval, pCha_IDs, Cha_Num);
}

int Send_Start_Measurement(int Interval, int* pCha_IDs, int Cha_Num) {
	char* pDataBuf;
	unsigned int BufferSize = sizeof(Interval) + sizeof(Cha_Num) + Cha_Num * sizeof(*pCha_IDs);

	pDataBuf = malloc(BufferSize);
	if (pDataBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

#pragma warning (disable: 6386)
	int network_Interval = htool(Interval);
	memcpy(&pDataBuf[0], &network_Interval, sizeof(network_Interval));

	int network_Cha_Num = htool(Cha_Num);
	memcpy(&pDataBuf[4], &network_Cha_Num, sizeof(network_Cha_Num));

	int* network_Cha_IDs = malloc(Cha_Num * sizeof(*network_Cha_IDs));
	if (network_Cha_IDs == NULL) {
		free(pDataBuf);
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(network_Cha_IDs, pCha_IDs, Cha_Num * sizeof(*network_Cha_IDs));
	for (int x = 0; x < Cha_Num; x++) {
		network_Cha_IDs[x] = htool(network_Cha_IDs[x]);
	}
	memcpy(&pDataBuf[8], network_Cha_IDs, Cha_Num * sizeof(*pCha_IDs));
#pragma warning (default: 6386)
	free(network_Cha_IDs);

	int result = Send_DCS_Command(START_MEASUREMENT, pDataBuf, BufferSize);

	free(pDataBuf);

	return result;
}

int Stop_DCS_Measurement(void) {
	return Send_Stop_Measurement();
}

int Send_Stop_Measurement(void) {
	return Send_DCS_Command(STOP_MEASUREMENT, NULL, 0);
}

int Enable_DCS(bool bCorr, bool bAnalyzer) {
	return Send_Enable_DCS(bCorr, bAnalyzer);
}

int Send_Enable_DCS(bool bCorr, bool bAnalyzer) {
	const unsigned int BufferSize = sizeof(bCorr) + sizeof(bAnalyzer);
	char* pDataBuf = malloc(BufferSize);
	if (pDataBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(&pDataBuf[0], &bCorr, sizeof(bCorr));
	memcpy(&pDataBuf[1], &bAnalyzer, sizeof(bAnalyzer));

	int result = Send_DCS_Command(ENABLE_CORR_ANALYZER, pDataBuf, BufferSize);

	free(pDataBuf);

	return result;
}

int Get_Simulated_Correlation(void) {
	return Send_Get_Simulated_Correlation();
}

int Send_Get_Simulated_Correlation(void) {
	return Send_DCS_Command(GET_SIMULATED_DATA, NULL, 0);
}

int Receive_Simulated_Correlation(char* pDataBuf) {
	Simulated_Corr_Type Simulated_Corr = { 0 };

	memcpy(&Simulated_Corr.Precut, &pDataBuf[0], sizeof(Simulated_Corr.Precut));
	memcpy(&Simulated_Corr.Cha_ID, &pDataBuf[4], sizeof(Simulated_Corr.Cha_ID));
	memcpy(&Simulated_Corr.Data_Num, &pDataBuf[8], sizeof(Simulated_Corr.Data_Num));

	//Change first three values from network to host endianess
	Simulated_Corr.Precut = itohl(Simulated_Corr.Precut);
	Simulated_Corr.Cha_ID = itohl(Simulated_Corr.Cha_ID);
	Simulated_Corr.Data_Num = itohl(Simulated_Corr.Data_Num);

	Simulated_Corr.pCorrBuf = malloc(Simulated_Corr.Data_Num * sizeof(*Simulated_Corr.pCorrBuf));
	if (Simulated_Corr.pCorrBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(Simulated_Corr.pCorrBuf, &pDataBuf[12], Simulated_Corr.Data_Num * sizeof(*Simulated_Corr.pCorrBuf));

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

int Set_Optical_Param(Optical_Param_Type* pOpt_Param, int Cha_Num) {
	return Send_Optical_Param(pOpt_Param, Cha_Num);
}

int Send_Optical_Param(Optical_Param_Type* pOpt_Param, int Cha_Num) {
	char* pDataBuf;
	unsigned int BufferSize = sizeof(Cha_Num) + Cha_Num * sizeof(*pOpt_Param);

	pDataBuf = malloc(BufferSize);
	if (pDataBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

#pragma warning (disable: 6386)
	unsigned int network_Cha_Num = htool(Cha_Num);
	memcpy(&pDataBuf[0], &network_Cha_Num, sizeof(network_Cha_Num));

	//Change array of params to network endianess
	for (int x = 0; x < Cha_Num; x++) {
		pOpt_Param[x].Cha_ID = htool(pOpt_Param[x].Cha_ID);

		pOpt_Param[x].mua0 = htoof(pOpt_Param[x].mua0);
		pOpt_Param[x].musp = htoof(pOpt_Param[x].musp);
	}
	memcpy(&pDataBuf[4], pOpt_Param, Cha_Num * sizeof(*pOpt_Param));
#pragma warning (default: 6386)

	int result = Send_DCS_Command(SET_OPTICAL_PARAM, pDataBuf, BufferSize);

	free(pDataBuf);

	return result;
}

int Set_Analyzer_Prefit_Param(Analyzer_Prefit_Param_Type* pAnalyzer_Prefit_Param) {
	return Send_Analyzer_Prefit_Param(pAnalyzer_Prefit_Param);
}

int Send_Analyzer_Prefit_Param(Analyzer_Prefit_Param_Type* pAnalyzer_Prefit_Param) {
	char* pDataBuf;
	int BufferSize = sizeof(*pAnalyzer_Prefit_Param);

	pDataBuf = malloc(BufferSize);
	if (pDataBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

#pragma warning (disable: 6386 6385)
	//Change params to network byte order
	Analyzer_Prefit_Param_Type network_Analyzer = { 0 };

	network_Analyzer.Precut = htool(pAnalyzer_Prefit_Param->Precut);
	network_Analyzer.PostCut = htool(pAnalyzer_Prefit_Param->PostCut);

	network_Analyzer.Min_Intensity = htoof(pAnalyzer_Prefit_Param->Min_Intensity);
	network_Analyzer.Max_Intensity = htoof(pAnalyzer_Prefit_Param->Max_Intensity);
	network_Analyzer.FitLimt = htoof(pAnalyzer_Prefit_Param->FitLimt);
	network_Analyzer.earlyLeakage = htoof(pAnalyzer_Prefit_Param->earlyLeakage);
	network_Analyzer.lightLeakage = htoof(pAnalyzer_Prefit_Param->lightLeakage);
	network_Analyzer.Model = &pAnalyzer_Prefit_Param->Model;

	memcpy(pDataBuf, &network_Analyzer, sizeof(network_Analyzer));
#pragma warning (default: 6386 6385)

	int result = Send_DCS_Command(SET_ANALYZER_PREFIT_PARAM, pDataBuf, BufferSize);
	free(pDataBuf);

	return result;
}

int Get_Analyzer_Prefit_Param(void) {
	return Send_Get_Analyzer_Prefit_Param();
}

int Send_Get_Analyzer_Prefit_Param(void) {
	return Send_DCS_Command(GET_ANALYZER_PREFIT_PARAM, NULL, 0);
}

int Receive_Analyzer_Prefit_Param(char* pDataBuf) {
	Analyzer_Prefit_Param_Type pAnalyzer_Prefit_Param;

#pragma warning (disable: 6386 6385)
	memcpy(&pAnalyzer_Prefit_Param, pDataBuf, sizeof(pAnalyzer_Prefit_Param));

	//Change network endianess to host
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
	//Read 4 bytes prepended string size
	unsigned __int32 strSize;
	memcpy(&strSize, pDataBuf, sizeof(strSize));

	//Allocate memory for string
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
	unsigned __int32 commandId;
	memcpy(&commandId, pDataBuf, sizeof(commandId));
	printf(ANSI_COLOR_GREEN"Command Ack: 0x%02x\n"ANSI_COLOR_RESET, commandId);

	return NO_DCS_ERROR;
}

int Receive_BFI_Data(char* pDataBuf) {
	//Number of channels to expect in following data.
	unsigned __int32 numChannels;
	memcpy(&numChannels, &pDataBuf[0], sizeof(numChannels));
	numChannels = itohl(numChannels);

	//Pointer to the memory storing the BFI data structure array.
	BFI_Data_Type* pBFI_Data = malloc(numChannels * sizeof(*pBFI_Data));
	for (unsigned int x = 0; x < numChannels; x++) {
		//Find index of BFI data in the raw buffer.
		unsigned __int32 rawDataOffset = sizeof(numChannels) + x * sizeof(*pBFI_Data);

		BFI_Data_Type* currentBFI = &pBFI_Data[x];
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

	Get_BFI_Data(pBFI_Data, numChannels);
	free(pBFI_Data);

	return NO_DCS_ERROR;
}

int Receive_Corr_Intensity_Data(char* pDataBuf) {
	//Index to keep track of where in the buffer we are.
	size_t dataBufIndex = 0;

	//The pointer to the memory storing the correlation and intensity.
	Corr_Intensity_Data_Type* pCorr_Intensity_Data;

	unsigned __int32 Cha_Num;
	memcpy(&Cha_Num, &pDataBuf[dataBufIndex], sizeof(Cha_Num));
	dataBufIndex += sizeof(Cha_Num);

	//Allocate the memory for the values for each channel.
	pCorr_Intensity_Data = malloc(Cha_Num * sizeof(*pCorr_Intensity_Data));
	if (pCorr_Intensity_Data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	for (unsigned int x = 0; x < Cha_Num; x++) {
		//Local loop pointer so that the address and array dereference operators do not need to be used every time.
		Corr_Intensity_Data_Type* local_ptr = &pCorr_Intensity_Data[x];

		unsigned __int32 Cha_ID;
		memcpy(&Cha_ID, &pDataBuf[dataBufIndex], sizeof(Cha_ID));
		local_ptr->Cha_ID = itohl(Cha_ID);
		dataBufIndex += sizeof(Cha_ID);

		float intensity;
		memcpy(&intensity, &pDataBuf[dataBufIndex], sizeof(intensity));
		local_ptr->intensity = itohf(intensity);
		dataBufIndex += sizeof(intensity);

		unsigned __int32 Data_Num;
		memcpy(&Data_Num, &pDataBuf[dataBufIndex], sizeof(Data_Num));
		local_ptr->Data_Num = itohl(Data_Num);
		dataBufIndex += sizeof(Data_Num);

		local_ptr->pCorrBuf = malloc(Data_Num * sizeof(float));
		if (local_ptr->pCorrBuf == NULL) {
			return MEMORY_ALLOCATION_ERROR;
		}

		for (unsigned int y = 0; y < Data_Num; y++) {
			float val;
			memcpy(&val, &pDataBuf[dataBufIndex], sizeof(val));
			dataBufIndex += sizeof(val);

			local_ptr->pCorrBuf[y] = itohf(val);
		}
	}

	int Delay_Num;
	float* pDelayBuf;
	memcpy(&Delay_Num, &pDataBuf[dataBufIndex], sizeof(Delay_Num));
	dataBufIndex += sizeof(Delay_Num);

	pDelayBuf = malloc(Delay_Num * sizeof(*pDelayBuf));
	if (pDelayBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	for (int y = 0; y < Delay_Num; y++) {
		float val;
		memcpy(&val, &pDataBuf[dataBufIndex], sizeof(val));
		dataBufIndex += sizeof(val);

		pDelayBuf[y] = itohf(val);
	}

	Get_Corr_Intensity_Data_CB(pCorr_Intensity_Data, Cha_Num, pDelayBuf, Delay_Num);

	//Release all previously allocated memory.
	for (unsigned int x = 0; x < Cha_Num; x++) {
		free(pCorr_Intensity_Data[x].pCorrBuf);
	}
	free(pCorr_Intensity_Data);
	free(pDelayBuf);

	return NO_DCS_ERROR;
}

int Receive_Intensity_Data(char* pDataBuf) {
	//Index to keep track of where in the buffer we are.
	size_t dataBufIndex = 0;

	//Buffer to store the intensity data.
	Intensity_Data_Type* pIntensity_Data;

	unsigned __int32 Cha_Num;
	memcpy(&Cha_Num, &pDataBuf[dataBufIndex], sizeof(Cha_Num));
	dataBufIndex += sizeof(Cha_Num);

	//Allocate the memory for the values for each channel.
	pIntensity_Data = malloc(Cha_Num * sizeof(*pIntensity_Data));
	if (pIntensity_Data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	for (unsigned int x = 0; x < Cha_Num; x++) {
		unsigned __int32 Cha_ID;
		float Intensity;

		memcpy(&Cha_ID, &pDataBuf[dataBufIndex], sizeof(Cha_ID));
		dataBufIndex += sizeof(Cha_Num);

		memcpy(&Intensity, &pDataBuf[dataBufIndex], sizeof(Intensity));
		dataBufIndex += sizeof(Intensity);

		pIntensity_Data[x].Cha_ID = itohl(Cha_ID);
		pIntensity_Data[x].intensity = itohf(Intensity);
	}

	Get_Intensity_Data_CB(pIntensity_Data, Cha_Num);

	free(pIntensity_Data);

	return NO_DCS_ERROR;
}

int Receive_BFI_Correlation_Ready() {
	printf("Received BFI Correlation Ready\n");

	return NO_DCS_ERROR;
}

int Send_DCS_Command(Data_ID data_ID, char* pDataBuf, unsigned int BufferSize) {
	Transmission_Data_Type* pTransmission = malloc(sizeof(Transmission_Data_Type));
	if (pTransmission == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	//Transmission size = 2(Header) + 4(Type ID) + 4(Data ID) + BufferSize + 1 (Checksum)
	unsigned int size = 2 + 4 + 4 + BufferSize + 1;
	pTransmission->size = size;

	pTransmission->pFrame = malloc(pTransmission->size);
	if (pTransmission->pFrame == NULL) {
		free(pTransmission);
		return MEMORY_ALLOCATION_ERROR;
	}

	const unsigned __int16 frame_version = htoos(FRAME_VERSION);
#pragma warning (disable: 6386)
	memcpy(&pTransmission->pFrame[0], &frame_version, sizeof(frame_version));
#pragma warning (default: 6386)

	const unsigned __int32 command = htool(COMMAND_ID);
	memcpy(&pTransmission->pFrame[2], &command, sizeof(command));
	data_ID = htool(data_ID);
	memcpy(&pTransmission->pFrame[6], &data_ID, sizeof(data_ID));
	memcpy(&pTransmission->pFrame[10], pDataBuf, BufferSize);

	//Add checksum calculated from 2(Header) + 4(Type ID) + 4(Data ID) + BufferSize
	pTransmission->pFrame[10 + BufferSize] = compute_checksum(pTransmission->pFrame, size - 1);

	return Enqueue_Trans_FIFO(pTransmission);
}

unsigned __int8 compute_checksum(char* pDataBuf, unsigned int size) {
	unsigned __int8 xor_sum = 0;
	for (size_t index = 0; index < size; index++) {
		xor_sum ^= pDataBuf[index];
	}

	return xor_sum;
}

bool check_checksum(char* pDataBuf, size_t size) {
	unsigned __int8 xor_sum = 0;
	for (size_t index = 0; index < size; index++) {
		xor_sum ^= pDataBuf[index];
	}

	return xor_sum == 0x00;
}

////////////////////////////////
//Endianess management functions
////////////////////////////////
static inline bool should_swap_htoo(void) {
	if (!IS_BIG_ENDIAN && ENDIANESS_OUTPUT == BIG_ENDIAN) {
		return true;
	}
	if (IS_BIG_ENDIAN && ENDIANESS_OUTPUT == LITTLE_ENDIAN) {
		return true;
	}
	return false;
}

static inline bool should_swap_itoh(void) {
	if (!IS_BIG_ENDIAN && ENDIANESS_INPUT == BIG_ENDIAN) {
		return true;
	}
	if (IS_BIG_ENDIAN && ENDIANESS_INPUT == LITTLE_ENDIAN) {
		return true;
	}
	return false;
}


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

u_long htool(u_long hostlong) {
	if (should_swap_htoo()) {
		return Swap32(hostlong);
	}
	return hostlong;
}

u_short htoos(u_short hostshort) {
	if (should_swap_htoo()) {
		return Swap16(hostshort);
	}
	return hostshort;
}

float htoof(float value) {
	if (should_swap_htoo()) {
		return swap_float(value);
	}
	return value;
}

u_long itohl(u_long ilong) {
	if (should_swap_itoh()) {
		return Swap32(ilong);
	}
	return ilong;
}

u_short itohs(u_short ishort) {
	if (should_swap_itoh()) {
		return Swap16(ishort);
	}
	return ishort;
}

float itohf(float value) {
	if (should_swap_itoh()) {
		return swap_float(value);
	}
	return value;
}