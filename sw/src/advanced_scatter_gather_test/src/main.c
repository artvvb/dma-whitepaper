#include "xparameters.h"
#include "xgpio.h"
#include "xaxidma.h"
#include "xil_printf.h"
#include "sleep.h"
#include "trigger.h"

#define DMA_ID XPAR_ADVANCED_SCATTER_GATHER_0_AXI_DMA_1_DEVICE_ID
#define DMA_BURST_SIZE XPAR_ADVANCED_SCATTER_GATHER_0_AXI_DMA_1_S2MM_BURST_SIZE
#define DMA_DATA_WIDTH XPAR_ADVANCED_SCATTER_GATHER_0_AXI_DMA_1_M_AXI_S2MM_DATA_WIDTH


#define TRAFFIC_CTRL_ID XPAR_STREAM_SOURCE_TO_ADVANCED_SCATTER_GATHER_0_CTRL_0_DEVICE_ID
#define TRAFFIC_CTRL_FREERUN_BIT 2
#define TRAFFIC_CTRL_ENABLE_BIT 1
#define TRAFFIC_CTRL_CHANNEL 1

#define MANUAL_TRIGGER_ID XPAR_ADVANCED_SCATTER_GATHER_TRIGGER_0_MANUAL_TRIGGER_0_DEVICE_ID
#define MANUAL_TRIGGER_CHANNEL 1

// Buffer Memory Allocation
#define DDR_BASE_ADDR       XPAR_PS7_DDR_0_S_AXI_BASEADDR
#define MEM_BASE_ADDR		(DDR_BASE_ADDR + 0x01000000)
#define RX0_BD_SPACE_BASE	(MEM_BASE_ADDR)
#define RX0_BD_SPACE_HIGH	(RX0_BD_SPACE_BASE + 0x00010000 - 1)

// Function definitions
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

#define RoundUpDivide(a, b) ((a / b) + (a % b != 0))

void AllocateBuffer (XAxiDma *InstPtr, u32 BufferLengthBytes, u32 **BufferPtr, u32 **BdSpacePtr) {
	*BufferPtr = malloc(BufferLengthBytes);
	memset(*BufferPtr, 0, BufferLengthBytes);

//	const u32 BytesPerBlock = 256;
//	u32 NumBds = RoundUpDivide(BufferLengthBytes, BytesPerBlock);
//	u32 BdSpaceBytes = XAxiDma_BdRingMemCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT, NumBds);
//	u32 *BdSpace = malloc(BdSpaceBytes);
	// initialize bd ring

}

void DeallocateBuffer (u32 **BufferPtr, u32 **BdSpacePtr) {
	if (*BufferPtr) free(*BufferPtr);
	if (*BdSpacePtr) free(*BdSpacePtr);
}

void SgInitialize (XAxiDma *instptr, u32 MaxBurstLengthBytes, u32 num_bds, u32 bd_space_base, u32 bd_space_high, UINTPTR buffer_base, u32 stride) {
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
		u32 BdLength = MaxBurstLengthBytes;
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

	XAxiDma_SelectCyclicMode(InstPtr, XAXIDMA_DEVICE_TO_DMA, TRUE);
	XAxiDma_BdRingEnableCyclicDMA(RingPtr);

	Status = XAxiDma_BdRingStart(RingPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("RX start hw failed %d for dma 0x%08x\r\n", Status, InstPtr);
	}
}

u32 *FindStartOfBuffer (XAxiDma *InstPtr, u32 NumBds, u32 MaxBurstLengthBytes) {
	XAxiDma_BdRing *RingPtr = XAxiDma_GetRxRing(InstPtr);
	XAxiDma_Bd *BdPtr;
	u32 ActualLength;

	BdPtr = (XAxiDma_Bd*)RingPtr->FirstBdAddr; // this is kind of weird. FIXME find out why this value isn't the one getting returned from BdRingFree

	for (u32 i = 0; i < NumBds; i++) {
		Xil_DCacheInvalidateRange((UINTPTR)BdPtr, XAXIDMA_BD_NUM_WORDS * sizeof(u32));

		XAxiDma_DumpBd(BdPtr);

		u32 Status = XAxiDma_BdGetSts(BdPtr);
		// RXEOF bit high indicates that tlast occured in that block
		if (Status & XAXIDMA_BD_STS_RXEOF_MASK) {
			ActualLength = XAxiDma_BdGetActualLength(BdPtr, ((MaxBurstLengthBytes*2)-1));
			xil_printf("Last beat found:\r\n");
			xil_printf("  BD base address: %08x\r\n", XAxiDma_BdGetBufAddr(BdPtr));
			xil_printf("  BD actual length: %08x\r\n", ActualLength);
			return (u32*)(XAxiDma_BdGetBufAddr(BdPtr) + ActualLength);
		}

		// Advance the pointer to the next descriptor
		BdPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RingPtr, BdPtr);
	}
	return 0;
}

