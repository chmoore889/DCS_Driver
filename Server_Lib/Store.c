#include "Store.h"
#include "Internal.h"
#include "Server_Lib.h"

#define NUM_CHANNELS 6

static HANDLE hStoreMutex;
static inline void set_Store_mutex(void);
static inline void release_Store_mutex(void);

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
	set_Store_mutex();

	corr_setting = newVal;

	release_Store_mutex();

	Send_DCS_Message("Set Correlator Setting Success");
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
	set_Store_mutex();

	if (sizeof(analyzer_setting) != sizeof(*pAnalyzer_Setting) * Cha_Num) {
		release_Store_mutex();
		Send_DCS_Error("Incorrect number of channels in analyzer set command", 5104);
		return 1;
	}

	memcpy(analyzer_setting, pAnalyzer_Setting, sizeof(analyzer_setting));

	release_Store_mutex();

	Send_DCS_Message("Set Analyzer Setting Success");

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

	set_Store_mutex();

	for (int x = 0; x < Cha_Num; x++) {
		Analyzer_Setting* chaToChange = &analyzer_setting[input_arr[x].Cha_ID];

		chaToChange->mua0 = input_arr[x].mua0;
		chaToChange->musp = input_arr[x].musp;
	}

	release_Store_mutex();

	Send_DCS_Message("Set Optical Param Success");

	return NO_DCS_ERROR;
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
	set_Store_mutex();

	if (Cha_Num > 2) {
		release_Store_mutex();
		Send_DCS_Error("Too many channel IDs", 5106);
		return 1;
	} 
	else if (interval <= 0) {
		release_Store_mutex();
		Send_DCS_Error("Interval cannot be 0", 5107);
		return 1;
	}

	for (int x = 0; x < Cha_Num; x++) {
		if (ids[x] >= NUM_CHANNELS) {
			release_Store_mutex();
			Send_DCS_Error("Invalid channel ID", 5108);
			return 1;
		}
	}

	measurement_status.measurement_going = true;
	measurement_status.interval = interval;
	measurement_status.Cha_Num = Cha_Num;
	memcpy(measurement_status.ids, ids, sizeof(*ids) * Cha_Num);

	release_Store_mutex();

	return NO_DCS_ERROR;
}

int Stop_Measurement() {
	set_Store_mutex();

	measurement_status.measurement_going = false;

	release_Store_mutex();

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