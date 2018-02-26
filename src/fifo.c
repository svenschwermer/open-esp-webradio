#include "fifo.h"
#include "spiram.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define FIFO_SIZE SPIRAM_SIZE

static uint32_t write_pos = 0;
static uint32_t read_pos = 0;
static uint32_t fill = 0;

static SemaphoreHandle_t mtx;
static TaskHandle_t producer_waiting = NULL;
static TaskHandle_t consumer_waiting = NULL;

int fifo_init(void) {
  mtx = xSemaphoreCreateMutex();
  if (mtx == NULL)
    return 1;

  if (spiram_init())
    return 1;

  return spiram_test();
}

static inline size_t fifo_enqueue_internal(const void *const data,
                                           const size_t len) // aka produce
{
  xSemaphoreTake(mtx, portMAX_DELAY);

  while (len > (FIFO_SIZE - fill)) {
    producer_waiting = xTaskGetCurrentTaskHandle();
    xSemaphoreGive(mtx);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    xSemaphoreTake(mtx, portMAX_DELAY);
  }

  size_t written = spiram_write(write_pos, data, len);
  write_pos = (write_pos + written) % FIFO_SIZE;
  fill += written;

  if (consumer_waiting != NULL) {
    xTaskNotifyGive(consumer_waiting);
    consumer_waiting = NULL;
  }

  xSemaphoreGive(mtx);

  return written;
}

void fifo_enqueue(const void *data, size_t len) {
  const uint8_t *byte_buf = data;
  while (len > 0) {
    const size_t written = fifo_enqueue_internal(byte_buf, len);
    byte_buf += written;
    len -= written;
  }
}

static inline size_t fifo_dequeue_internal(void *const data,
                                           const size_t len) // aka consume
{
  xSemaphoreTake(mtx, portMAX_DELAY);

  while (len > fill) {
    consumer_waiting = xTaskGetCurrentTaskHandle();
    xSemaphoreGive(mtx);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    xSemaphoreTake(mtx, portMAX_DELAY);
  }

  size_t read = spiram_read(read_pos, data, len);
  read_pos = (read_pos + read) % FIFO_SIZE;
  fill -= read;

  if (producer_waiting != NULL) {
    xTaskNotifyGive(producer_waiting);
    producer_waiting = NULL;
  }

  xSemaphoreGive(mtx);

  return read;
}

void fifo_dequeue(void *data, size_t len) {
  uint8_t *byte_buf = data;
  while (len > 0) {
    const size_t read = fifo_dequeue_internal(byte_buf, len);
    byte_buf += read;
    len -= read;
  }
}

void fifo_clear(void) {
  xSemaphoreTake(mtx, portMAX_DELAY);

  write_pos = 0;
  read_pos = 0;
  fill = 0;

  if (producer_waiting != NULL) {
    xTaskNotifyGive(producer_waiting);
    producer_waiting = NULL;
  }

  xSemaphoreGive(mtx);
}

size_t fifo_fill(void) {
  uint32_t ret;
  xSemaphoreTake(mtx, portMAX_DELAY);
  ret = fill;
  xSemaphoreGive(mtx);
  return ret;
}

size_t fifo_free(void) { return FIFO_SIZE - fifo_fill(); }

size_t fifo_size(void) { return FIFO_SIZE; }
