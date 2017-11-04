#ifndef INCLUDE_FIFO_H_
#define INCLUDE_FIFO_H_

#include <stdint.h>

int fifo_init(void);
void fifo_enqueue(const void *data, uint32_t len);
void fifo_dequeue(void *data, uint32_t len);
uint32_t fifo_fill(void);
uint32_t fifo_free(void);
uint32_t fifo_size(void);

#endif /* INCLUDE_FIFO_H_ */
