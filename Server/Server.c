#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>

#include "Server_Lib.h"
#include "Store.h"

#define DEFAULT_PORT "50000"

int main(void) {
	//Needed to detect and output memory leaks in debug mode.
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	int result = Start_Server(DEFAULT_PORT);
	if (result != NO_DCS_ERROR) {
		return result;
	}

	while (1) {
		char** message = malloc(sizeof(*message));
		unsigned __int32* messageLength = malloc(sizeof(*messageLength));
		result = Get_Logs(message, messageLength);
		if (result == 0 && message != NULL && messageLength != NULL) {
			char* toPrint = calloc((size_t)*messageLength + 1, sizeof(*toPrint));
			if (toPrint == NULL) {
				free(*message);
				free(message);
				free(messageLength);
				break;
			}

			memcpy(toPrint, *message, *messageLength);

			printf("%s\n", toPrint);
			free(*message);
		}
		free(message);
		free(messageLength);
	}

	return Stop_Server();
}