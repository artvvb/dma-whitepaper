#ifndef TRIGGER_H_   /* prevent circular inclusions */
#define TRIGGER_H_

#include "xgpio.h"

#define TRIGGER_CTRL_CHANNEL 1
#define TRIGGER_CTRL_START_MASK 1
#define TRIGGER_CTRL_IDLE_MASK 2

#define TRIGGER_CONFIG_ID XPAR_ADVANCED_SCATTER_GATHER_TRIGGER_0_CFG_0_DEVICE_ID
#define TRIGGER_CONFIG_ENABLE_CHANNEL 1
#define TRIGGER_CONFIG_DETECTED_CHANNEL 2

#define TRIGGER_CTRL_ID XPAR_ADVANCED_SCATTER_GATHER_TRIGGER_0_CTRL_0_DEVICE_ID

#define TRIGGER_COUNTER_CONFIG_ID XPAR_ADVANCED_SCATTER_GATHER_TRIGGER_0_COUNTER_CFG_0_DEVICE_ID
#define TRIGGER_COUNTER_CONFIG_TRIG_TO_LAST_BEATS_CHANNEL 1
#define TRIGGER_COUNTER_CONFIG_PREBUFFER_BEATS_CHANNEL 2

typedef struct {
	XGpio Config;
	XGpio Ctrl;
	XGpio CounterConfig;
} TriggerController;

u32 TriggerGetIdle (TriggerController *InstPtr) {
	return XGpio_DiscreteRead(&(InstPtr->Ctrl), TRIGGER_CTRL_CHANNEL) & TRIGGER_CTRL_IDLE_MASK;
}

void TriggerControllerInitialize (TriggerController *InstPtr, u32 ConfigDeviceId, u32 CtrlDeviceId, u32 CounterConfigDeviceId) {
	XGpio_Config *GpioCfgPtr;

	GpioCfgPtr = XGpio_LookupConfig(TRIGGER_CONFIG_ID);
	XGpio_CfgInitialize(&(InstPtr->Config), GpioCfgPtr, GpioCfgPtr->BaseAddress);

	GpioCfgPtr = XGpio_LookupConfig(TRIGGER_CTRL_ID);
	XGpio_CfgInitialize(&(InstPtr->Ctrl), GpioCfgPtr, GpioCfgPtr->BaseAddress);

	GpioCfgPtr = XGpio_LookupConfig(TRIGGER_COUNTER_CONFIG_ID);
	XGpio_CfgInitialize(&(InstPtr->CounterConfig), GpioCfgPtr, GpioCfgPtr->BaseAddress);
};

void TriggerSetPosition (TriggerController *InstPtr, u32 BufferLength, u32 TriggerPosition) {
	const u32 PrebufferLength = TriggerPosition;
	const u32 TrigToLastLength = BufferLength - TriggerPosition;

	XGpio_DiscreteWrite(&(InstPtr->CounterConfig), TRIGGER_COUNTER_CONFIG_TRIG_TO_LAST_BEATS_CHANNEL, TrigToLastLength);
	XGpio_DiscreteWrite(&(InstPtr->CounterConfig), TRIGGER_COUNTER_CONFIG_PREBUFFER_BEATS_CHANNEL, PrebufferLength);
}

void TriggerSetEnable (TriggerController *InstPtr, u32 EnableMask) {
	XGpio_DiscreteWrite(&(InstPtr->Config), TRIGGER_CONFIG_ENABLE_CHANNEL, EnableMask);
}

void TriggerStart (TriggerController *InstPtr) {
	XGpio_DiscreteWrite(&(InstPtr->Ctrl), TRIGGER_CTRL_CHANNEL, TRIGGER_CTRL_START_MASK);
	XGpio_DiscreteWrite(&(InstPtr->Ctrl), TRIGGER_CTRL_CHANNEL, 0);
}

u32 TriggerGetDetected (TriggerController *InstPtr) {
	return XGpio_DiscreteRead(&(InstPtr->Config), TRIGGER_CONFIG_DETECTED_CHANNEL);
}

#endif /* end of protection macro */
