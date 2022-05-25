#include "xparameters.h"
#include "xgpio.h"
#include "xaxidma.h"
#include "xil_printf.h"

#define DMA_ID XPAR_BASIC_SCATTER_GATHER_0_AXI_DMA_0_DEVICE_ID
#define GPIO_ID XPAR_STREAM_SOURCE_TO_BASIC_SCATTER_GATHER_0_AXI_GPIO_0_DEVICE_ID

#define STREAM_SOURCE_FREERUN_BIT 2
#define STREAM_SOURCE_ENABLE_BIT 1
#define STREAM_SOURCE_DEFAULT 0
#define STREAM_SOURCE_CHANNEL 1

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

u32 *InitializeBuffer (XAxiDma *InstPtr, u32 BufferLengthBytes) {
	u32 *RxBuffer = malloc(BufferLengthBytes);
	memset(RxBuffer, 0, BufferLengthBytes);

	//

	return RxBuffer;
}

void SgInitialize (XAxiDma *instptr, u32 num_bds, u32 bd_space_base, u32 bd_space_high, UINTPTR buffer_base, u32 stride) {
	XAxiDma_BdRing *ringptr;
	int coalesce = 1;
	int delay = 0;
	XAxiDma_Bd template;
	XAxiDma_Bd *BdPtr;
	XAxiDma_Bd *BdCurPtr;
	u32 BdCount;
	u32 FreeBdCount;
	UINTPTR RxBufferPtr;
	int Index;
	int Status;

	ringptr = XAxiDma_GetRxRing(instptr);
	XAxiDma_BdRingIntDisable(ringptr, XAXIDMA_IRQ_ALL_MASK);
	Status = XAxiDma_BdRingSetCoalesce(ringptr, coalesce, delay);
	if (Status != XST_SUCCESS) {
		xil_printf("XAxiDma_BdRingSetCoalesce failed for dma 0x%08x\r\n", instptr);
	}

//	BdCount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT, RX_BD_SPACE_HIGH - RX_BD_SPACE_BASE + 1);
	BdCount = num_bds;
	int MaxBdCount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT, bd_space_high - bd_space_base + 1);
	if (BdCount > MaxBdCount) {
		xil_printf("Error: memory allocated to Rx BD storage too small\r\n");
	}
	Status = XAxiDma_BdRingCreate(ringptr, bd_space_base, bd_space_base, XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	if (Status != XST_SUCCESS) {
		xil_printf("XAxiDma_BdRingCreate failed for dma 0x%08x\r\n", instptr);
	}

	// use a all-zero bd as template
	XAxiDma_BdClear(&template);
	Status = XAxiDma_BdRingClone(ringptr, &template);
	if (Status != XST_SUCCESS) {
		xil_printf("XAxiDma_BdRingClone failed for dma 0x%08x\r\n", instptr);
	}

	FreeBdCount = XAxiDma_BdRingGetFreeCnt(ringptr);
	Status = XAxiDma_BdRingAlloc(ringptr, FreeBdCount, &BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("XAxiDma_BdRingAlloc failed for dma 0x%08x\r\n", instptr);
	}

	BdCurPtr = BdPtr;
	RxBufferPtr = buffer_base;

	for (Index = 0; Index < FreeBdCount; Index++) {
		u32 BdId = RxBufferPtr;
		u32 BdLength = TRANSFER_LENGTH;
		u32 BdCtrl = 0;

		Status = XAxiDma_BdSetBufAddr(BdCurPtr, RxBufferPtr);
		if (Status != XST_SUCCESS) {
			xil_printf("XAxiDma_BdSetBufAddr failed for dma 0x%08x, BD %d\r\n", instptr, Index);
		}
		Status = XAxiDma_BdSetLength(BdCurPtr, BdLength, ringptr->MaxTransferLen);
		if (Status != XST_SUCCESS) {
			xil_printf("XAxiDma_BdSetBufAddr failed for dma 0x%08x, BD %d\r\n", instptr, Index);
		}

		XAxiDma_BdSetCtrl(BdCurPtr, BdCtrl);
		XAxiDma_BdSetId(BdCurPtr, BdId);

//		xil_printf("BD 0x%08x set up with length %d and address 0x%08x\r\n", BdId, BdLength, RxBufferPtr);

		RxBufferPtr += BdLength + stride;
		BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(ringptr, BdCurPtr);
	}

	Status = XAxiDma_BdRingToHw(ringptr, FreeBdCount, BdPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("XAxiDma_BdRingToHw failed for dma 0x%08x\r\n", instptr);
	}
}

void SgStartReceiveTransfer (XAxiDma *InstPtr) {
	XAxiDma_BdRing *RingPtr;
	RingPtr = XAxiDma_GetRxRing(InstPtr);
	int Status;

	Status = XAxiDma_BdRingStart(RingPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("RX start hw failed %d for dma 0x%08x\r\n", Status, InstPtr);
	}
}

#define TRIGGER_LEVEL 0x200000

