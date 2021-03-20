#pragma once

#include <stdbool.h>

#ifdef DCS_DRIVER_EXPORTS
#define DCS_DRIVER_API __declspec(dllexport)
#else
#define DCS_DRIVER_API __declspec(dllimport)
#endif

//DCS Error Codes
#define NO_DCS_ERROR 0
#define FRAME_CHECKSUM_ERROR -1
#define FRAME_VERSION_ERROR -2
#define FRAME_INVALID_DATA -3
#define FRAME_DATA_CORRUPTION -4
#define MEMORY_ALLOCATION_ERROR -5
#define NETWORK_NOT_READY -6

//COM Task - Thread Error Codes
#define THREAD_START_ERROR -7
#define THREAD_ALREADY_EXISTS -8

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

typedef struct {
	int Cha_ID; // Channel ID
	float BFI; // absolute blood flow index
	float Beta; // β value in the fitting
	float rMSE; // relative mean square error
} BFI_Data_Type;

typedef struct {
	int Cha_ID; //Channel ID
	float intensity; //intensity of the optical channel
	int Data_Num; //number of the correlation value
	float* pCorrBuf; //pointer to the buffer of the correlation values
} Corr_Intensity_Data_Type;

//Structure for DCS address data.
typedef struct {
	const char* address; //IP Address of the DCS
	const char* port; //Port of the DCS
} DCS_Address;


/////////////////////////////////
//User-defined Callbacks Typedefs
/////////////////////////////////

//Callback for Get_DCS_Status.
typedef void(*Get_DCS_Status_CB_Def)(bool bCorr, bool bAnalyzer, int DCS_Cha_Num);

//Callback for Get_Correlator_Setting.
typedef void(*Get_Correlator_Setting_CB_Def)(Correlator_Setting_Type* pCorrelator_Setting);

//Callback for Get_Analyzer_Setting.
typedef void(*Get_Analyzer_Setting_CB_Def)(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num);

//Callback for Get_Simulated_Correlation
typedef void(*Get_Simulated_Correlation_CB_Def)(Simulated_Corr_Type* Simulated_Corr);

//Callback for Get_Analyzer_Prefit_Param
typedef void(*Get_Analyzer_Prefit_Param_CB_Def)(Analyzer_Prefit_Param_Type* pAnalyzer_Setting);

//Callback for error messages that are received
typedef void(*Get_Error_Message_CB_Def)(char* pMessage, unsigned __int32 Size);

//Callback for getting BFI data.
typedef void(*Get_BFI_Data_Def)(BFI_Data_Type* pBFI_Data, int Cha_Num);

//Callback for signaling that the BFI correlation data is ready.
typedef void(*Get_BFI_Corr_Ready_CB_Def)(bool bReady);

//Callback for getting the correlation intensity data.
typedef void(*Get_Corr_Intensity_Data_CB_Def)(Corr_Intensity_Data_Type* pCorr_Intensity_Data, int Cha_Num, float* pDelayBuf, int Delay_Num);

//Structure to hold all of the callbacks for the COM task to call.
typedef struct {
	//Callback for Get_DCS_Status.
	Get_DCS_Status_CB_Def Get_DCS_Status_CB;
	//Callback for Get_Correlator_Setting.
	Get_Correlator_Setting_CB_Def Get_Correlator_Setting_CB;
	//Callback for Get_Analyzer_Setting.
	Get_Analyzer_Setting_CB_Def Get_Analyzer_Setting_CB;
	//Callback for Get_Simulated_Correlation
	Get_Simulated_Correlation_CB_Def Get_Simulated_Correlation_CB;
	//Callback for Get_Analyzer_Prefit_Param
	Get_Analyzer_Prefit_Param_CB_Def Get_Analyzer_Prefit_Param_CB;
	//Callback for error messages that are received
	Get_Error_Message_CB_Def Get_Error_Message_CB;
	//Callback for getting BFI data.
	Get_BFI_Data_Def Get_BFI_Data;
	//Callback for signaling that the BFI correlation data is ready.
	Get_BFI_Corr_Ready_CB_Def Get_BFI_Corr_Ready_CB;
	//Callback for getting the correlation intensity data.
	Get_Corr_Intensity_Data_CB_Def Get_Corr_Intensity_Data_CB;
} Receive_Callbacks;

