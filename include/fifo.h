#ifndef INCLUDE_FIFO_H_
#define INCLUDE_FIFO_H_

#include <stddef.h>

int fifo_init(void);
void fifo_enqueue(const void *data, size_t len);
void fifo_dequeue(void *data, size_t len);
void fifo_clear(void);
size_t fifo_fill(void);
size_t fifo_free(void);
size_t fifo_size(void);

#endif /* INCLUDE_FIFO_H_ */
