#pragma once
#include "OVCAM.h"


void rots_init();
void capture_task(void*);
void tx_task(void*);
void rx_task(void*);
