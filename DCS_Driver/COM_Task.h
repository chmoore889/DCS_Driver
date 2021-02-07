#pragma once

#include "Internal.h"
#include "DCS_Driver.h"

//Sends the data to be transmitted to the transmission FIFO.
int Enqueue_Trans_FIFO(Transmission_Data_Type* pTransmission);