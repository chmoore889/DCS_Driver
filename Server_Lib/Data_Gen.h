#pragma once

#include "Internal.h"

/// <summary>
/// Generates semi-random intensity data
/// </summary>
/// <param name="ids">Array of channel ids</param>
/// <param name="Cha_Num">Length of channel id array</param>
/// <param name="output">Pointer to output fake data array of Cha_Num size</param>
/// <returns></returns>
int gen_intensity_data(int* ids, int Cha_Num, Intensity_Data* output);

/// <summary>
/// Generates semi-random correlation-intensity data
/// </summary>
/// <param name="ids">Array of channel ids</param>
/// <param name="Cha_Num">Length of channel id array</param>
/// <param name="output">Pointer to output fake data array of Cha_Num size</param>
/// <param name="delayOut">Pointer to output fake delay data array of delayAndCorrBufLen size</param>
/// <param name="delayAndCorrBufLen">Size of delayOut and correlation arrays</param>
/// <returns></returns>
int gen_corr_intensity_data(int* ids, int Cha_Num, Corr_Intensity_Data* output, float* delayOut, int delayAndCorrBufLen);

/// <summary>
/// Generates semi-random BFI data
/// </summary>
/// <param name="ids">Array of channel ids</param>
/// <param name="Cha_Num">Length of channel id array</param>
/// <param name="output">Pointer to output fake data array of Cha_Num size</param>
/// <returns></returns>
int gen_bfi_data(int* ids, int Cha_Num, BFI_Data* output);