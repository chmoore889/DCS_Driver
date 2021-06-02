#define _CRT_RAND_S
#include <stdlib.h>

#include "Server_Lib.h"
#include "Data_Gen.h"

int gen_intensity_data(int* ids, int Cha_Num, Intensity_Data* output) {
	for (int x = 0; x < Cha_Num; x++) {
		unsigned int rand;

		int result = rand_s(&rand);
		if (result != 0) {
			return result;
		}

		int id = ids[x];
		float intensity = ((float) rand / ((float) UINT_MAX) * 100.0f) + 600.0f;

		Intensity_Data data = {
			.Cha_ID = id,
			.intensity = intensity,
		};

		output[x] = data;
	}
	return NO_DCS_ERROR;
}

int gen_corr_intensity_data(int* ids, int Cha_Num, Corr_Intensity_Data* output, float* delayOut, int delayAndCorrBufLen) {
	for (int x = 0; x < Cha_Num; x++) {
		unsigned int rand;

		int result = rand_s(&rand);
		if (result != 0) {
			return result;
		}

		int id = ids[x];
		float intensity = ((float)rand / ((float)UINT_MAX) * 100.0f) + 600.0f;

		output[x].Cha_ID = id;
		output[x].intensity = intensity;
		output[x].Data_Num = delayAndCorrBufLen;
		for (int y = 0; y < output[x].Data_Num; y++) {
			int result = rand_s(&rand);
			if (result != 0) {
				return result;
			}

			float correlation = ((float)rand / ((float)UINT_MAX) * 0.5f) + 0.5f + (float)id;

			output[x].pCorrBuf[y] = correlation;
		}
	}

	for (int x = 0; x < delayAndCorrBufLen; x++) {
		unsigned int rand;

		int result = rand_s(&rand);
		if (result != 0) {
			return result;
		}

		float delay = ((float)rand / ((float)UINT_MAX) * 1e-6f);

		delayOut[x] = delay;
	}
	return NO_DCS_ERROR;
}

int gen_bfi_data(int* ids, int Cha_Num, BFI_Data* output) {
	for (int x = 0; x < Cha_Num; x++) {
		int id = ids[x];

		unsigned int rand;

		int result = rand_s(&rand);
		if (result != 0) {
			return result;
		}
		float bfi = ((float)rand / ((float)UINT_MAX) * 0.1f) + (float)id;

		result = rand_s(&rand);
		if (result != 0) {
			return result;
		}
		float beta = ((float)rand / ((float)UINT_MAX) * 0.1f) + 0.5f;

		BFI_Data data = {
			.Cha_ID = id,
			.BFI = bfi,
			.Beta = beta,
			.rMSE = 1e-7f,
		};

		output[x] = data;
	}
	return NO_DCS_ERROR;
}