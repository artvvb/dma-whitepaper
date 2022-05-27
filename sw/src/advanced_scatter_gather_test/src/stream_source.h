#ifndef TRAFFIC_GENERATOR_H_   /* prevent circular inclusions */
#define TRAFFIC_GENERATOR_H_

#include "xgpio.h"

#define TRAFFIC_CTRL_FREERUN_BIT 1
#define TRAFFIC_CTRL_ENABLE_BIT 0
#define TRAFFIC_CTRL_CHANNEL 1

typedef struct {
	XGpio Ctrl;
} StreamSource;

void StreamSourceInitialize(StreamSource *InstPtr, u32 CtrlDeviceId) {
	XGpio_Config *GpioCfgPtr;
	GpioCfgPtr = XGpio_LookupConfig(CtrlDeviceId);
	XGpio_CfgInitialize(&(InstPtr->Ctrl), GpioCfgPtr, GpioCfgPtr->BaseAddress);
}

void StreamSourceSetEnable(StreamSource *InstPtr, u8 Enable) {
	XGpio_DiscreteWrite(&(InstPtr->Ctrl), TRAFFIC_CTRL_CHANNEL, (Enable & 1) << TRAFFIC_CTRL_ENABLE_BIT);
}

#endif /* end of protection macro */