////////////
//Public API
////////////

/// <summary>
/// Starts the COM task for the DCS driver. <para>Not necessary to call before other DCS functions.
/// Other function calls will be queued up and sent once the COM task is initialized.
/// Can be called multiple times without calling [Destroy_COM_Task] to change the callbacks
/// being used, but not the address.</para>
/// </summary>
/// <param name="address">Address structure containing the IP and port of the
/// DCS device.
/// </param>
/// <param name="local_callbacks">Structure of callbacks for the COM task
/// to call when data is received.</param>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Initialize_COM_Task(DCS_Address address);

/// <summary>
/// Destroys an already created COM task. Should be called when no more data is to be sent or received.
/// Also needs to be called to change the IP address of the DCS.
/// </summary>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Destroy_COM_Task(void);

/// <summary>
/// Initiates the command to retrieve the status of the DCS. The data will be sent back by the driver
/// through the callback function [Get_DCS_Status_CB].
/// </summary>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Get_DCS_Status(void);

/// <summary>
/// Configures the correlator settings in the DCS.
/// </summary>
/// <param name="pCorr_Setting">The settings to send to the DCS.</param>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Set_Correlator_Setting(Correlator_Setting_Type* pCorr_Setting);

/// <summary>
/// Initiates the command to retrieve the correlator settings of the DCS. The settings data will be
/// sent back by the driver through the callback function [Get_Correlator_Setting_CB].
/// </summary>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Get_Correlator_Setting(void);

/// <summary>
/// Configures the analyzer settings of the DCS.
/// </summary>
/// <param name="pAnalyzer_Setting">Array of analyzer settings to send.</param>
/// <param name="Cha_Num">Number of DCS channels. Should equal the length of the
/// <paramref name="pAnalyzer_Setting"/> array.</param>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Set_Analyzer_Setting(Analyzer_Setting_Type* pAnalyzer_Setting, int Cha_Num);

/// <summary>
/// Retrieves the last fitted correlation from the DCS. The correlation data will be sent back by the
/// driver through the callback function [Get_Analyzer_Setting_CB].
/// </summary>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Get_Analyzer_Setting(void);

/// <summary>
/// Sends command to start the DCS measurement.
/// </summary>
/// <param name="interval">The time between measurements as a multiple of 10ms.</param>
/// <param name="pCha_IDs">Array of channel ids.</param>
/// <param name="Cha_Num">Length of the <paramref name="pCha_IDs"/> array.</param>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Start_DCS_Measurement(int interval, int* pCha_IDs, int Cha_Num);

/// <summary>
/// Sends command to stop the DCS measurement.
/// </summary>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Stop_DCS_Measurement(void);

/// <summary>
/// Enables or disables the data output of correlation data and BFI data. Intensity data is
/// always output during measurement scans.
/// </summary>
/// <param name="bCorr">Whether correlation data output should be enabled.</param>
/// <param name="bAnalyzer">Whether analyzer data output should be enabled.</param>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Enable_DCS(bool bCorr, bool bAnalyzer);

/// <summary>
/// Retrieves the last fitted correlation from the DCS.
/// The correlation data will be sent back by the driver through the callback 
/// function [Get_Simulated_Correlation_CB].
/// </summary>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Get_Simulated_Correlation(void);

/// <summary>
/// Sends the optical parameters to the DCS.
/// </summary>
/// <param name="pOpt_Param">Array of optical parameters.</param>
/// <param name="Cha_Num">Length of <paramref name="pOpt_Param"/> array.</param>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Set_Optical_Param(Optical_Param_Type* pOpt_Param, int Cha_Num);

/// <summary>
/// Send analyzer prefit parameter settings to the DCS.
/// </summary>
/// <param name="pAnalyzer_Prefit_Param">Analyzer prefit parameter settings.</param>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Set_Analyzer_Prefit_Param(Analyzer_Prefit_Param_Type* pAnalyzer_Prefit_Param);

/// <summary>
/// Retrieves the prefit parameters from the DCS.
/// The correlation data will be sent back by the driver through the callback 
/// function [Get_Analyzer_Prefit_Param_CB].
/// </summary>
/// <returns>Standard DCS status code.</returns>
DCS_DRIVER_API int Get_Analyzer_Prefit_Param(void);
