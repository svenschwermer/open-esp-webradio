#include "fifo.h"
#include "wm8731.h"
#include "stream_client.h"

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

// Continuously print fifo fill level info
static void fifo_status_task(void *arg)
{
	while (1)
	{
		printf("fifo fill status: %u/%u\n", fifo_fill(), fifo_size());
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}

uint32_t consumed_data = 0;

void datarate_timer(TimerHandle_t xTimer)
{
    TickType_t period_ms = xTimerGetPeriod(xTimer) * portTICK_PERIOD_MS;
    float datarate = consumed_data;
    consumed_data = 0;
    datarate = datarate * 1000.f / period_ms;
    printf("datarate: %f Bytes/s\n", datarate);
}

// Just consume all the data that's being put into the FIFO
static void consumer_task(void *arg)
{
    uint8_t buf[32];
    while (1)
    {
        fifo_dequeue(buf, sizeof buf);
        consumed_data += sizeof buf;
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

    xTaskCreate(consumer_task, "consumer", 1024, NULL, 2, NULL);
    xTaskCreate(stream_task, "producer", 1024, &stream_params, 2, NULL);
//    xTaskCreate(fifo_status_task, "status", 1024, NULL, 2, NULL);

    TimerHandle_t timer = xTimerCreate("datarate", pdMS_TO_TICKS(3000), pdTRUE, NULL, datarate_timer);
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
