#ifndef MP3_H_
#define MP3_H_

#include <stdint.h>

int mp3_start(void);
void mp3_stop(void);
unsigned int get_and_reset_underrun_counter(void);

#endif /* MP3_H_ */
