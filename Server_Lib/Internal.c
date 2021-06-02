#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "Server_Lib.h"
#include "Internal.h"
#include "Store.h"
#include "Data_Gen.h"

static int Send_DCS_Data(Data_ID data_ID, char* pDataBuf, const unsigned __int32 BufferSize);
static int Send_Command_Ack(Data_ID id);

static int Send_Intensity_Data(Intensity_Data* dataArray, unsigned __int32 arrLength);
static int Send_BFI_Data(BFI_Data* dataArray, unsigned __int32 arrLength);
static int Send_Corr_Intensity_Data(Corr_Intensity_Data* dataArray, unsigned __int32 arrLength, float* delays, unsigned __int32 delay_Num);

static int Process_DCS_Status();
static int Process_Corr_Set(char* buff);
static int Process_Corr_Status();
static int Process_Analyzer_Set(char* buff);
static int Process_Analyzer_Status();
static int Process_Start_DCS(char* buff);
static int Process_Stop_DCS();
static int Process_Enable_DCS(char* buff);
static int Process_Simulated_Correlation();
static int Process_Optical_Set(char* buff);
static int Process_Analyzer_Prefit(char* buff);
static int Process_Get_Analyzer_Prefit();

int process_recv(char* buff, unsigned __int32 buffLen) {
	hexDump("process_recv", buff, buffLen);

	//Verify checksum.
	if (!check_checksum(buff, buffLen)) {
		printf("Checksum error\n");
		return FRAME_CHECKSUM_ERROR;
	}

	unsigned __int32 index = 0; //Index to track position in buff.

	//Ensure header is correct
	Frame_Version header;
	memcpy(&header, &buff[index], sizeof(header));
	index += sizeof(header);

	header = itohs(header);
	if (header != FRAME_VERSION) {
		printf("Invalid header\n");
		return FRAME_VERSION_ERROR;
	}

	//Ensure type id is correct.
	Type_ID type_id;
	memcpy(&type_id, &buff[index], sizeof(type_id));
	index += sizeof(type_id);

	type_id = itohl(type_id);
	if (type_id != COMMAND_ID) {
		printf("Invalid Type ID\n");
		return FRAME_INVALID_DATA;
	}

	//Get data id to later call correct callbacks based on data id.
	Data_ID data_id;
	memcpy(&data_id, &buff[index], sizeof(data_id));
	index += sizeof(data_id);

	data_id = itohl(data_id);

	//Obtain just the data portion of the frame and place in pDataBuff.
	unsigned int pDataBuffLen = buffLen - sizeof(data_id) - sizeof(type_id) - sizeof(header) - sizeof(Checksum);
	char* pDataBuff = malloc(pDataBuffLen);
	if (pDataBuff == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(pDataBuff, &buff[index], pDataBuffLen);
	index += pDataBuffLen;

	Send_Command_Ack(data_id);

	//Call the correct callbacks based on data id with pDataBuff.
	int err = NO_DCS_ERROR;
	switch (data_id) {
		case GET_DCS_STATUS:
			Process_DCS_Status();
			break;

		case SET_CORRELATOR_SETTING:
			Process_Corr_Set(pDataBuff);
			break;

		case GET_CORRELATOR_SETTING:
			Process_Corr_Status();
			break;

		case SET_ANALYZER_SETTING:
			Process_Analyzer_Set(pDataBuff);
			break;

		case GET_ANALYZER_SETTING:
			Process_Analyzer_Status();
			break;

		case START_MEASUREMENT:
			Process_Start_DCS(pDataBuff);
			break;

		case STOP_MEASUREMENT:
			Process_Stop_DCS();
			break;

		case ENABLE_CORR_ANALYZER:
			Process_Enable_DCS(pDataBuff);
			break;

		case GET_SIMULATED_DATA:
			Process_Simulated_Correlation();
			break;

		case SET_ANALYZER_PREFIT_PARAM:
			Process_Analyzer_Prefit(pDataBuff);
			break;

		case GET_ANALYZER_PREFIT_PARAM:
			Process_Get_Analyzer_Prefit(pDataBuff);
			break;

		case SET_OPTICAL_PARAM:
			Process_Optical_Set(pDataBuff);
			break;

		case CHECK_NET_CONNECTION:
			//Nothing to do here
			break;

		default:
			printf("ERROR: Invalid Command Received");
			return FRAME_INVALID_DATA;
	}

	free(pDataBuff);

	return NO_DCS_ERROR;
}

//Computes checksum from given buffer of given size
Checksum compute_checksum(char* pDataBuf, unsigned int size) {
	Checksum xor_sum = 0;
	for (size_t index = 0; index < size; index++) {
		xor_sum ^= pDataBuf[index];
	}

	return xor_sum;
}

//Checks given checksum from a full DCS frame
//Returns true if checksum is valid
bool check_checksum(char* pDataBuf, size_t size) {
	Checksum xor_sum = 0;
	for (size_t index = 0; index < size; index++) {
		xor_sum ^= pDataBuf[index];
	}

	return xor_sum == 0x00;
}

int Handle_Measurement() {
	static double last_Measurement_Time = 0.0;

	Measurement_Status status;
	bool bCorrOut;
	bool bAnalyzerOut;

	int result = Get_Measurement_Status(&status);
	if (result != NO_DCS_ERROR) {
		return result;
	}

	result = Get_Measurement_Output_Data(&bCorrOut, &bAnalyzerOut);
	if (result != NO_DCS_ERROR) {
		return result;
	}

	if (status.measurement_going) {
		const clock_t currTime = clock();
		const double currTimeSec = currTime / CLOCKS_PER_SEC;

		if ((currTimeSec - last_Measurement_Time) >= status.interval * 10 / 1000) {
			int result = NO_DCS_ERROR;
			if (bCorrOut) {
#pragma warning (disable: 6386 6385 6001)
				const unsigned __int8 delayAndCorrBufLen = 48;

				//Generate fake data
				Corr_Intensity_Data* arr = malloc(sizeof(*arr) * status.Cha_Num);
				if (arr == NULL) {
					return MEMORY_ALLOCATION_ERROR;
				}
				for (int x = 0; x < status.Cha_Num; x++) {
					arr[x].Data_Num = delayAndCorrBufLen;
					arr[x].pCorrBuf = malloc(delayAndCorrBufLen * sizeof(*arr[x].pCorrBuf));
					if (arr[x].pCorrBuf == NULL) {
						for (int y = 0; y < x; x++) {
							free(arr[x].pCorrBuf);
						}
						free(arr);
						return MEMORY_ALLOCATION_ERROR;
					}
				}

				float* delays = malloc(delayAndCorrBufLen * sizeof(*delays));
				if (delays == NULL) {
					for (int x = 0; x < status.Cha_Num; x++) {
						free(arr[x].pCorrBuf);
					}
					free(arr);
					return MEMORY_ALLOCATION_ERROR;
				}

				result = gen_corr_intensity_data(status.ids, status.Cha_Num, arr, delays, delayAndCorrBufLen);
				if (result != NO_DCS_ERROR) {
					for (int x = 0; x < status.Cha_Num; x++) {
						free(arr[x].pCorrBuf);
					}
					free(arr);
					free(delays);
					return result;
				}

				Send_Corr_Intensity_Data(arr, status.Cha_Num, delays, delayAndCorrBufLen);

				for (int x = 0; x < status.Cha_Num; x++) {
					free(arr[x].pCorrBuf);
				}
				free(arr);
				free(delays);
#pragma warning (default: 6386 6385 6001)
			}
			else {
				//Generate fake data
				Intensity_Data* arr = malloc(sizeof(*arr) * status.Cha_Num);
				result = gen_intensity_data(status.ids, status.Cha_Num, arr);
				if (result != NO_DCS_ERROR) {
					free(arr);
					return result;
				}

				Send_Intensity_Data(arr, status.Cha_Num);
				free(arr);
			}

			if (bAnalyzerOut) {
				//Generate fake data
				BFI_Data* arr = malloc(sizeof(*arr) * status.Cha_Num);
				result = gen_bfi_data(status.ids, status.Cha_Num, arr);
				if (result != NO_DCS_ERROR) {
					free(arr);
					return result;
				}

				Send_BFI_Data(arr, status.Cha_Num);
				free(arr);
			}

			last_Measurement_Time = currTimeSec;
		}
	}
	return NO_DCS_ERROR;
}

static int Send_Intensity_Data(Intensity_Data* dataArray, unsigned __int32 arrLength) {
	const unsigned int to_send_data_size = sizeof(arrLength) + arrLength * sizeof(*dataArray);
	char* to_send_data = malloc(to_send_data_size);
	if (to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	size_t index = 0;
	memcpy(&to_send_data[index], &arrLength, sizeof(arrLength));
	index += sizeof(arrLength);

	for (unsigned int x = 0; x < arrLength; x++) {
		int netCha_Id = htool(dataArray[x].Cha_ID);
		memcpy(&to_send_data[index], &netCha_Id, sizeof(netCha_Id));
		index += sizeof(netCha_Id);

		float netIntensity = htoof(dataArray[x].intensity);
		memcpy(&to_send_data[index], &netIntensity, sizeof(netIntensity));
		index += sizeof(netIntensity);
	}

	int result = Send_DCS_Data(GET_INTENSITY, to_send_data, to_send_data_size);
	free(to_send_data);

	return result;
}

static int Send_BFI_Data(BFI_Data* dataArray, unsigned __int32 arrLength) {
	const unsigned int to_send_data_size = sizeof(arrLength) + arrLength * sizeof(*dataArray);
	char* to_send_data = malloc(to_send_data_size);
	if (to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	size_t index = 0;
	memcpy(&to_send_data[index], &arrLength, sizeof(arrLength));
	index += sizeof(arrLength);

	for (unsigned int x = 0; x < arrLength; x++) {
		int netCha_Id = htool(dataArray[x].Cha_ID);
		memcpy(&to_send_data[index], &netCha_Id, sizeof(netCha_Id));
		index += sizeof(netCha_Id);

		float netBFI = htoof(dataArray[x].BFI);
		memcpy(&to_send_data[index], &netBFI, sizeof(netBFI));
		index += sizeof(netBFI);

		float netBeta = htoof(dataArray[x].Beta);
		memcpy(&to_send_data[index], &netBeta, sizeof(netBeta));
		index += sizeof(netBeta);

		float netrMSE = htoof(dataArray[x].rMSE);
		memcpy(&to_send_data[index], &netrMSE, sizeof(netrMSE));
		index += sizeof(netrMSE);
	}

	int result = Send_DCS_Data(GET_BFI_DATA, to_send_data, to_send_data_size);
	free(to_send_data);

	return result;
}

static int Send_Corr_Intensity_Data(Corr_Intensity_Data* dataArray, unsigned __int32 arrLength, float* delays, unsigned __int32 delay_Num) {
	unsigned int corrValSize = 0;

	//Calculate size of nested correlation values
	for (unsigned int x = 0; x < arrLength; x++) {
		corrValSize += dataArray[x].Data_Num * sizeof(*dataArray[x].pCorrBuf);
	}

	const unsigned int to_send_data_size = sizeof(arrLength) + arrLength * (sizeof(dataArray->Cha_ID) + sizeof(dataArray->intensity) + sizeof(dataArray->Data_Num)) + corrValSize + sizeof(delay_Num) + delay_Num * sizeof(*delays);
	char* to_send_data = malloc(to_send_data_size);
	if (to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	size_t index = 0;
	memcpy(&to_send_data[index], &arrLength, sizeof(arrLength));
	index += sizeof(arrLength);

	for (unsigned int x = 0; x < arrLength; x++) {
		int netCha_Id = htool(dataArray[x].Cha_ID);
		memcpy(&to_send_data[index], &netCha_Id, sizeof(netCha_Id));
		index += sizeof(netCha_Id);

		float netIntensity = htoof(dataArray[x].intensity);
		memcpy(&to_send_data[index], &netIntensity, sizeof(netIntensity));
		index += sizeof(netIntensity);

		int netDataN = htool(dataArray[x].Data_Num);
		memcpy(&to_send_data[index], &netDataN, sizeof(netDataN));
		index += sizeof(netDataN);

		for (int y = 0; y < dataArray[x].Data_Num; y++) {
			float netCorr = htoof(dataArray[x].pCorrBuf[y]);
			memcpy(&to_send_data[index], &netCorr, sizeof(netCorr));
			index += sizeof(netCorr);
		}
	}

	//Add delays
	int netDelayNum = htool(delay_Num);
	memcpy(&to_send_data[index], &netDelayNum, sizeof(netDelayNum));
	index += sizeof(netDelayNum);

	for (unsigned int x = 0; x < delay_Num; x++) {
		float netDelay = htoof(delays[x]);
		memcpy(&to_send_data[index], &netDelay, sizeof(netDelay));
		index += sizeof(netDelay);
	}

	int result = Send_DCS_Data(GET_CORR_INTENSITY, to_send_data, to_send_data_size);
	free(to_send_data);

	return result;
}

static int Process_DCS_Status() {
	DCS_Status status;
	Get_DCS_Status_Data(&status);

	bool bCorr = status.bCorr;
	bool bAnalyzer = status.bAnalyzer;
	int DCS_Cha_Num = status.DCS_Cha_Num;

	size_t index = 0;

	const unsigned int to_send_data_size = sizeof(bCorr) + sizeof(bAnalyzer) + sizeof(DCS_Cha_Num);
	char* to_send_data = malloc(to_send_data_size);
	if (to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(&to_send_data[index], &bCorr, sizeof(bCorr));
	index += sizeof(bCorr);

	memcpy(&to_send_data[index], &bAnalyzer, sizeof(bAnalyzer));
	index += sizeof(bAnalyzer);

	DCS_Cha_Num = htool(DCS_Cha_Num);
	memcpy(&to_send_data[index], &DCS_Cha_Num, sizeof(DCS_Cha_Num));

	int result = Send_DCS_Data(GET_DCS_STATUS, to_send_data, to_send_data_size);
	free(to_send_data);

	return result;
}

static int Process_Corr_Set(char* buff) {
	int Data_N;
	int Scale;
	int Sample_Size;

	unsigned int index = 0;
	memcpy(&Data_N, &buff[index], sizeof(Data_N));
	index += sizeof(Data_N);

	memcpy(&Scale, &buff[index], sizeof(Scale));
	index += sizeof(Scale);

	memcpy(&Sample_Size, &buff[index], sizeof(Sample_Size));
	index += sizeof(Sample_Size);

	//Change network endianess to host
	Data_N = itohl(Data_N);
	Scale = itohl(Scale);
	Sample_Size = itohl(Sample_Size);

	//printf("Setting Correlator Params:\nData_N: %d\nScale: %d\nCorr_Time: %d\n", Data_N, Scale, Corr_Time);
	const Correlator_Setting setting = {
		.Data_N = Data_N,
		.Corr_Time = (float)2e-7 * Sample_Size * Data_N,
		.Scale = Scale,
	};
	Set_Correlator_Setting_Data(setting);

	return NO_DCS_ERROR;
}

static int Process_Corr_Status() {
	Correlator_Setting setting;
	Get_Correlator_Setting_Data(&setting);

	int Data_N = setting.Data_N;
	int Scale = setting.Scale;
	int Sample_Size = (int) ceil(setting.Corr_Time / setting.Data_N / 200e-9);

	size_t index = 0;

	const unsigned int to_send_data_size = sizeof(Data_N) + sizeof(Scale) + sizeof(Sample_Size);
	char* to_send_data = malloc(to_send_data_size);
	if (to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	Data_N = htool(Data_N);
	memcpy(&to_send_data[index], &Data_N, sizeof(Data_N));
	index += sizeof(Data_N);

	Scale = htool(Scale);
	memcpy(&to_send_data[index], &Scale, sizeof(Scale));
	index += sizeof(Scale);

	Sample_Size = htool(Sample_Size);
	memcpy(&to_send_data[index], &Sample_Size, sizeof(Sample_Size));

	int result = Send_DCS_Data(GET_CORRELATOR_SETTING, to_send_data, to_send_data_size);
	free(to_send_data);

	return result;
}

static int Process_Analyzer_Set(char* buff) {
	Analyzer_Setting* settings;
	int Cha_Num;

	//Copy prepended number of channels to local
	memcpy(&Cha_Num, &buff[0], sizeof(Cha_Num));
	Cha_Num = itohl(Cha_Num);

	settings = malloc(sizeof(*settings) * Cha_Num);
	if (settings == NULL) {
		return MEMORY_ALLOCATION_ALIGNMENT;
	}

#pragma warning (disable: 6386 6385)
	for (int x = 0; x < Cha_Num; x++) {
		memcpy(&settings[x], &buff[sizeof(Cha_Num) + sizeof(*settings) * x], sizeof(*settings));

		//Change network endianess to host
		settings[x].Alpha = itohf(settings[x].Alpha);
		settings[x].Beta = itohf(settings[x].Beta);
		settings[x].Db = itohf(settings[x].Db);
		settings[x].Distance = itohf(settings[x].Distance);
		settings[x].mua0 = itohf(settings[x].mua0);
		settings[x].musp = itohf(settings[x].musp);
		settings[x].Wavelength = itohf(settings[x].Wavelength);
	}
#pragma warning (default: 6386 6385)

	Set_Analyzer_Setting_Data(settings, Cha_Num);

	free(settings);

	return NO_DCS_ERROR;
}

static int Process_Analyzer_Status() {
	Analyzer_Setting* data;
	int Cha_Num;
	Get_Analyzer_Setting_Data(&data, &Cha_Num);

	size_t index = 0;

	const unsigned int to_send_data_size = sizeof(Cha_Num) + sizeof(Analyzer_Setting) * Cha_Num;
	char* to_send_data = malloc(to_send_data_size);
	if (to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	unsigned int net_Cha_Num = htool(Cha_Num);
	memcpy(&to_send_data[index], &net_Cha_Num, sizeof(net_Cha_Num));
	index += sizeof(net_Cha_Num);

	//Change to network endianess
	for (int x = 0; x < Cha_Num; x++) {
		data[x].Alpha = htoof(data[x].Alpha);
		data[x].Beta = htoof(data[x].Beta);
		data[x].Db = htoof(data[x].Db);
		data[x].Distance = htoof(data[x].Distance);
		data[x].mua0 = htoof(data[x].mua0);
		data[x].musp = htoof(data[x].musp);
		data[x].Wavelength = htoof(data[x].Wavelength);
	}

	memcpy(&to_send_data[index], data, sizeof(*data) * Cha_Num);
	free(data);

	int result = Send_DCS_Data(GET_ANALYZER_SETTING, to_send_data, to_send_data_size);
	free(to_send_data);

	return result;
}

static int Process_Start_DCS(char* buff) {
	int Interval;
	int* pCha_IDs;
	int Cha_Num;

	unsigned int index = 0;
	memcpy(&Interval, &buff[index], sizeof(Interval));
	Interval = itohl(Interval);
	index += sizeof(Interval);

	memcpy(&Cha_Num, &buff[index], sizeof(Cha_Num));
	Cha_Num = itohl(Cha_Num);
	index += sizeof(Cha_Num);

	pCha_IDs = malloc(sizeof(*pCha_IDs) * Cha_Num);
	if (pCha_IDs == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}
	memcpy(pCha_IDs, &buff[index], sizeof(*pCha_IDs) * Cha_Num);
	index += sizeof(*pCha_IDs) * Cha_Num;

	//Change to network to host endian
	for (int x = 0; x < Cha_Num; x++) {
#pragma warning (disable: 6386)
		pCha_IDs[x] = itohl(pCha_IDs[x]);
#pragma warning (default: 6386)
	}

	Start_Measurement(Interval, Cha_Num, pCha_IDs);

	free(pCha_IDs);

	return NO_DCS_ERROR;
}

static int Process_Stop_DCS() {
	Stop_Measurement();

	return NO_DCS_ERROR;
}

static int Process_Enable_DCS(char* buff) {
	bool bCorr;
	bool bAnalyzer;

	unsigned int index = 0;

	memcpy(&bAnalyzer, &buff[index], sizeof(bAnalyzer));
	index += sizeof(bAnalyzer);

	memcpy(&bCorr, &buff[index], sizeof(bCorr));
	index += sizeof(bCorr);

	Set_Measurement_Output_Data(bCorr, bAnalyzer);

	return NO_DCS_ERROR;
}

static int Process_Simulated_Correlation() {
	int Precut;
	int Cha_ID;
	int Data_Num;
	float* pCorrBuf;

	//Fake values for testing
	Precut = 5;
	Cha_ID = 2;
	Data_Num = 3;

	pCorrBuf = malloc(sizeof(*pCorrBuf) * Data_Num);
	if (pCorrBuf == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	for (int x = 0; x < Data_Num; x++) {
		pCorrBuf[x] = (float)(x + 0.2);
	}

	//Copy values to buffer
	size_t index = 0;

	unsigned int to_send_data_size = sizeof(Precut) + sizeof(Cha_ID) + sizeof(Data_Num) + sizeof(*pCorrBuf) * Data_Num;
	char* to_send_data = malloc(to_send_data_size);
	if (to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	Precut = htool(Precut);
	memcpy(&to_send_data[index], &Precut, sizeof(Precut));
	index += sizeof(Precut);

	Cha_ID = htool(Cha_ID);
	memcpy(&to_send_data[index], &Cha_ID, sizeof(Cha_ID));
	index += sizeof(Cha_ID);

	unsigned int net_Data_Num = htool(Data_Num);
	memcpy(&to_send_data[index], &net_Data_Num, sizeof(net_Data_Num));
	index += sizeof(net_Data_Num);

	for (int x = 0; x < Data_Num; x++) {
		pCorrBuf[x] = htoof(pCorrBuf[x]);
	}

	memcpy(&to_send_data[index], pCorrBuf, sizeof(*pCorrBuf) * Data_Num);
	free(pCorrBuf);

	int result = Send_DCS_Data(GET_SIMULATED_DATA, to_send_data, to_send_data_size);
	free(to_send_data);

	return result;
}

static int Process_Optical_Set(char* buff) {
	Optical_Param_Type* param;
	int Cha_Num;

	//Copy prepended number of channels to local
	memcpy(&Cha_Num, &buff[0], sizeof(Cha_Num));
	Cha_Num = itohl(Cha_Num);

	param = malloc(sizeof(*param) * Cha_Num);
	if (param == NULL) {
		return MEMORY_ALLOCATION_ALIGNMENT;
	}

#pragma warning (disable: 6386 6385)
	for (int x = 0; x < Cha_Num; x++) {
		memcpy(&param[x], &buff[sizeof(Cha_Num) + sizeof(*param) * x], sizeof(*param));

		//Change network endianess to host
		param[x].Cha_ID = itohl((u_long)param[x].Cha_ID);
		param[x].mua0 = itohf(param[x].mua0);
		param[x].musp = itohf(param[x].musp);
	}
#pragma warning (default: 6386 6385)

	Set_Optical_Param_Data(param, Cha_Num);

	free(param);

	return NO_DCS_ERROR;
}

static int Process_Analyzer_Prefit(char* buff) {
	Analyzer_Prefit_Param prefit;

#pragma warning (disable: 6386 6385)
	memcpy(&prefit, buff, sizeof(prefit));

	//Change network endianess to host
	prefit.Precut = itohl((u_long)prefit.Precut);
	prefit.PostCut = itohl((u_long)prefit.PostCut);
	prefit.Min_Intensity = itohf(prefit.Min_Intensity);
	prefit.Max_Intensity = itohf(prefit.Max_Intensity);
	prefit.FitLimt = itohf(prefit.FitLimt);
	prefit.lightLeakage = itohf(prefit.lightLeakage);
	prefit.earlyLeakage = itohf(prefit.earlyLeakage);
#pragma warning (default: 6386 6385)

	Set_Analyzer_Prefit_Param_Data(prefit);

	return NO_DCS_ERROR;
}

static int Process_Get_Analyzer_Prefit() {
	Analyzer_Prefit_Param data;
	Get_Analyzer_Prefit_Param_Data(&data);

	const unsigned int to_send_data_size = sizeof(data);
	char* to_send_data = malloc(to_send_data_size);
	if (to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	//Change to network endianess
	data.Precut = htool(data.Precut);
	data.PostCut = htool(data.PostCut);

	data.Min_Intensity = htoof(data.Min_Intensity);
	data.Max_Intensity = htoof(data.Max_Intensity);
	data.FitLimt = htoof(data.FitLimt);
	data.lightLeakage = htoof(data.lightLeakage);
	data.earlyLeakage = htoof(data.earlyLeakage);

	memcpy(to_send_data, &data, sizeof(data));

	int result = Send_DCS_Data(GET_ANALYZER_PREFIT_PARAM, to_send_data, to_send_data_size);
	free(to_send_data);

	return result;
}

static int Send_Command_Ack(Data_ID id) {
	Data_ID networkID = htool(id);
	return Send_DCS_Data(COMMAND_ACK, (char*) &id, sizeof(id));
}

int Send_DCS_Message(const char* message) {
	const size_t message_len = strlen(message);

	const unsigned __int32 to_send_data_size = (unsigned int) message_len + sizeof(to_send_data_size);
	char* to_send_data = malloc(to_send_data_size);
	if (to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	const unsigned __int32 prependSize = htool((u_long)message_len);

	size_t index = 0;

	memcpy(&to_send_data[index], &prependSize, sizeof(prependSize));
	index += sizeof(prependSize);

	memcpy(&to_send_data[index], message, message_len);
	index += message_len;

	return Send_DCS_Data(GET_ERROR_MESSAGE, to_send_data, to_send_data_size);
}

int Send_DCS_Error(const char* message, unsigned int code) {
	if (code > 9999) {
		return 1;
	}

	char error[100];

	_snprintf_s(error, sizeof(error), _TRUNCATE, "Error (%04u): %s", code, message);

	return Send_DCS_Message(error);
}

static int Send_DCS_Data(Data_ID data_ID, char* pDataBuf, const unsigned __int32 BufferSize) {
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
	const Type_ID command = htool(DATA_ID);
	memcpy(&pTransmission->pFrame[index], &command, sizeof(command));
	index += sizeof(command);

	//Change data ID to output byte order and copy to output buffer.
	const Data_ID data_ID_out = htool(data_ID);
	memcpy(&pTransmission->pFrame[index], &data_ID, sizeof(data_ID));
	index += sizeof(data_ID);

	//Copy main data to output buffer.
	if (pDataBuf != NULL) {
		memcpy(&pTransmission->pFrame[index], pDataBuf, BufferSize);
		index += BufferSize;
	}

	//Add checksum calculated from 2(Header) + 4(Type ID) + 4(Data ID) + BufferSize
	pTransmission->pFrame[pTransmission->size - 1] = compute_checksum(pTransmission->pFrame, pTransmission->size - 1);

	pTransmission->command_code = data_ID;

	int result = Enqueue_Trans_FIFO(pTransmission);
	return result;
}

////////////////////////////////
//Endianess management functions
////////////////////////////////
static inline bool should_swap_htoo() {
	if (!IS_BIG_ENDIAN && ENDIANESS_OUTPUT == BIG_ENDIAN) {
		return true;
	}
	if (IS_BIG_ENDIAN && ENDIANESS_OUTPUT == LITTLE_ENDIAN) {
		return true;
	}
	return false;
}

static inline bool should_swap_itoh() {
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