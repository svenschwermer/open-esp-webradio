#include "mp3.h"
#include "fifo.h"

#include "global.h"
#include "decoder.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

#include "esp/gpio.h"

static uint32_t total_samples = 0;

void render_sample_block(short *short_sample_buff, int no_samples)
{
	total_samples += no_samples;
}

uint32_t reset_total_samples()
{
	uint32_t samples = total_samples;
	total_samples = 0;
	return samples;
}

void set_dac_sample_rate(unsigned int samplerate)
{

}

/*
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */
static enum mad_flow input(void *cb_data, struct mad_stream *stream)
{
	// Maximum MP3 frame size http://www.mars.org/pipermail/mad-dev/2002-January/000428.html
	static unsigned char buffer[2106];//[1441];

	size_t rem = stream->bufend - stream->next_frame;
	memmove(buffer, stream->next_frame, rem);

	fifo_dequeue(buffer + rem, sizeof(buffer) - rem);
	mad_stream_buffer(stream, buffer, sizeof(buffer));

	return MAD_FLOW_CONTINUE;
}

/*
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
 * header file.
 */
static enum mad_flow error(void *cb_data, struct mad_stream *stream, struct mad_frame *frame)
{
	printf("decoding error: %s (0x%04x)\n",
			mad_stream_errorstr(stream), stream->error);

	/* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */

	return MAD_FLOW_IGNORE;
}

/*
 * This is the function called by main() above to perform all the decoding.
 * It instantiates a decoder object and configures it with the input,
 * output, and error callback functions above. A single call to
 * mad_decoder_run() continues until a callback function returns
 * MAD_FLOW_STOP (to stop decoding) or MAD_FLOW_BREAK (to stop decoding and
 * signal an error).
 */
void mp3_task(void *arg)
{
	int r;
	static struct mad_stream stream;
	static struct mad_frame frame;
	static struct mad_synth synth;

	printf("MAD: Decoder start.\n");

	mad_stream_init(&stream);
	mad_frame_init(&frame);
	mad_synth_init(&synth);

	while(1) {
		input(NULL, &stream);
		while(1) {
			r=mad_frame_decode(&frame, &stream);
			if (r==-1) {
				if (!MAD_RECOVERABLE(stream.error)) {
					break; // we're most likely out of buffer and need to call input() again
				}
				error(NULL, &stream, &frame); 
				continue;
			}
			mad_synth_frame(&synth, &frame);
		}
	}

	vTaskDelete(NULL);
}
