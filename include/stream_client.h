#ifndef STREAM_CLIENT_H_
#define STREAM_CLIENT_H_

typedef void (*stream_up_cb)(void);
typedef void (*stream_metadata_cb)(const char *);

int stream_start(const char *host, const char *path, stream_up_cb up,
                 stream_metadata_cb meta);
void stream_stop(void);

#endif /* STREAM_CLIENT_H_ */
