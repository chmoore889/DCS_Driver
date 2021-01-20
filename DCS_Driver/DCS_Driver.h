#pragma once

//Error Codes
#define NO_DCS_ERROR 0
#define FRAME_CHECKSUM_ERROR -1
#define FRAME_VERSION_ERROR -2
#define FRAME_INVALID_DATA -3
#define FRAME_DATA_CORRUPTION -4
#define MEMORY_ALLOCATION_ERROR -5
#define NETWORK_NOT_READY -6

typedef struct {
	int Data_N; //data number for correlation computation
	int Scale; //determine the number of correlation values (8*Scale)
	float Corr_Time; //The duration for the correlation
} Correlator_Setting_Type;

typedef struct {
	float Alpha; // error threshold for fitting
	float Distance; // source detector separation in cm
	float Wavelength; // laser wavelength in nm
	float mua0; // light absorption coefficient (1/cm)
	float musp; // light scattering coefficient (1/cm)
	float Db; // initial Db value 5X10 -9
	float Beta; // initial Beta value 0.5
} Analyzer_Setting_Type;

typedef struct {
	int Precut; // Start index of raw correlation
	int PostCut; // End index of raw correlation
	float Min_Intensity; // minimum intensity
	float Max_Intensity; // maximum intensity
	float FitLimt; // lower threshold to redefine the end
				   // index of the raw correlation
	float lightLeakage; // The last 10 correlation values must
						// be over this threshold
	float earlyLeakage; // The first 5 correlation values must
						// be below this threshold
	bool Model; // selection of DCS model, FALSE: semi-infinite
				// TRUE: infinite
} Analyzer_Prefit_Param_Type;

typedef struct {
	int Precut; // Index corresponds to the correlation
				// delay for the first fitted
				// correlation value
	int Cha_ID; // Channel ID of the fitted correlation
	int Data_Num; // number of correlation values
	float* pCorrBuf; // Array of single precision values for the
					 // fitted correlation
} Simulated_Corr_Type;

typedef struct {
	int Cha_ID; // channel ID
	float mua0; // light absorption coefficient (1/cm)
	float musp; // light scattering coefficient (1/cm)
} Optical_Param_Type;


//Public API

//Initiates the command to retrieve the status of the DCS.
//The settings data will be sent back by the driver through the callback 
//function Get_DCS_Status_CB.
int Get_DCS_Status();

//Configures the correlator settings in the DCS using the data in the array pCorr_Setting.
int Set_Correlator_Setting(Correlator_Setting_Type* pCorr_Setting);

//Initiates the command to retrieve the correlator settings in the DCS.
//The settings data will be sent back by the driver through the callback 
//function Get_Correlator_Setting_CB.
int Get_Correlator_Setting();

//Configures the analyzer settings in the DCS using the data in the array pAnalyzer_Setting
int Set_Analyzer_Setting(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num);

//Retrieves the last fitted correlation from the DCS.
//The correlation data will be sent back by the driver through the callback 
//function Get_Analyzer_Setting_CB.
int Get_Analyzer_Setting();

//Starts the DCS measurement.
int Start_DCS_Measurement(int interval, int* pCha_IDs, int Cha_Num);

//Stops the DCS measurement.
int Stop_DCS_Measurement();

//Enables or disables the data output of correlation data and BFI data. Intensity data is
//always output during measurement scans
int Enable_DCS(bool bCorr, bool bAnalyzer);

//Retrieves the last fitted correlation from the DCS.
//The correlation data will be sent back by the driver through the callback 
//function Get_Simulated_Correlation_CB.
int Get_Simulated_Correlation();

//Sets optical parameters.
int Set_Optical_Param(Optical_Param_Type* pOpt_Param, int Cha_Num);

//Configures the analyzer settings in the DCS using the data in the array pAnalyzer_Setting
int Set_Analyzer_Prefit_Param(Analyzer_Prefit_Param_Type* pAnalyzer_Prefit_Param);

//Retrieves the prefit parameters from the DCS.
//The correlation data will be sent back by the driver through the callback 
//function Get_Analyzer_Prefit_Param_CB.
int Get_Analyzer_Prefit_Param();


//User-defined Callbacks

//Callback for Get_DCS_Status.
void Get_DCS_Status_CB(bool bCorr, bool bAnalyzer, int DCS_Cha_Num);

//Callback for Get_Correlator_Setting.
void Get_Correlator_Setting_CB(Correlator_Setting_Type* pCorrelator_Setting);

//Callback for Get_Analyzer_Setting.
void Get_Analyzer_Setting_CB(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num);

//Callback for Get_Simulated_Correlation
void Get_Simulated_Correlation_CB(Simulated_Corr_Type* Simulated_Corr);

//Callback for Get_Analyzer_Prefit_Param
void Get_Analyzer_Prefit_Param_CB(Analyzer_Prefit_Param_Type* pAnalyzer_Setting);