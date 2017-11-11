#include "mp3.h"
#include "fifo.h"

#include "i2s_dma/i2s_dma.h"

#include "libmad/global.h"
#include "libmad/frame.h"
#include "libmad/stream.h"
#include "libmad/synth.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <stdio.h>
#include <string.h>

#define DMA

#ifdef DMA
#define DMA_BUFFER_SIZE         256
#define DMA_QUEUE_SIZE          5 // 14

// Circular list of descriptors
static dma_descriptor_t dma_block_list[DMA_QUEUE_SIZE];

// Array of buffers for circular list of descriptors
static uint8_t dma_buffer[DMA_QUEUE_SIZE][DMA_BUFFER_SIZE];

// Queue of empty DMA blocks
static QueueHandle_t dma_queue;
#endif

static uint32_t total_samples = 0;

#ifdef DMA
static uint8_t *curr_dma_buf;
static size_t curr_dma_pos;
#endif

void render_sample_block(short *samples, int no_samples)
{
	total_samples += no_samples;

#ifdef DMA
	while (no_samples > 0) {
		if (curr_dma_buf) {
			while (curr_dma_pos < DMA_BUFFER_SIZE && no_samples > 0) {
				uint32_t stereo_sample = *samples;
				stereo_sample |= stereo_sample<<16;
				*(uint32_t *)(curr_dma_buf + curr_dma_pos) = stereo_sample;
				curr_dma_pos += sizeof stereo_sample;
				--no_samples;
			}
			// DMA buffer full
			if (curr_dma_pos >= DMA_BUFFER_SIZE) {
				curr_dma_buf = NULL;
				curr_dma_pos = 0;
			}
			// All samples processed
			if (no_samples <= 0) {
				break;
			}
		}

		// Get a free block from the DMA queue. This call will suspend the task
		// until a free block is available in the queue.
		if (xQueueReceive(dma_queue, &curr_dma_buf, portMAX_DELAY) == pdFALSE) {
			// Or timeout occurs
			printf("Cound't get free blocks to push data\n");
		}
	}
#endif
}

#ifdef DMA
/**
 * Create a circular list of DMA descriptors
 */
static inline void init_descriptors_list()
{
    memset(dma_buffer, 0, DMA_QUEUE_SIZE * DMA_BUFFER_SIZE);

    for (int i = 0; i < DMA_QUEUE_SIZE; i++) {
        dma_block_list[i].owner = 1;
        dma_block_list[i].eof = 1;
        dma_block_list[i].sub_sof = 0;
        dma_block_list[i].unused = 0;
        dma_block_list[i].buf_ptr = dma_buffer[i];
        dma_block_list[i].datalen = DMA_BUFFER_SIZE;
        dma_block_list[i].blocksize = DMA_BUFFER_SIZE;
        if (i == (DMA_QUEUE_SIZE - 1)) {
            dma_block_list[i].next_link_ptr = &dma_block_list[0];
        } else {
            dma_block_list[i].next_link_ptr = &dma_block_list[i + 1];
        }
    }

    // The queue depth is one smaller than the amount of buffers we have,
    // because there's always a buffer that is being used by the DMA subsystem
    // *right now* and we don't want to be able to write to that simultaneously
    dma_queue = xQueueCreate(DMA_QUEUE_SIZE - 1, sizeof(uint8_t*));
	if (dma_queue == NULL) {
		printf("Queue creation failed");
	}
}

// DMA interrupt handler. It is called each time a DMA block is finished processing.
static void dma_isr_handler(void *args)
{
    portBASE_TYPE task_awoken = pdFALSE;

    if (i2s_dma_is_eof_interrupt()) {
        dma_descriptor_t *descr = i2s_dma_get_eof_descriptor();

        if (xQueueIsQueueFullFromISR(dma_queue)) {
            // List of empty blocks is full. Sender don't send data fast enough.
            int dummy;
            //underrun_counter++;
            // Discard top of the queue
            xQueueReceiveFromISR(dma_queue, &dummy, &task_awoken);
        }
        // Push the processed buffer to the queue so sender can refill it.
        xQueueSendFromISR(dma_queue, (void*)(&descr->buf_ptr), &task_awoken);
    }
    i2s_dma_clear_interrupt();

    portEND_SWITCHING_ISR(task_awoken);
}
#endif

uint32_t reset_total_samples()
{
	uint32_t samples = total_samples;
	total_samples = 0;
	return samples;
}

void set_dac_sample_rate(unsigned int sample_rate)
{
//	wm8731_set_sample_rate(sample_rate);
}

/*
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */
static void input(struct mad_stream *stream)
{
	// Maximum MP3 frame size http://www.mars.org/pipermail/mad-dev/2002-January/000428.html
	static unsigned char buffer[2106];//[1441];

	size_t rem = stream->bufend - stream->next_frame;
	memmove(buffer, stream->next_frame, rem);

	fifo_dequeue(buffer + rem, sizeof(buffer) - rem);
	mad_stream_buffer(stream, buffer, sizeof(buffer));
}

/*
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
 * header file.
 */
static void error(struct mad_stream *stream, struct mad_frame *frame)
{
	printf("decoding error: %s (0x%04x)\n",
			mad_stream_errorstr(stream), stream->error);
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
	static struct mad_stream stream;
	static struct mad_frame frame;
	static struct mad_synth synth;

	printf("MAD: Decoder start.\n");

	mad_stream_init(&stream);
	mad_frame_init(&frame);
	mad_synth_init(&synth);

#ifdef DMA
	i2s_clock_div_t clock_div = i2s_get_clock_div(48000 * 2 * 16);
	i2s_pins_t i2s_pins = {.data = true, .clock = true, .ws = true};
	i2s_dma_init(dma_isr_handler, NULL, clock_div, i2s_pins);
	init_descriptors_list();
	i2s_dma_start(dma_block_list);
#endif

	while(1) {
		input(&stream);
		while(1) {
			int r = mad_frame_decode(&frame, &stream);
			if (r == -1) {
				if (!MAD_RECOVERABLE(stream.error)) {
					break; // we're most likely out of buffer and need to call input() again
				}
				error(&stream, &frame); 
				continue;
			}
			mad_synth_frame(&synth, &frame);
		}
	}

#ifdef DMA
	i2s_dma_stop();
	vQueueDelete(dma_queue);
#endif

	vTaskDelete(NULL);
}
