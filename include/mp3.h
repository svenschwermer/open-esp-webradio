#ifndef MP3_H_
#define MP3_H_

#include <stdint.h>

void mp3_task(void *arg);
unsigned int get_and_reset_underrun_counter(void);

#endif /* MP3_H_ */
