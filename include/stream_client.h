#ifndef STREAM_CLIENT_H_
#define STREAM_CLIENT_H_

#include <stdint.h>

struct stream_params
{
    const char *host;
    const char *path;
};

void stream_task(void *arg);
unsigned int get_and_reset_streamed_bytes(void);

#endif /* STREAM_CLIENT_H_ */
