#include "xparameters.h"
#include "xgpio.h"
#include "xaxidma.h"
#include "xil_printf.h"

#define DMA_ID XPAR_ADVANCED_SCATTER_GATHER_0_AXI_DMA_1_DEVICE_ID

#define TRAFFIC_CTRL_ID XPAR_STREAM_SOURCE_TO_ADVANCED_SCATTER_GATHER_0_CTRL_0_DEVICE_ID
#define TRAFFIC_CTRL_FREERUN_BIT 2
#define TRAFFIC_CTRL_ENABLE_BIT 1
#define TRAFFIC_CTRL_CHANNEL 1

#define MANUAL_TRIGGER_ID XPAR_ADVANCED_SCATTER_GATHER_TRIGGER_0_MANUAL_TRIGGER_0_DEVICE_ID
#define MANUAL_TRIGGER_CHANNEL 1

#define TRIGGER_CONFIG_ID XPAR_ADVANCED_SCATTER_GATHER_TRIGGER_0_CFG_0_DEVICE_ID
#define TRIGGER_CONFIG_ENABLE_CHANNEL 1
#define TRIGGER_CONFIG_DETECTED_CHANNEL 2

#define TRIGGER_CTRL_ID XPAR_ADVANCED_SCATTER_GATHER_TRIGGER_0_CTRL_0_DEVICE_ID
#define TRIGGER_CTRL_CHANNEL 1
#define TRIGGER_CTRL_START_BIT 1
#define TRIGGER_CTRL_IDLE_BIT 2

#define TRIGGER_COUNTER_CONFIG_ID XPAR_ADVANCED_SCATTER_GATHER_TRIGGER_0_COUNTER_CFG_0_DEVICE_ID
#define TRIGGER_COUNTER_CONFIG_TRIG_TO_LAST_BEATS_CHANNEL 1
#define TRIGGER_COUNTER_CONFIG_PREBUFFER_BEATS_CHANNEL 2

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

u32 *InitializeBuffer (XAxiDma *InstPtr, u32 BufferLengthBytes) {
	u32 *RxBuffer = malloc(BufferLengthBytes);
	memset(RxBuffer, 0, BufferLengthBytes);

	// initialize bd ring

	return RxBuffer;
}

