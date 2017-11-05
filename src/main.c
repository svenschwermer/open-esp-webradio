#include "fifo.h"
#include "wm8731.h"
#include "stream_client.h"
#include "mp3.h"

#include "esp8266.h"
#include "esp/uart.h"
#include "esp/hwrand.h"
#include "espressif/esp_common.h"
#include "ssid_config.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

#include <stdio.h>

static struct stream_params stream_params = {
    .host = "icecast.omroep.nl",
    .path = "/3fm-sb-mp3"
};

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

static TaskHandle_t mp3_task_hndl = NULL;
static TaskHandle_t stream_task_hndl = NULL;

void debug_timer(TimerHandle_t xTimer)
{
    TickType_t period_ms = xTimerGetPeriod(xTimer) * portTICK_PERIOD_MS;
    float freq = 1000.f / period_ms;
    float samplerate = reset_total_samples() * freq;
    float datarate = reset_total_bytes() * freq;
    eTaskState state_mp3 = eTaskGetState(mp3_task_hndl);
    eTaskState state_stream = eTaskGetState(stream_task_hndl);
    printf("stream: %f Bytes/s\n"
        "decoder: %f samples/s\n"
        "fifo: %u/%u\n"
        "states: %d,%d\n\n",
        datarate,
        samplerate,
        fifo_fill(), fifo_size(),
        state_mp3, state_stream);
}

static void mp3_test(void *arg)
{
    static unsigned char buffer[2106];
    while (1) {
        fifo_dequeue(buffer, sizeof buffer);
        uint32_t delay = hwrand() % 4096;
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}

static void stream_test(void *arg)
{
    // produce 12000 bytes/s
    uint8_t buf[120];
    const TickType_t xFrequency = pdMS_TO_TICKS(10);
    TickType_t xLastWakeTime = xTaskGetTickCount();
   
    while (1)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        hwrand_fill(buf, sizeof buf);
        fifo_enqueue(buf, sizeof buf);
    }
}

// Just consume all the data that's being put into the FIFO
static void init_task(void *arg)
{
    printf("Waiting for the WiFi to connect...\n");
    uint8_t connection_status = STATION_IDLE;
    while (connection_status != STATION_GOT_IP)
    {
        connection_status = sdk_wifi_station_get_connect_status();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Got IP, starting tasks...\n");

    if (xTaskCreate(mp3_task, "consumer", 2100, NULL, 2, &mp3_task_hndl) != pdPASS)
        printf("Failed to create mp3 task!\n");

    if (xTaskCreate(stream_task, "producer", 1024, &stream_params, 2, &stream_task_hndl) != pdPASS)
        printf("Failed to create stream task!\n");
    
    TimerHandle_t timer = xTimerCreate("debug", pdMS_TO_TICKS(3000), pdTRUE, NULL, debug_timer);
    xTimerStart(timer, 0);
    
    vTaskDelete(NULL);
}

void user_init(void)
{
    uart_set_baud(0, 115200);
 
    if(fifo_init() != 0)
    {
    	printf("fifo init failed\n");
    	return;
    }

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

	xTaskCreate(init_task, "init", 512, NULL, 2, NULL);
}
