#include "xparameters.h"
#include "xaxidma.h"
#include "xil_printf.h"
#include "xil_types.h"

#define DMA_ID XPAR_LOOPBACK_0_AXI_DMA_0_DEVICE_ID

int main () {
	u32 Status;

	// Initialize device drivers
	XAxiDma Dma;
	XAxiDma_Config *CfgPtr;
	CfgPtr = XAxiDma_LookupConfig(DMA_ID);
	XAxiDma_CfgInitialize(&Dma, CfgPtr);
	XAxiDma_IntrDisable(&Dma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);

	// Initialize the buffers
	const u32 BufferLength = 256; // max bytes = 2 ** DMA width of buffer length register parameter (default: 2 ** 14)
	const u32 BufferLengthBytes = BufferLength * sizeof(u32);

	u32 *RxBuffer = malloc(BufferLengthBytes);
	u32 *TxBuffer = malloc(BufferLengthBytes);

	memset(RxBuffer, 0, BufferLengthBytes);

	for (u32 i = 0; i < BufferLength; i++) {
		TxBuffer[i] = i;
	}

	xil_printf("Initialization done\r\n");

	// Flush the cache before any transfer
	Xil_DCacheFlushRange((UINTPTR)RxBuffer, BufferLengthBytes);
	Xil_DCacheFlushRange((UINTPTR)TxBuffer, BufferLengthBytes);

	// Start the receive transfer
	Status = XAxiDma_SimpleTransfer(&Dma, (UINTPTR)RxBuffer, BufferLengthBytes, XAXIDMA_DEVICE_TO_DMA);
	if (Status != 0) {
		xil_printf("Receive SimpleTransfer failed with error code %d\r\n", Status);
	}

	// Start transmit
	Status = XAxiDma_SimpleTransfer(&Dma, (UINTPTR)TxBuffer, BufferLengthBytes, XAXIDMA_DMA_TO_DEVICE);
	if (Status != 0) {
		xil_printf("Transmit SimpleTransfer failed with error code %d\r\n", Status);
	}

	// Wait for the receive transfer to complete
	while (XAxiDma_Busy(&Dma, XAXIDMA_DEVICE_TO_DMA) || XAxiDma_Busy(&Dma, XAXIDMA_DMA_TO_DEVICE));

	// Invalidate the cache to make sure data in RxBuffer is available to the processor
	Xil_DCacheInvalidateRange((UINTPTR)RxBuffer, BufferLengthBytes);

	xil_printf("Transfer done\r\n");

	// Check the buffer to see if data is as expected
	u32 errors = 0;
	for (u32 i = 0; i < BufferLength; i++) {
		if (RxBuffer[i] != TxBuffer[i]) {
			xil_printf("RxBuffer and TxBuffer don't match at index %d: %d != %d\r\n", i, RxBuffer[i], TxBuffer[i]);
			errors++;
		}
	}
	if (errors == 0) {
		xil_printf("The buffers matched!\r\n");
	}
}
