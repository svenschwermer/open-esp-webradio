#include "fifo.h"
#include "spiram.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stdlib.h>

#define FIFO_SIZE SPIRAMSIZE

static uint32_t writePos = 0;
static uint32_t readPos = 0;
static uint32_t fill = 0;

static SemaphoreHandle_t mtx;
//static xTaskHandle xProducerWaiting = NULL;
static SemaphoreHandle_t semphrWrite;
//static xTaskHandle xConsumerWaiting = NULL;
static SemaphoreHandle_t semphrRead;

int fifo_init(void)
{
	mtx = xSemaphoreCreateMutex();
	vSemaphoreCreateBinary(semphrWrite);
	vSemaphoreCreateBinary(semphrRead);

	spiRamInit();

	return spiRamTest();
}

static inline int fifo_enqueue_internal(const void * const data, const uint32_t len) // aka produce
{
	int written;

	xSemaphoreTake(mtx, portMAX_DELAY);

	while(len > (FIFO_SIZE - fill) )
	{
		//xProducerWaiting = xTaskGetCurrentTaskHandle();
		xSemaphoreGive(mtx);
		//ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		xSemaphoreTake(semphrWrite, portMAX_DELAY);
		xSemaphoreTake(mtx, portMAX_DELAY);
	}

	written = spiRamWrite(writePos, data, len);
	writePos = (writePos + written) % FIFO_SIZE;
	fill += written;

	/*
	if(xConsumerWaiting != NULL)
	{
		xTaskNotifyGive(xConsumerWaiting);
		xConsumerWaiting = NULL;
	}
	*/
	xSemaphoreGive(semphrRead);

	xSemaphoreGive(mtx);

	return written;
}

void fifo_enqueue(const void *data, uint32_t len)
{
	while(len > 0)
	{
		const int written = fifo_enqueue_internal(data, len);
		data = ( (const char *) data ) + written;
		len -= written;
	}
}

static inline int fifo_dequeue_internal(void * const data, const uint32_t len) // aka consume
{
	int read;

	xSemaphoreTake(mtx, portMAX_DELAY);

	while(len > fill)
	{
		//xConsumerWaiting = xTaskGetCurrentTaskHandle();
		xSemaphoreGive(mtx);
		//ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		xSemaphoreTake(semphrRead, portMAX_DELAY);
		xSemaphoreTake(mtx, portMAX_DELAY);
	}

	read = spiRamRead(readPos, data, len);
	readPos = (readPos + read) % FIFO_SIZE;
	fill -= read;

	/*
	if(xProducerWaiting != NULL)
	{
		xTaskNotifyGive(xProducerWaiting);
		xProducerWaiting = NULL;
	}
	*/
	xSemaphoreGive(semphrWrite);

	xSemaphoreGive(mtx);

	return read;
}

void fifo_dequeue(void *data, uint32_t len)
{
	while(len > 0)
	{
		const int read = fifo_dequeue_internal(data, len);
		data = ( (char *) data ) + read;
		len -= read;
	}
}

uint32_t fifo_fill(void)
{
	uint32_t ret;
	xSemaphoreTake(mtx, portMAX_DELAY);
	ret = fill;
	xSemaphoreGive(mtx);
	return ret;
}

uint32_t fifo_free(void)
{
	return FIFO_SIZE - fifo_fill();
}

uint32_t fifo_size(void)
{
	return FIFO_SIZE;
}
