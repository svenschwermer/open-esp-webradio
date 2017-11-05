#ifndef STREAM_CLIENT_H_
#define STREAM_CLIENT_H_

#include <stdint.h>

struct stream_params
{
    const char *host;
    const char *path;
};

void stream_task(void *arg);
uint32_t reset_total_samples(void);

#endif /* STREAM_CLIENT_H_ */
