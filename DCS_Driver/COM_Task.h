#pragma once

#include "Internal.h"
#include "DCS_Driver.h"

#define MAX_COMMAND_RESPONSE_TIME 1000
#define COMMAND_RSP_RESET 0
#define COMMAND_RSP_SET 1
#define COMMAND_RSP_CHECK 2
#define COMMAND_RSP_VALIDATE 3

//Adds the data to be transmitted to the transmission FIFO.
int Enqueue_Trans_FIFO(Transmission_Data_Type* pTransmission);

int Check_Command_Response(int Option, int Command_Code);
bool Command_Ack = true;