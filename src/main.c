#include "fifo.h"
#include "wm8731.h"
#include "spiram.h"

#include "esp/uart.h"
#include "esp/hwrand.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "esp8266.h"

#include "crc_generic/crc_lib/crc_generic.h"

#include <stdio.h>

void producer_task(void *pvParameters)
{
    uint8_t buf[32];
    while (true)
    {
        const size_t len = hwrand() % sizeof buf;
        hwrand_fill(buf, len);
        printf("enqueuing %u bytes\n", len);
        fifo_enqueue(buf, len);
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void consumer_task(void *pvParameters)
{
    uint8_t buf[32];
    while (true)
    {
        fifo_dequeue(buf, sizeof buf);
        printf("32 bytes dequeued\n");
    }
}

void hexdump(const void * buf, size_t len)
{
    size_t i = 0;
    const uint8_t * byte_buf = buf;
    while (i < len)
    {
        for (size_t j=0; j < 16 && i < len; ++j, ++i)
            printf("%02x ", byte_buf[i]);
        printf("\n");
    }
}

void dummy_task(void *pvParameters)
{
    uint8_t buf[256];
    config_crc_8 crc;

    //init crc parameters  (MAXIM parameters)
    crc_8_generic_init(&crc, 0x31, 8, 0x00, 0x00, 1, 1, 1);
    crc_8_generic_select_algo(&crc, crc_8_tab_MAXIM, sizeof crc_8_tab_MAXIM, CRC_TABLE_FAST);

    spiram_init();

    while (true)
    {
        const uint32_t len = hwrand() % sizeof buf;
        const uint32_t addr = hwrand() % SPIRAM_SIZE;
        crc_8 write_crc, read_crc;

        hwrand_fill(buf, len);

        printf("writing (%u bytes @ 0x%05x):\n", len, addr);
        hexdump(buf, len);
        write_crc = crc_8_generic_compute(&crc, buf, len);
        printf("CRC: 0x%02x\n", write_crc);
        for (size_t written=0; written < len;)
            written += spiram_write((addr + written) % SPIRAM_SIZE, buf + written, len - written);

        // clear buffer
        for (size_t i=0; i < len; ++i)
            buf[i] = 0x00;

        for (size_t read=0; read < len;)
            read += spiram_read((addr + read) % SPIRAM_SIZE, buf + read, len - read);
        printf("read back:\n");
        hexdump(buf, len);
        read_crc = crc_8_generic_compute(&crc, buf, len);
        printf("CRC: 0x%02x\n", read_crc);

        if (write_crc == read_crc)
            printf("\nCRCs match!\n\n");
        else
            printf("\nCRCs don't match!\n\n");

        vTaskDelay(3000/portTICK_PERIOD_MS);
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
 
    if(fifo_init() != 0)
    {
    	printf("fifo init failed\n");
    	return;
    }
/*
    if(wm8731_init() != 0)
    {
    	printf("dac init failed\n");
    	return;
    }
*/
    xTaskCreate(consumer_task, "consumer", 1024, NULL, 2, NULL);
    xTaskCreate(producer_task, "producer", 1024, NULL, 2, NULL);

//    xTaskCreate(dummy_task, "dummy", 1024, NULL, 2, NULL);
}