u32 CheckLast(XAxiDma_Bd *BdPtr) {
	// placeholder; compare data at the front of the buffer to a specified level
	u32 *DataBuffer = XAxiDma_BdGetBufAddr(BdPtr);
	if (*DataBuffer >= TRIGGER_LEVEL) {
		// test this by placing it after the end of a transfer?
		return 1;
	}
	return 0;
}

u32 SgProcessBlocks (XAxiDma *InstPtr, u32 Resubmit) {
	XAxiDma_BdRing *RingPtr;
	XAxiDma_Bd *BdPtr;
	int ProcessedBdCount;
	int Status;

	RingPtr = XAxiDma_GetRxRing(InstPtr);

	ProcessedBdCount = XAxiDma_BdRingFromHw(RingPtr, XAXIDMA_ALL_BDS, &BdPtr);

	if (ProcessedBdCount > 0) {
		Status = XAxiDma_BdRingFree(RingPtr, ProcessedBdCount, BdPtr);
		if (Status != XST_SUCCESS) {
			xil_printf("XAxiDma_BdRingFree failed %d for dma 0x%08x\r\n", Status, InstPtr);
		}

		// reallocate the same number of BDs
		if (Resubmit && !CheckLast(BdPtr)) {
			Status = XAxiDma_BdRingAlloc(RingPtr, ProcessedBdCount, &BdPtr);
			if (Status != XST_SUCCESS) {
				xil_printf("XAxiDma_BdRingAlloc failed for dma 0x%08x\r\n", InstPtr);
			}
		}
	}

	return ProcessedBdCount;
}



u32 SgProcessBlocks_UntilLast (XAxiDma *InstPtr, u32 Resubmit, u32 *ProcessedBdCountPtr, u32 **LastBlockPtr) {
	XAxiDma_BdRing *RingPtr;
	XAxiDma_Bd *BdPtr;
//	int ProcessedBdCount;
	int Status;

	RingPtr = XAxiDma_GetRxRing(InstPtr);

	*ProcessedBdCountPtr = XAxiDma_BdRingFromHw(RingPtr, XAXIDMA_ALL_BDS, &BdPtr);

	if (*ProcessedBdCountPtr > 0) {
		Status = XAxiDma_BdRingFree(RingPtr, *ProcessedBdCountPtr, BdPtr);
		if (Status != XST_SUCCESS) {
			xil_printf("XAxiDma_BdRingFree failed %d for dma 0x%08x\r\n", Status, InstPtr);
		}

		if (CheckLast(BdPtr)) {
			*LastBlockPtr = XAxiDma_BdGetBufAddr(BdPtr);
			return 1;
		} else if (Resubmit) {
			// reallocate the same number of BDs
			Status = XAxiDma_BdRingAlloc(RingPtr, *ProcessedBdCountPtr, &BdPtr);
			if (Status != XST_SUCCESS) {
				xil_printf("XAxiDma_BdRingAlloc failed for dma 0x%08x\r\n", InstPtr);
			}
//			XAxiDma_DumpBd(BdPtr);
			Status = XAxiDma_BdRingToHw(RingPtr, *ProcessedBdCountPtr, BdPtr);
			if (Status != XST_SUCCESS) {
				xil_printf("XAxiDma_BdRingToHw failed for dma 0x%08x\r\n", InstPtr);
			}
		}
	}

	return 0;
}


int main () {
	u32 Status;

	// Initialize device drivers
	XAxiDma Dma;
	XGpio Gpio;
	InitializeDma(&Dma, DMA_ID);
	InitializeGpio(&Gpio, GPIO_ID);
	XGpio_DiscreteWrite(&Gpio, STREAM_SOURCE_CHANNEL, STREAM_SOURCE_DEFAULT);

	// Initialize the buffer for receiving data from PL
	const u32 BufferLength = 512; // max bytes = 2 ** DMA buffer length register
	const u32 BufferLengthBytes = BufferLength * sizeof(u32);
	u32 *RxBuffer = InitializeBuffer(&Dma, BufferLengthBytes);

	xil_printf("initialization done\r\n");

	// Flush the cache before any transfer
	Xil_DCacheFlushRange((UINTPTR)RxBuffer, BufferLengthBytes);
//	Status = XAxiDma_SimpleTransfer(&Dma, (UINTPTR)RxBuffer, BufferLengthBytes, XAXIDMA_DEVICE_TO_DMA);

	if (Status != 0) {
		xil_printf("SimpleTransfer Status = %d\r\n", Status);
	}

	// Enable the traffic generator
	XGpio_DiscreteWrite(&Gpio, STREAM_SOURCE_CHANNEL, STREAM_SOURCE_ENABLE_BIT);

	// Wait for the receive transfer to complete
	do {
		SgProcessBlocks_UntilLast();
	} while (XAxiDma_Busy(&Dma, XAXIDMA_DEVICE_TO_DMA));

	// Invalidate the cache to ensure acquired data can be read
	Xil_DCacheInvalidateRange((UINTPTR)RxBuffer, BufferLengthBytes);

	xil_printf("transfer done\r\n");

	// The traffic generator counters reset when enable is deasserted
	XGpio_DiscreteWrite(&Gpio, STREAM_SOURCE_CHANNEL, 0);

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