void SgInitialize (XAxiDma *instptr, u32 MaxBurstLength, u32 num_bds, u32 bd_space_base, u32 bd_space_high, UINTPTR buffer_base, u32 stride) {
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
		u32 BdLength = MaxBurstLength * sizeof(u32);
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

int main () {
	// Initialize device drivers
	XAxiDma Dma;
	XGpio TrafficCtrlGpio;
	XGpio ManualTriggerGpio;
	XGpio TriggerConfigGpio;
	XGpio TriggerCtrlGpio;
	XGpio TriggerCounterConfigGpio;

	InitializeDma(&Dma, DMA_ID);
	InitializeGpio(&TrafficCtrlGpio, TRAFFIC_CTRL_ID);
	InitializeGpio(&ManualTriggerGpio, MANUAL_TRIGGER_ID);
	InitializeGpio(&TriggerConfigGpio, TRIGGER_CONFIG_ID);
	InitializeGpio(&TriggerCtrlGpio, TRIGGER_CTRL_ID);
	InitializeGpio(&TriggerCounterConfigGpio, TRIGGER_COUNTER_CONFIG_ID);

	// Initialize the buffer for receiving data from PL
	const u32 BufferLength = 65536;
	const u32 TriggerPosition = 200;
	const u32 PrebufferLength = TriggerPosition;
	const u32 TrigToLastLength = BufferLength - TriggerPosition;
	const u32 BufferLengthBytes = BufferLength * sizeof(u32);
	u32 *RxBuffer = InitializeBuffer(&Dma, BufferLengthBytes);

	// Set up the Dma transfer
	const u32 MaxBurstLength = 256;
	const u32 NumBds = RoundUpDivide(BufferLength, MaxBurstLength);
	SgInitialize(&Dma, MaxBurstLength, NumBds, RX0_BD_SPACE_BASE, RX0_BD_SPACE_HIGH, (UINTPTR)RxBuffer, 0);

	// Flush the cache before any transfer
	Xil_DCacheFlushRange((UINTPTR)RxBuffer, BufferLengthBytes);

	// Configure the trigger
	XGpio_DiscreteWrite(&TriggerCounterConfigGpio, TRIGGER_COUNTER_CONFIG_TRIG_TO_LAST_BEATS_CHANNEL, TrigToLastLength);
	XGpio_DiscreteWrite(&TriggerCounterConfigGpio, TRIGGER_COUNTER_CONFIG_PREBUFFER_BEATS_CHANNEL, PrebufferLength);
	XGpio_DiscreteWrite(&TriggerConfigGpio, TRIGGER_CONFIG_ENABLE_CHANNEL, 0xFFFFFFFF);

	xil_printf("Initialization done\r\n");

	// Start the DMA receive
	SgStartReceiveTransfer(&Dma);

	// Start the trigger hardware
	XGpio_DiscreteWrite(&TriggerCtrlGpio, TRIGGER_CTRL_CHANNEL, TRIGGER_CTRL_START_BIT);
	XGpio_DiscreteWrite(&TriggerCtrlGpio, TRIGGER_CTRL_CHANNEL, 0);

	// Enable the traffic generator
	XGpio_DiscreteWrite(&TrafficCtrlGpio, TRAFFIC_CTRL_CHANNEL, TRAFFIC_CTRL_ENABLE_BIT);

	// Wait for the receive transfer to complete
	u32 BufferHeadIndex = 0;
	u32 TriggerDetected = 0;

	u32 trig_time = 300;

	while (1) {
		if (trig_time > 0) {
			trig_time--;
		} else {
			XGpio_DiscreteWrite(&ManualTriggerGpio, MANUAL_TRIGGER_CHANNEL, 0x1);
			break;
		}
	};

	// wait for trigger hardware to go idle. should be immediate
	while (!(XGpio_DiscreteRead(&TriggerCtrlGpio, TRIGGER_CTRL_CHANNEL) & TRIGGER_CTRL_IDLE_BIT));

	XAxiDma_Pause(&Dma);
	XAxiDma_BdRing *RingPtr = XAxiDma_GetRxRing(&Dma);
	XAxiDma_Bd *BdPtr, *BdCurPtr;
	u32 ProcessedBdCount = XAxiDma_BdRingFromHw(RingPtr, XAXIDMA_ALL_BDS, &BdPtr);
	XAxiDma_BdRingFree(RingPtr, ProcessedBdCount, BdPtr);
	BdCurPtr = BdPtr;
	BdCurPtr = RingPtr->FreeHead; // this is kind of weird. should probably find out why this value isn't the one getting returned from BdRingFree
	for (u32 i = 0; i < NumBds; i++) {
		Xil_DCacheInvalidateRange((UINTPTR)BdCurPtr, XAXIDMA_BD_NUM_WORDS * sizeof(u32));
		XAxiDma_DumpBd(BdCurPtr);
		BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(RingPtr, BdCurPtr);
		u32 Status = XAxiDma_BdGetSts(BdCurPtr);
		// check the tlast bit
		if (Status & XAXIDMA_BD_STS_RXEOF_MASK) {
			u32 ActualLength = XAxiDma_BdGetActualLength(BdCurPtr, (MaxBurstLength*sizeof(u32)*2)-1);
			xil_printf("Last beat found:\r\n");
			xil_printf("  BD base address: %08x\r\n", XAxiDma_BdGetBufAddr(BdCurPtr));
			xil_printf("  BD actual length: %08x\r\n", ActualLength);
			BufferHeadIndex = (((u32)XAxiDma_BdGetBufAddr(BdCurPtr) + (u32)(ActualLength) - (u32)RxBuffer) / sizeof(u32)) % BufferLength;
		}
	}

	TriggerDetected = XGpio_DiscreteRead(&TriggerConfigGpio, TRIGGER_CONFIG_DETECTED_CHANNEL);

	xil_printf("Buffer base address: %08x\r\n", RxBuffer);
	xil_printf("Length of buffer (words): %d\r\n", BufferLength);
	xil_printf("Index of buffer head: %d\r\n", BufferHeadIndex);
	xil_printf("Trigger position: %d\r\n", TriggerPosition);
	xil_printf("Index of trigger position: %d\r\n", (BufferHeadIndex + TriggerPosition) % BufferLength);
	xil_printf("Detected trigger condition: %08x\r\n", TriggerDetected);

	// Invalidate the cache to ensure acquired data can be read
	Xil_DCacheInvalidateRange((UINTPTR)RxBuffer, BufferLengthBytes);

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

	xil_printf("done\r\n");
}
