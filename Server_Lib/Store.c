#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>

#include "Store.h"
#include "Internal.h"
#include "Server_Lib.h"

#define NUM_CHANNELS 6

static HANDLE hStoreMutex;
static inline void set_Store_mutex(void);
static inline void release_Store_mutex(void);

typedef struct Log {
	unsigned __int32 size; //Size of log string
	char* str; //Non-null-terminated log string
	struct Log* pNextItem; //Pointer to the next item in the queue.
} Log;

static Log* log_head = NULL;
static Log* log_tail = NULL;

static DCS_Status status = {
	.bCorr = true,
	.bAnalyzer = true,
	.DCS_Cha_Num = NUM_CHANNELS,
};

int Get_DCS_Status_Data(DCS_Status* output) {
	set_Store_mutex();

	*output = status;

	release_Store_mutex();
	return NO_DCS_ERROR;
}

static Correlator_Setting corr_setting = {
	.Data_N = 32768,
	.Scale = 6,
	.Corr_Time = 0.6095f,
};

int Get_Correlator_Setting_Data(Correlator_Setting* output) {
	set_Store_mutex();

	*output = corr_setting;

	release_Store_mutex();
	return NO_DCS_ERROR;
}

int Set_Correlator_Setting_Data(Correlator_Setting newVal) {
	const unsigned int errCode = 5104;
	char errStr[60] = "Correlator setting error: ";

#pragma warning (disable: 6001)
	// Parameter validation
	if (newVal.Data_N != 16384 && newVal.Data_N != 32768) {
		strcat_s(errStr, sizeof(errStr), "Invalid data size.");

		return Send_DCS_Error(errStr, errCode);
	}
	if (newVal.Scale < 2 || newVal.Scale > 10) {
		strcat_s(errStr, sizeof(errStr), "Invalid scale.");

		return Send_DCS_Error(errStr, errCode);
	}
	if (newVal.Corr_Time <= 0) {
		strcat_s(errStr, sizeof(errStr), "Invalid sample number.");

		return Send_DCS_Error(errStr, errCode);
	}
#pragma warning (default: 6001)

	set_Store_mutex();

	corr_setting = newVal;

	release_Store_mutex();

	Send_DCS_Message("Set Correlator Setting Success");
	Add_Log("Changed Correlator Setting");
	return NO_DCS_ERROR;
}

static Analyzer_Setting analyzer_setting[NUM_CHANNELS] = {
	{
		.Beta = 0.5f,
		.Alpha = 0.00001f,
		.Distance = 3.0f,
		.Wavelength = 785.0f,
		.mua0 = 0.1f,
		.musp = 15.0f,
		.Db = 0.00000005f,
	},
	{
		.Beta = 0.5f,
		.Alpha = 0.00001f,
		.Distance = 3.0f,
		.Wavelength = 785.0f,
		.mua0 = 0.1f,
		.musp = 15.0f,
		.Db = 0.00000005f,
	},
	{
		.Beta = 0.5f,
		.Alpha = 0.00001f,
		.Distance = 3.0f,
		.Wavelength = 785.0f,
		.mua0 = 0.1f,
		.musp = 15.0f,
		.Db = 0.00000005f,
	},
	{
		.Beta = 0.5f,
		.Alpha = 0.00001f,
		.Distance = 3.0f,
		.Wavelength = 785.0f,
		.mua0 = 0.1f,
		.musp = 15.0f,
		.Db = 0.00000005f,
	},
	{
		.Beta = 0.5f,
		.Alpha = 0.00001f,
		.Distance = 3.0f,
		.Wavelength = 785.0f,
		.mua0 = 0.1f,
		.musp = 15.0f,
		.Db = 0.00000005f,
	},
	{
		.Beta = 0.5f,
		.Alpha = 0.00001f,
		.Distance = 3.0f,
		.Wavelength = 785.0f,
		.mua0 = 0.1f,
		.musp = 15.0f,
		.Db = 0.00000005f,
	},
};

int Get_Analyzer_Setting_Data(Analyzer_Setting** pAnalyzer_Setting, int* Cha_Num) {
	set_Store_mutex();

	*pAnalyzer_Setting = malloc(sizeof(analyzer_setting));
	if (*pAnalyzer_Setting == NULL) {
		release_Store_mutex();
		return MEMORY_ALLOCATION_ERROR;
	}

	*Cha_Num = sizeof(analyzer_setting) / sizeof(*analyzer_setting);

	memcpy(*pAnalyzer_Setting, analyzer_setting, sizeof(analyzer_setting));

	release_Store_mutex();
	return NO_DCS_ERROR;
}

