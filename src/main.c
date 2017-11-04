#include "fifo.h"
#include "wm8731.h"

#include "esp/uart.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "esp8266.h"

#include <stdio.h>

void dummy_task(void *pvParameters)
{

}

void user_init(void)
{
    uart_set_baud(0, 115200);
    
    if(fifo_init() != 0)
    {
    	printf("fifo init failed\n");
    	return;
    }

    if(wm8731_init() != 0)
    {
    	printf("dac init failed\n");
    	return;
    }

    xTaskCreate(dummy_task, "dummy task", 1024, NULL, 2, NULL);
}
