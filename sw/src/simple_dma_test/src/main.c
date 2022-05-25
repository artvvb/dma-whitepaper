#include "xparameters.h"
#include "xaxidma.h"
#include "xil_printf.h"
#include "xil_types.h"
#include "xgpio.h"

#define DMA_ID XPAR_SIMPLE_TRANSFER_0_AXI_DMA_0_DEVICE_ID

#define TRAFFIC_CTRL_ID XPAR_STREAM_SOURCE_TO_SIMPLE_TRANSFER_0_TRAFFIC_GENERATOR_CTRL_0_DEVICE_ID
#define TRAFFIC_CTRL_FREERUN_BIT 2
#define TRAFFIC_CTRL_ENABLE_BIT 1
#define TRAFFIC_CTRL_CHANNEL 1

#define TLAST_INJECT_ID XPAR_STREAM_SOURCE_TO_SIMPLE_TRANSFER_0_INJECT_TLAST_CTRL_0_DEVICE_ID
#define TLAST_INJECT_COMPARE_VALUE_CHANNEL 1
#define TLAST_INJECT_COMPARE_MASK_CHANNEL 2

void InitializeDma (XAxiDma *InstPtr, const u32 DeviceId) {
	XAxiDma_Config *CfgPtr;

	CfgPtr = XAxiDma_LookupConfig(DeviceId);
	XAxiDma_CfgInitialize(InstPtr, CfgPtr);
	XAxiDma_IntrDisable(InstPtr, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
}

void InitializeGpio (XGpio *InstPtr, const u32 DeviceId) {
	XGpio_Config *GpioCfgPtr;

	GpioCfgPtr = XGpio_LookupConfig(DeviceId);
	XGpio_CfgInitialize(InstPtr, GpioCfgPtr, GpioCfgPtr->BaseAddress);
}

int main () {
	u32 Status;

	// Initialize device drivers
	XAxiDma Dma;
	XGpio TrafficCtrl;
	XGpio TlastInject;
	InitializeDma(&Dma, DMA_ID);
	InitializeGpio(&TrafficCtrl, TRAFFIC_CTRL_ID);
	InitializeGpio(&TlastInject, TLAST_INJECT_ID);

	// Initialize the buffer for receiving data from PL
	const u32 BufferLength = 512; // max bytes = 2 ** DMA buffer length register
	const u32 BufferAddressMask = 0x1FF;
	const u32 BufferLengthBytes = BufferLength * sizeof(u32);
	u32 *RxBuffer = malloc(BufferLengthBytes);
	memset(RxBuffer, 0, BufferLengthBytes);

	xil_printf("Initialization done\r\n");

	// Flush the cache before any transfer
	Xil_DCacheFlushRange((UINTPTR)RxBuffer, BufferLengthBytes);
	Status = XAxiDma_SimpleTransfer(&Dma, (UINTPTR)RxBuffer, BufferLengthBytes, XAXIDMA_DEVICE_TO_DMA);

	if (Status != 0) {
		xil_printf("SimpleTransfer Status = %d\r\n", Status);
	}

	// Set up stream to have tlast injected at the end of the buffer
	XGpio_DiscreteWrite(&TlastInject, TLAST_INJECT_COMPARE_VALUE_CHANNEL, BufferLength-1);
	XGpio_DiscreteWrite(&TlastInject, TLAST_INJECT_COMPARE_MASK_CHANNEL, BufferAddressMask);

	// Enable the traffic generator, initiating the transfer
	XGpio_DiscreteWrite(&TrafficCtrl, TRAFFIC_CTRL_CHANNEL, TRAFFIC_CTRL_ENABLE_BIT);

	// Wait for the receive transfer to complete
	while (XAxiDma_Busy(&Dma, XAXIDMA_DEVICE_TO_DMA));
	Xil_DCacheInvalidateRange((UINTPTR)RxBuffer, BufferLengthBytes);

	xil_printf("Transfer done\r\n");

	// The traffic generator counters reset when enable is deasserted
	XGpio_DiscreteWrite(&TrafficCtrl, TRAFFIC_CTRL_CHANNEL, 0);

	// Return tlast inject regs to default 0
	XGpio_DiscreteWrite(&TlastInject, TLAST_INJECT_COMPARE_VALUE_CHANNEL, 0);
	XGpio_DiscreteWrite(&TlastInject, TLAST_INJECT_COMPARE_MASK_CHANNEL, 0);

	// Check the buffer to see if data increments like expected
	u32 errors = 0;
	for (u32 i = 0; i < BufferLength; i++) {
		if (RxBuffer[i] != i) {
			xil_printf("RxBuffer[%d] mismatch: %d\r\n", i, RxBuffer[i]);
			errors++;
		}
	}
	if (errors == 0) {
		xil_printf("All RxBuffer data matched!\r\n");
	}
}
