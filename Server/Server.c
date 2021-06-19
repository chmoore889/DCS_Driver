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

	Sleep(INFINITE);

	return Stop_Server();
}