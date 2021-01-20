#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <WinSock2.h>

#include "Internal.h"

static int write_DCS_header(char** buff, unsigned int* to_send_data_size);
static int write_command_ack_resp(char** to_send_data, unsigned int* to_send_data_size, Data_ID command_being_responded);
static int Process_DCS_Status(char** to_send_data, unsigned int* to_send_data_size);
static int Process_Corr_Set(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size);
static int Process_Corr_Status(char** to_send_data, unsigned int* to_send_data_size);
static int Process_Analyzer_Set(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size);
static int Process_Analyzer_Status(char** to_send_data, unsigned int* to_send_data_size);
static int Process_Start_DCS(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size);
static int Process_Stop_DCS(char** to_send_data, unsigned int* to_send_data_size);
static int Process_Enable_DCS(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size);
static int Process_Simulated_Correlation(char** to_send_data, unsigned int* to_send_data_size);
static int Process_Optical_Set(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size);
static int Process_Analyzer_Prefit(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size);
static int Process_Get_Analyzer_Prefit(char** to_send_data, unsigned int* to_send_data_size);

int process_recv(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size) {
	//Verify checksum
	unsigned __int8 received_checksum;

	//Copy checksum from frame
	memcpy(&received_checksum, &received_data[received_data_size - 1], sizeof(received_checksum));
	if (!check_checksum(received_data, received_data_size)) {
		return FRAME_CHECKSUM_ERROR;
	}

	//Ensure header is correct
	unsigned __int16 header;
	memcpy(&header, &received_data[0], HEADER_SIZE);
	header = ntohs(header);
	if (header != FRAME_VERSION) {
		return FRAME_VERSION_ERROR;
	}


	//Ensure type id is correct
	unsigned __int32 type_id;
	memcpy(&type_id, &received_data[2], TYPE_ID_SIZE);
	type_id = ntohl(type_id);
	if (type_id != COMMAND_ID) {
		return FRAME_INVALID_DATA;
	}


	//Return correct data based on command id
	Data_ID command_id;
	memcpy(&command_id, &received_data[6], DATA_ID_SIZE);
	command_id = ntohl(command_id);

	char* dcs_header = NULL;
	unsigned int header_size = 0;;
	write_DCS_header(&dcs_header, &header_size);

	char* main_frame_data = NULL;
	unsigned int main_frame_data_size = 0;

	char* main_recv_data = &received_data[10];
	unsigned int main_recv_data_size = received_data_size - DATA_ID_SIZE - TYPE_ID_SIZE - HEADER_SIZE - CHECKSUM_SIZE;
	switch (command_id) {
	case GET_DCS_STATUS:
		Process_DCS_Status(&main_frame_data, &main_frame_data_size);
		break;

	case SET_CORRELATOR_SETTING:
		Process_Corr_Set(main_recv_data, main_recv_data_size, &main_frame_data, &main_frame_data_size);
		break;

	case GET_CORRELATOR_SETTING:
		Process_Corr_Status(&main_frame_data, &main_frame_data_size);
		break;

	case SET_ANALYZER_SETTING:
		Process_Analyzer_Set(main_recv_data, main_recv_data_size, &main_frame_data, &main_frame_data_size);
		break;

	case GET_ANALYZER_SETTING:
		Process_Analyzer_Status(&main_frame_data, &main_frame_data_size);
		break;

	case START_MEASUREMENT:
		Process_Start_DCS(main_recv_data, main_recv_data_size, &main_frame_data, &main_frame_data_size);
		break;

	case STOP_MEASUREMENT:
		Process_Stop_DCS(&main_frame_data, &main_frame_data_size);
		break;

	case ENABLE_CORR_ANALYZER:
		Process_Enable_DCS(main_recv_data, main_recv_data_size, &main_frame_data, &main_frame_data_size);
		break;

	case GET_SIMULATED_DATA:
		Process_Simulated_Correlation(&main_frame_data, &main_frame_data_size);
		break;

	case SET_ANALYZER_PREFIT_PARAM:
		Process_Analyzer_Prefit(main_recv_data, main_recv_data_size, &main_frame_data, &main_frame_data_size);
		break;

	case GET_ANALYZER_PREFIT_PARAM:
		Process_Get_Analyzer_Prefit(&main_frame_data, &main_frame_data_size);
		break;

	case SET_OPTICAL_PARAM:
		Process_Optical_Set(main_recv_data, main_recv_data_size, &main_frame_data, &main_frame_data_size);
		break;

	case CHECK_NET_CONNECTION:

		break;

	//case GET_ERROR_MESSAGE:

	//    break;

	case STOP_DCS:

		break;

	default:
		printf("ERROR: Invalid Command Received");
		return FRAME_INVALID_DATA;
	}

	//Allocate memory for header, main data, and 1-byte checksum
	*to_send_data_size = header_size + main_frame_data_size + 1;
	*to_send_data = malloc(*to_send_data_size);
	if (*to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(&(*to_send_data)[0], dcs_header, header_size);
	memcpy(&(*to_send_data)[header_size], main_frame_data, main_frame_data_size);
	free(main_frame_data);

	unsigned __int8 checksum = compute_checksum(*to_send_data, header_size + main_frame_data_size);
	memcpy(&(*to_send_data)[*to_send_data_size - 1], &checksum, sizeof(checksum));

	return NO_DCS_ERROR;
}

//Computes checksum from given buffer of given size
unsigned __int8 compute_checksum(char* pDataBuf, unsigned int size) {
	unsigned __int8 xor_sum = 0;
	for (size_t index = 0; index < size; index++) {
		xor_sum ^= pDataBuf[index];
	}

	return xor_sum;
}

//Checks given checksum from a full DCS frame
//Returns true if checksum is valid
bool check_checksum(char* pDataBuf, size_t size) {
	unsigned __int8 xor_sum = 0;
	for (size_t index = 0; index < size; index++) {
		xor_sum ^= pDataBuf[index];
	}

	return xor_sum == 0x00;
}

//Allocates and writes DCS status to_send_data buffer and outputs size to_send_data_size
static int Process_DCS_Status(char** to_send_data, unsigned int* to_send_data_size) {
	bool bCorr; // TRUE if correlator is started, FALSE if the correlator is not started.
	bool bAnalyzer; // TRUE if analyzer is started, FALSE if the analyzer is not started.
	int DCS_Cha_Num; // number of total DCS channels on the remote DCS.

	//Fake testing values
	bCorr = true;
	bAnalyzer = false;
	DCS_Cha_Num = 3;

	Data_ID data_id = GET_DCS_STATUS;
	size_t index = 0;

	*to_send_data_size = sizeof(bCorr) + sizeof(bAnalyzer) + sizeof(DCS_Cha_Num) + sizeof(data_id);
	*to_send_data = malloc(*to_send_data_size);
	if (*to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	data_id = htonl(data_id);
	memcpy(&(*to_send_data)[index], &data_id, sizeof(data_id));
	index += sizeof(data_id);

	memcpy(&(*to_send_data)[index], &bCorr, sizeof(bCorr));
	index += sizeof(bCorr);

	memcpy(&(*to_send_data)[index], &bAnalyzer, sizeof(bAnalyzer));
	index += sizeof(bAnalyzer);

	DCS_Cha_Num = htonl(DCS_Cha_Num);
	memcpy(&(*to_send_data)[index], &DCS_Cha_Num, sizeof(DCS_Cha_Num));

	return NO_DCS_ERROR;
}

static int Process_Corr_Set(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size) {
	int Data_N;
	int Scale;
	int Corr_Time;
	memcpy(&Data_N, &received_data[0], sizeof(Data_N));
	memcpy(&Scale, &received_data[4], sizeof(Scale));
	memcpy(&Corr_Time, &received_data[8], sizeof(Corr_Time));

	//Change network endianess to host
	Data_N = ntohl(Data_N);
	Scale = ntohl(Scale);
	Corr_Time = ntohl(Corr_Time);

	printf("Setting Correlator Params:\nData_N: %d\nScale: %d\nCorr_Time: %d\n", Data_N, Scale, Corr_Time);

	return write_command_ack_resp(to_send_data, to_send_data_size, SET_CORRELATOR_SETTING);
}

//Allocates and writes Corr settings to_send_data buffer and outputs size to_send_data_size
static int Process_Corr_Status(char** to_send_data, unsigned int* to_send_data_size) {
	int Data_N;
	int Scale;
	int Sample_Size;

	//Fake testing values
	Data_N = 16384;
	Scale = 1;
	Sample_Size = 3;

	Data_ID data_id = GET_CORRELATOR_SETTING;
	size_t index = 0;

	*to_send_data_size = sizeof(Data_N) + sizeof(Scale) + sizeof(Sample_Size) + sizeof(data_id);
	*to_send_data = malloc(*to_send_data_size);
	if (*to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	data_id = htonl(data_id);
	memcpy(&(*to_send_data)[index], &data_id, sizeof(data_id));
	index += sizeof(data_id);

	Data_N = htonl(Data_N);
	memcpy(&(*to_send_data)[index], &Data_N, sizeof(Data_N));
	index += sizeof(Data_N);

	Scale = htonl(Scale);
	memcpy(&(*to_send_data)[index], &Scale, sizeof(Scale));
	index += sizeof(Scale);

	Sample_Size = htonl(Sample_Size);
	memcpy(&(*to_send_data)[index], &Sample_Size, sizeof(Sample_Size));

	return NO_DCS_ERROR;
}

static int Process_Analyzer_Set(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size) {
	Analyzer_Setting_Type* settings;
	int Cha_Num;

	//Copy prepended number of channels to local
	memcpy(&Cha_Num, &received_data[0], sizeof(Cha_Num));
	Cha_Num = ntohl(Cha_Num);

	settings = malloc(sizeof(*settings) * Cha_Num);
	if (settings == NULL) {
		return MEMORY_ALLOCATION_ALIGNMENT;
	}

#pragma warning (disable: 6386 6385)
	for (int x = 0; x < Cha_Num; x++) {
		memcpy(&settings[x], &received_data[sizeof(Cha_Num) + sizeof(*settings) * x], sizeof(*settings));

		//Change network endianess to host
		settings[x].Alpha = ntohf(*(unsigned int*)&settings[x].Alpha);
		settings[x].Beta = ntohf(*(unsigned int*)&settings[x].Beta);
		settings[x].Db = ntohf(*(unsigned int*)&settings[x].Db);
		settings[x].Distance = ntohf(*(unsigned int*)&settings[x].Distance);
		settings[x].mua0 = ntohf(*(unsigned int*)&settings[x].mua0);
		settings[x].musp = ntohf(*(unsigned int*)&settings[x].musp);
		settings[x].Wavelength = ntohf(*(unsigned int*)&settings[x].Wavelength);
	}
#pragma warning (default: 6386 6385)

	//Print received settings for testing purposes
	for (int x = 0; x < Cha_Num; x++) {
		printf("Settings %d\n", x);
		printf("Alpha: %f\n", settings[x].Alpha);
		printf("Beta: %f\n", settings[x].Beta);
		printf("Db: %f\n", settings[x].Db);
		printf("Distance: %f\n", settings[x].Distance);
		printf("mua0: %f\n", settings[x].mua0);
		printf("musp: %f\n", settings[x].musp);
		printf("Wavelength: %f\n\n", settings[x].Wavelength);
	}

	free(settings);

	return write_command_ack_resp(to_send_data, to_send_data_size, SET_ANALYZER_SETTING);
}

//Allocates and writes Analyzer settings to_send_data buffer and outputs size to_send_data_size
static int Process_Analyzer_Status(char** to_send_data, unsigned int* to_send_data_size) {
	int Cha_Num = 5; //Fake cha num for testing
	Analyzer_Setting_Type* data;

	//Make fake analyzer setting data for testing
	data = malloc(sizeof(*data) * Cha_Num);
	if (data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	for (int x = 0; x < Cha_Num; x++) {
		data[x] = (Analyzer_Setting_Type){
			.Alpha = (float) 10.8 - x,
			.Beta = (float) 9.7 - x,
			.Db = (float) 8.6 - x,
			.Distance = (float) 7.4 - x,
			.mua0 = (float) 6.3 - x,
			.musp = (float) 5.2 - x,
			.Wavelength = (float) 4.1 - x,
		};
	}

	Data_ID data_id = GET_ANALYZER_SETTING;
	size_t index = 0;

	*to_send_data_size = sizeof(data_id) + sizeof(Cha_Num) + sizeof(Analyzer_Setting_Type) * Cha_Num;
	*to_send_data = malloc(*to_send_data_size);
	if (*to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	data_id = htonl(data_id);
	memcpy(&(*to_send_data)[index], &data_id, sizeof(data_id));
	index += sizeof(data_id);

	unsigned int net_Cha_Num = htonl(Cha_Num);
	memcpy(&(*to_send_data)[index], &net_Cha_Num, sizeof(net_Cha_Num));
	index += sizeof(net_Cha_Num);

	//Change to network endianess
	for (int x = 0; x < Cha_Num; x++) {
		unsigned int net_Alpha = htonf(data[x].Alpha);
		memcpy(&data[x].Alpha, &net_Alpha, sizeof(net_Alpha));

		unsigned int net_Beta = htonf(data[x].Beta);
		memcpy(&data[x].Beta, &net_Beta, sizeof(net_Beta));

		unsigned int net_Db = htonf(data[x].Db);
		memcpy(&data[x].Db, &net_Db, sizeof(net_Db));

		unsigned int net_Distance = htonf(data[x].Distance);
		memcpy(&data[x].Distance, &net_Distance, sizeof(net_Distance));

		unsigned int net_mua0 = htonf(data[x].mua0);
		memcpy(&data[x].mua0, &net_mua0, sizeof(net_mua0));

		unsigned int net_musp = htonf(data[x].musp);
		memcpy(&data[x].musp, &net_musp, sizeof(net_musp));

		unsigned int net_Wavelength = htonf(data[x].Wavelength);
		memcpy(&data[x].Wavelength, &net_Wavelength, sizeof(net_Wavelength));
	}

	memcpy(&(*to_send_data)[index], data, sizeof(*data) * Cha_Num);
	free(data);

	return NO_DCS_ERROR;
}

static int Process_Start_DCS(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size) {
	int Interval;
	int* pCha_IDs;
	int Cha_Num;

	memcpy(&Interval, &received_data[0], sizeof(Interval));
	Interval = ntohl(Interval);
	memcpy(&Cha_Num, &received_data[4], sizeof(Cha_Num));
	Cha_Num = ntohl(Cha_Num);

	pCha_IDs = malloc(sizeof(*pCha_IDs) * Cha_Num);
	if (pCha_IDs == NULL) {
		return MEMORY_ALLOCATION_ALIGNMENT;
	}
	memcpy(pCha_IDs, &received_data[8], sizeof(*pCha_IDs) * Cha_Num);

	//Change to network to host endian
	for (int x = 0; x < Cha_Num; x++) {
#pragma warning (disable: 6386)
		pCha_IDs[x] = ntohl(pCha_IDs[x]);
#pragma warning (default: 6386)
	}

	//Printing data for testing purposes
	printf("Start DCS\n");
	printf("Interval: %d\n", Interval);
	printf("Cha_Num: %d\n", Cha_Num);
	printf("Ids: [\n");
	for (int x = 0; x < Cha_Num; x++) {
		printf("\t%d\n", pCha_IDs[x]);
	}
	printf("]\n");

	return write_command_ack_resp(to_send_data, to_send_data_size, START_MEASUREMENT);
}

static int Process_Stop_DCS(char** to_send_data, unsigned int* to_send_data_size) {
	printf("Stopping DCS Measurement\n");

	return write_command_ack_resp(to_send_data, to_send_data_size, STOP_MEASUREMENT);
}

static int Process_Enable_DCS(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size) {
	bool bCorr;
	bool bAnalyzer;

	memcpy(&bCorr, &received_data[0], sizeof(bCorr));

	memcpy(&bAnalyzer, &received_data[1], sizeof(bAnalyzer));

	//Printing data for testing purposes
	printf("Enable DCS\n");
	printf("bCorr: %s\n", bCorr ? "true" : "false");
	printf("bAnalyzer: %s\n", bAnalyzer ? "true" : "false");

	return write_command_ack_resp(to_send_data, to_send_data_size, ENABLE_CORR_ANALYZER);
}

static int Process_Simulated_Correlation(char** to_send_data, unsigned int* to_send_data_size) {
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
		pCorrBuf[x] = (float) (x + 0.2);
	}

	//Copy values to buffer
	Data_ID data_id = GET_SIMULATED_DATA;
	size_t index = 0;

	*to_send_data_size = sizeof(data_id) + sizeof(Precut) + sizeof(Cha_ID) + sizeof(Data_Num) + sizeof(*pCorrBuf) * Data_Num;
	*to_send_data = malloc(*to_send_data_size);
	if (*to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	data_id = htonl(data_id);
	memcpy(&(*to_send_data)[index], &data_id, sizeof(data_id));
	index += sizeof(data_id);

	Precut = htonl(Precut);
	memcpy(&(*to_send_data)[index], &Precut, sizeof(Precut));
	index += sizeof(Precut);

	Cha_ID = htonl(Cha_ID);
	memcpy(&(*to_send_data)[index], &Cha_ID, sizeof(Cha_ID));
	index += sizeof(Cha_ID);

	unsigned int net_Data_Num = htonl(Data_Num);
	memcpy(&(*to_send_data)[index], &net_Data_Num, sizeof(net_Data_Num));
	index += sizeof(net_Data_Num);

	for (int x = 0; x < Data_Num; x++) {
		unsigned int net_float = htonf(pCorrBuf[x]);
		memcpy(&pCorrBuf[x], &net_float, sizeof(net_float));
	}

	memcpy(&(*to_send_data)[index], pCorrBuf, sizeof(*pCorrBuf) * Data_Num);
	free(pCorrBuf);

	return NO_DCS_ERROR;
}

static int Process_Optical_Set(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size) {
	Optical_Param_Type* param;
	int Cha_Num;

	//Copy prepended number of channels to local
	memcpy(&Cha_Num, &received_data[0], sizeof(Cha_Num));
	Cha_Num = ntohl(Cha_Num);

	param = malloc(sizeof(*param) * Cha_Num);
	if (param == NULL) {
		return MEMORY_ALLOCATION_ALIGNMENT;
	}

#pragma warning (disable: 6386 6385)
	for (int x = 0; x < Cha_Num; x++) {
		memcpy(&param[x], &received_data[sizeof(Cha_Num) + sizeof(*param) * x], sizeof(*param));

		//Change network endianess to host
		param[x].Cha_ID = ntohl((u_long)param[x].Cha_ID);
		param[x].mua0 = ntohf(*(unsigned int*)&param[x].mua0);
		param[x].musp = ntohf(*(unsigned int*)&param[x].musp);
	}
#pragma warning (default: 6386 6385)

	//Print received settings for testing purposes
	for (int x = 0; x < Cha_Num; x++) {
		printf("Settings %d\n", x);
		printf("Cha_ID: %d\n", param[x].Cha_ID);
		printf("mua0: %f\n", param[x].mua0);
		printf("musp: %f\n", param[x].musp);
	}

	free(param);

	return write_command_ack_resp(to_send_data, to_send_data_size, SET_OPTICAL_PARAM);
}

static int Process_Analyzer_Prefit(char* received_data, unsigned int received_data_size, char** to_send_data, unsigned int* to_send_data_size) {
	Analyzer_Prefit_Param_Type prefit;

#pragma warning (disable: 6386 6385)
	memcpy(&prefit, received_data, sizeof(prefit));

	//Change network endianess to host
	prefit.Precut = ntohl((u_long) prefit.Precut);
	prefit.PostCut = ntohl((u_long) prefit.PostCut);
	prefit.Min_Intensity = ntohf(*(unsigned int*)&prefit.Min_Intensity);
	prefit.Max_Intensity = ntohf(*(unsigned int*)&prefit.Max_Intensity);
	prefit.FitLimt = ntohf(*(unsigned int*)&prefit.FitLimt);
	prefit.lightLeakage = ntohf(*(unsigned int*)&prefit.lightLeakage);
	prefit.earlyLeakage = ntohf(*(unsigned int*)&prefit.earlyLeakage);
#pragma warning (default: 6386 6385)

	//Print received prefit for testing purposes
	printf("Precut: %d\n", prefit.Precut);
	printf("PostCut: %d\n", prefit.PostCut);
	printf("Min_Intensity: %f\n", prefit.Min_Intensity);
	printf("Max_Intensity: %f\n", prefit.Max_Intensity);
	printf("FitLimt: %f\n", prefit.FitLimt);
	printf("lightLeakage: %f\n", prefit.lightLeakage);
	printf("earlyLeakage: %f\n", prefit.earlyLeakage);
	printf("Model: %s\n", prefit.Model ? "true" : "false");

	return write_command_ack_resp(to_send_data, to_send_data_size, SET_ANALYZER_PREFIT_PARAM);
}

//Allocates and writes Analyzer prefit params to_send_data buffer and outputs size to_send_data_size
static int Process_Get_Analyzer_Prefit(char** to_send_data, unsigned int* to_send_data_size) {
	Analyzer_Prefit_Param_Type data;

	//Make fake analyzer prefit data for testing
	data = (Analyzer_Prefit_Param_Type){
		.Precut = 1,
		.PostCut = 2,
		.Min_Intensity = 1.7F,
		.Max_Intensity = 2.5F,
		.FitLimt = 0.65465498498F,
		.lightLeakage = 7.5,
		.earlyLeakage = 8.5F,
		.Model = true,
	};

	Data_ID data_id = GET_ANALYZER_PREFIT_PARAM;

	*to_send_data_size = sizeof(data_id) + sizeof(data);
	*to_send_data = malloc(*to_send_data_size);
	if (*to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	data_id = htonl(data_id);
	memcpy(&(*to_send_data)[0], &data_id, sizeof(data_id));

	//Change to network endianess
	data.Precut = htonl(data.Precut);
	data.PostCut = htonl(data.PostCut);

	unsigned int net_Min_Intensity = htonf(data.Min_Intensity);
	memcpy(&data.Min_Intensity, &net_Min_Intensity, sizeof(net_Min_Intensity));

	unsigned int net_Max_Intensity = htonf(data.Max_Intensity);
	memcpy(&data.Max_Intensity, &net_Max_Intensity, sizeof(net_Max_Intensity));

	unsigned int net_FitLimt = htonf(data.FitLimt);
	memcpy(&data.FitLimt, &net_FitLimt, sizeof(net_FitLimt));

	unsigned int net_lightLeakage = htonf(data.lightLeakage);
	memcpy(&data.lightLeakage, &net_lightLeakage, sizeof(net_lightLeakage));

	unsigned int net_earlyLeakage = htonf(data.earlyLeakage);
	memcpy(&data.earlyLeakage, &net_earlyLeakage, sizeof(net_earlyLeakage));

	memcpy(&(*to_send_data)[4], &data, sizeof(data));

	return NO_DCS_ERROR;
}

static int write_DCS_header(char** buff, unsigned int* header_size) {
	const unsigned __int16 frame_version = htons(FRAME_VERSION);
	const unsigned __int32 command = htonl(DATA_ID);

	//Allocate just the header (2) and type id (4)
	*header_size = sizeof(frame_version) + sizeof(command);

	*buff = malloc(*header_size);
	if (*buff == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	memcpy(&(*buff)[0], &frame_version, sizeof(frame_version));
	memcpy(&(*buff)[sizeof(frame_version)], &command, sizeof(command));

	return NO_DCS_ERROR;
}

//Writes a command acknowledgement frame with the given command_being_responded
static int write_command_ack_resp(char** to_send_data, unsigned int* to_send_data_size, Data_ID command_being_responded) {
	Data_ID data_id = COMMAND_ACK;
	size_t index = 0;

	*to_send_data_size = sizeof(command_being_responded) + sizeof(data_id);
	*to_send_data = malloc(*to_send_data_size);
	if (*to_send_data == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	data_id = htonl(data_id);
	memcpy(&(*to_send_data)[index], &data_id, sizeof(data_id));
	index += sizeof(data_id);

	command_being_responded = htonl(command_being_responded);
	memcpy(&(*to_send_data)[index], &command_being_responded, sizeof(command_being_responded));

	return NO_DCS_ERROR;
}