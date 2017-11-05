#ifndef STREAM_CLIENT_H_
#define STREAM_CLIENT_H_

struct stream_params
{
    const char *host;
    const char *path;
};

void stream_task(void *arg);

#endif /* STREAM_CLIENT_H_ */