int main () {
	// Initialize device drivers
	XAxiDma Dma;
	XGpio TrafficCtrlGpio;
	XGpio ManualTriggerGpio;
	TriggerController Trig;

	TriggerControllerInitialize(&Trig, TRIGGER_CONFIG_ID, TRIGGER_CTRL_ID, TRIGGER_COUNTER_CONFIG_ID);
	InitializeDma(&Dma, DMA_ID);
	InitializeGpio(&TrafficCtrlGpio, TRAFFIC_CTRL_ID);
	InitializeGpio(&ManualTriggerGpio, MANUAL_TRIGGER_ID);

	// Define the acquisition window
	const u32 BufferLength = 1024;
	const u32 TriggerPosition = 200;

	// Initialize the buffer for receiving data from PL
	u32 *RxBuffer = NULL;
	u32 *RxBdSpace = NULL;
	AllocateBuffer(&Dma, BufferLength * sizeof(u32), &RxBuffer, &RxBdSpace);

	// Set up the Dma transfer
//	const u32 MaxBurstLengthBytes = 0x200;
	const u32 MaxBurstLengthBytes = DMA_BURST_SIZE * DMA_DATA_WIDTH / 8;
	const u32 NumBds = RoundUpDivide(BufferLength * sizeof(u32), MaxBurstLengthBytes);
	SgInitialize(&Dma, MaxBurstLengthBytes, NumBds, RX0_BD_SPACE_BASE, RX0_BD_SPACE_HIGH, (UINTPTR)RxBuffer, 0);

	// Flush the cache before any transfer
	Xil_DCacheFlushRange((UINTPTR)RxBuffer, BufferLength * sizeof(u32));

	// Configure the trigger
	TriggerSetPosition (&Trig, BufferLength, TriggerPosition);
	TriggerSetEnable (&Trig, 0xFFFFFFFF);

	xil_printf("Initialization done\r\n");

	// Start up the input pipeline from back to front
	// Start the DMA receive
	SgStartReceiveTransfer(&Dma);

	// Start the trigger hardware
	TriggerStart(&Trig);

	// Enable the traffic generator
	XGpio_DiscreteWrite(&TrafficCtrlGpio, TRAFFIC_CTRL_CHANNEL, TRAFFIC_CTRL_ENABLE_BIT);

	// Wait for the receive transfer to complete

	// Apply a manual trigger
//	usleep(1);
	u32 trigtime = 0;
	while (trigtime++ < 1000);
	XGpio_DiscreteWrite(&ManualTriggerGpio, MANUAL_TRIGGER_CHANNEL, 0x1);

	// wait for trigger hardware to go idle
	while (TriggerGetIdle(&Trig)); // FIXME polarity seems wrong?

//	XAxiDma_Pause(&Dma);

	u32 *BufferHeadPtr = FindStartOfBuffer(&Dma, NumBds, MaxBurstLengthBytes);
	if (BufferHeadPtr == NULL) {
		xil_printf("ERROR: No buffer head detected\r\n");
	}

	u32 BufferHeadIndex = (((u32)BufferHeadPtr - (u32)RxBuffer) / sizeof(u32)) % BufferLength;

//	XAxiDma_Resume(&Dma);

	u32 TriggerDetected = TriggerGetDetected(&Trig);

	xil_printf("Buffer base address: %08x\r\n", RxBuffer);
	xil_printf("Buffer high address: %08x\r\n", ((u32)RxBuffer) + ((BufferLength-1) * sizeof(u32)));
	xil_printf("Length of buffer (words): %d\r\n", BufferLength);
	xil_printf("Index of buffer head: %d\r\n", BufferHeadIndex);
	xil_printf("Trigger position: %d\r\n", TriggerPosition);
	xil_printf("Index of trigger position: %d\r\n", (BufferHeadIndex + TriggerPosition) % BufferLength);
	xil_printf("Detected trigger condition: %08x\r\n", TriggerDetected);

	// Invalidate the cache to ensure acquired data can be read
	Xil_DCacheInvalidateRange((UINTPTR)RxBuffer, BufferLength * sizeof(u32));

	xil_printf("Transfer done\r\n");

	// The traffic generator counters reset when enable is deasserted
	XGpio_DiscreteWrite(&TrafficCtrlGpio, TRAFFIC_CTRL_CHANNEL, 0);

	// Check the buffer to see if data increments like expected
	u32 errors = 0;
	u32 FirstValue = RxBuffer[BufferHeadIndex];
	for (u32 i = 0; i < BufferLength; i++) {
		u32 index = (i + BufferHeadIndex) % BufferLength;
		if (RxBuffer[index] != FirstValue + i) {
			xil_printf("RxBuffer at %08x: %d; expected %d\r\n", (u32)RxBuffer + index*sizeof(u32), RxBuffer[index], FirstValue + i);
			errors++;
		}
	}
	if (errors == 0) {
		xil_printf("All RxBuffer data matched!\r\n");
	}

	// Clean up allocated memory
	DeallocateBuffer(&RxBuffer, &RxBdSpace);

	xil_printf("done\r\n");
}