int Set_Analyzer_Setting_Data(Analyzer_Setting* pAnalyzer_Setting, int Cha_Num) {
	const unsigned int errCode = 5104;

	if (sizeof(analyzer_setting) != sizeof(*pAnalyzer_Setting) * Cha_Num) {
		return Send_DCS_Error("Incorrect number of channels in analyzer set command.", errCode);
	}

	// Parameter validation.
	for (int x = 0; x < Cha_Num; x++) {
		char errStr[120];
		_snprintf_s(errStr, sizeof(errStr), _TRUNCATE, "Analyzer configuration error in detector %d.", x);

		Analyzer_Setting setting = pAnalyzer_Setting[x];
		if (setting.Db < 0.0f || setting.Db > 1E-7f || setting.Beta < 0.0f || setting.Beta > 2.0f || setting.Alpha < 0.0f || setting.Alpha > 1.0f) {
			strcat_s(errStr, sizeof(errStr), " Invalid Beta, Db or error threshold.");

			return Send_DCS_Error(errStr, errCode);
		}

		if (setting.musp < 1.0f || setting.musp > 30.0f) {
			strcat_s(errStr, sizeof(errStr), " Invalid light scattering coefficient.");

			return Send_DCS_Error(errStr, errCode);
		}

		if (setting.mua0 < 0.01f || setting.mua0 > 3.0f) {
			strcat_s(errStr, sizeof(errStr), " Invalid light absorption coefficient.");

			return Send_DCS_Error(errStr, errCode);
		}

		if (setting.Wavelength < 200.0f || setting.Wavelength > 1000.0f) {
			return Send_DCS_Error(errStr, errCode);
		}
	}

	set_Store_mutex();

	memcpy(analyzer_setting, pAnalyzer_Setting, sizeof(analyzer_setting));

	release_Store_mutex();

	Send_DCS_Message("Set Analyzer Setting Success");
	Add_Log("Changed Analyzer Setting");

	return NO_DCS_ERROR;
}

int Set_Optical_Param_Data(Optical_Param_Type* input_arr, int Cha_Num) {
	//Ensure cha_ids are within proper range
	for (int x = 0; x < Cha_Num; x++) {
		if (input_arr[x].Cha_ID < 0 || input_arr[x].Cha_ID >= sizeof(analyzer_setting) / sizeof(*analyzer_setting)) {
			release_Store_mutex();
			Send_DCS_Error("Invalid channel number in optical set command", 5105);
			return 1;
		}
	}

	Analyzer_Setting settingsCopy[sizeof(analyzer_setting) / sizeof(*analyzer_setting)];
	memcpy(settingsCopy, analyzer_setting, sizeof(settingsCopy));

	for (int x = 0; x < Cha_Num; x++) {
		Analyzer_Setting* chaToChange = &settingsCopy[input_arr[x].Cha_ID];

		chaToChange->mua0 = input_arr[x].mua0;
		chaToChange->musp = input_arr[x].musp;
	}

	return Set_Analyzer_Setting_Data(settingsCopy, sizeof(settingsCopy) / sizeof(*settingsCopy));
}

static Analyzer_Prefit_Param analyzer_prefit = {
	.Precut = 5,
	.PostCut = 10,
	.Min_Intensity = 2.0f,
	.Max_Intensity = 3.1415f,
	.FitLimt = 4.5f,
	.lightLeakage = 6.5f,
	.earlyLeakage = 5.5f,
	.Model = false,
};

int Get_Analyzer_Prefit_Param_Data(Analyzer_Prefit_Param* output) {
	set_Store_mutex();

	*output = analyzer_prefit;

	release_Store_mutex();
	return NO_DCS_ERROR;
}

int Set_Analyzer_Prefit_Param_Data(Analyzer_Prefit_Param input) {
	set_Store_mutex();

	analyzer_prefit = input;

	release_Store_mutex();

	Send_DCS_Message("Set Analyzer Prefit Param Success");
	Add_Log("Changed Analyzer Prefit Params");

	return NO_DCS_ERROR;
}

static Measurement_Output measurement_output = {
	.bCorr = false,
	.bAnalyzer = false,
};

int Get_Measurement_Output_Data(bool* bCorr, bool* bAnalyzer) {
	set_Store_mutex();

	*bCorr = measurement_output.bCorr;
	*bAnalyzer = measurement_output.bAnalyzer;

	release_Store_mutex();
	return NO_DCS_ERROR;
}

int Set_Measurement_Output_Data(bool bCorr, bool bAnalyzer) {
	set_Store_mutex();

	measurement_output.bCorr = bCorr;
	measurement_output.bAnalyzer = bAnalyzer;

	release_Store_mutex();

	return NO_DCS_ERROR;
}

static Measurement_Status measurement_status = {
	.measurement_going = false,
	.interval = 0,
	.Cha_Num = 0,
	.ids = { 0 },
};

int Start_Measurement(int interval, int Cha_Num, int* ids) {
	const unsigned int errCode = 5106;
	char errStr[65] = "Measurement parameters error: ";

#pragma warning (disable: 6001)
	// Parameter validation
	if (Cha_Num > NUM_CHANNELS) {
		strcat_s(errStr, sizeof(errStr), "Too many channel IDs.");
		Send_DCS_Error(errStr, errCode);
		return 1;
	}
	if (Cha_Num <= 0) {
		strcat_s(errStr, sizeof(errStr), "Missing channel IDs.");
		Send_DCS_Error(errStr, errCode);
		return 1;
	}
	if (interval <= 0) {
		strcat_s(errStr, sizeof(errStr), "Invalid scan interval.");
		Send_DCS_Error(errStr, errCode);
		return 1;
	}

	for (int x = 0; x < Cha_Num; x++) {
		if (ids[x] >= NUM_CHANNELS || ids[x] < 0) {
			strcat_s(errStr, sizeof(errStr), "Invalid channel ID.");
			Send_DCS_Error(errStr, errCode);
			return 1;
		}
	}
#pragma warning (default: 6001)

	set_Store_mutex();

	measurement_status.measurement_going = true;
	measurement_status.interval = interval;
	measurement_status.Cha_Num = Cha_Num;
	memcpy(measurement_status.ids, ids, sizeof(*ids) * Cha_Num);

	release_Store_mutex();

	Add_Log("Starting Measurement");

	return NO_DCS_ERROR;
}

int Stop_Measurement() {
	set_Store_mutex();

	measurement_status.measurement_going = false;

	release_Store_mutex();

	Add_Log("Stopping Measurement");

	return NO_DCS_ERROR;
}

int Get_Measurement_Status(Measurement_Status* status) {
	set_Store_mutex();

	*status = measurement_status;

	release_Store_mutex();

	return NO_DCS_ERROR;
}

int Add_Log(const char* log) {
	unsigned __int32 size = (unsigned __int32) strlen(log);
	char* str = malloc(size);
	if (str == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}
	memcpy(str, log, size);

	Log* pLog = malloc(sizeof(*pLog));
	if (pLog == NULL) {
		free(str);
		return MEMORY_ALLOCATION_ERROR;
	}

	*pLog = (Log) {
		.pNextItem = NULL,
		.size = size,
		.str = str,
	};

	set_Store_mutex();

	//If FIFO is empty, this element is both the head and tail.
	if (log_head == NULL) {
		log_head = pLog;
		log_tail = pLog;
	}
	//Otherwise, add to the end of FIFO.
	else {
		log_tail->pNextItem = pLog;
		log_tail = pLog;
	}

	release_Store_mutex();

	return NO_DCS_ERROR;
}

int Get_Logs(char** pMessage, unsigned __int32* length) {
	set_Store_mutex();

	Log* pLog = log_head;

	//If the original FIFO head isn't null, shift the queue.
	if (pLog != NULL) {
		log_head = log_head->pNextItem;
	}

	//If the new FIFO head is null, the queue is empty.
	if (log_head == NULL) {
		log_tail = NULL;
	}

	release_Store_mutex();

	if (pLog == NULL) {
		return 1;
	}

	*pMessage = pLog->str;
	*length = pLog->size;

	free(pLog);

	return NO_DCS_ERROR;
}

int Cleanup_Logs(void) {
	char** message = malloc(sizeof(*message));
	if (message == NULL) {
		return MEMORY_ALLOCATION_ERROR;
	}

	unsigned __int32* messageLength = malloc(sizeof(*messageLength));
	if (messageLength == NULL) {
		free(message);
		return MEMORY_ALLOCATION_ERROR;
	}

	int result;

	while(1) {
		result = Get_Logs(message, messageLength);
		if (result == 0) {
			free(*message);
		}
		else {
			break;
		}
	}

	free(message);
	free(messageLength);
	return NO_DCS_ERROR;
}

int init_Store() {
	if (hStoreMutex != NULL) {
		return THREAD_ALREADY_EXISTS;
	}

	hStoreMutex = CreateMutexW(NULL, false, NULL);
	if (hStoreMutex == NULL) {
		return THREAD_START_ERROR;
	}
	return NO_DCS_ERROR;
}

int close_Store() {
	if (hStoreMutex == NULL) {
		return NO_DCS_ERROR;
	}

	int result = CloseHandle(hStoreMutex);

	hStoreMutex = NULL;

	return result;
}

static inline void set_Store_mutex() {
	if (hStoreMutex == NULL) {
		return;
	}

	WaitForSingleObject(hStoreMutex, INFINITE);
}

static inline void release_Store_mutex() {
	if (hStoreMutex == NULL) {
		return;
	}

	ReleaseMutex(hStoreMutex);
}