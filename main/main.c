/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const int TRIGGER = 7;
const int ECHO = 8;

QueueHandle_t xQueueTime;
SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueDistance;

void pin_callback(uint gpio, uint32_t events) {
    static uint32_t time_start;
    if (events == GPIO_IRQ_EDGE_RISE) {
        time_start = time_us_32();
    } else if (events == GPIO_IRQ_EDGE_FALL) {
        uint32_t time_end = time_us_32();
        uint32_t time_diff = time_end - time_start;
        xQueueReset(xQueueTime);
        xQueueSendFromISR(xQueueTime, &time_diff, NULL);
    }
}

void echo_task(void *p) {
    while (true) {
        int dtempo;
        if (xQueueReceive(xQueueTime, &dtempo, portMAX_DELAY)) {
            printf("Tempo do Echo: %d us\n", dtempo);
            float distance_cm = (dtempo / 10000.0) * 340.0/ 2.0;
            printf("Distancia: %.2f cm\n", distance_cm);
            xQueueReset(xQueueDistance);
            xQueueSend(xQueueDistance, &distance_cm, 0);
        }
    }
}

void trigger_task(void *p) {
    while (true) {
        gpio_put(TRIGGER, 1);
        sleep_us(10);
        gpio_put(TRIGGER, 0);
        xSemaphoreGive(xSemaphoreTrigger);
    }
}

void oled_task(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");

    float distance;
    char str[50];
    while(true){
        if (xSemaphoreTake(xSemaphoreTrigger, 0) == pdTRUE) {
            if (xQueueReceive(xQueueDistance, &distance,  0)) {
                sprintf(str, "%.1f cm", distance);
                if (distance <= 220)
                {
                    gfx_clear_buffer(&disp);
                    gfx_draw_string(&disp, 0, 0, 1, "Distancia: ");
                    gfx_draw_string(&disp, 0, 10, 1, str);
                    gfx_draw_line(&disp, 0, 27, distance, 27);
                    gfx_show(&disp);
                }
                else
                {
                    gfx_clear_buffer(&disp);
                    gfx_draw_string(&disp, 0, 0, 1, "Distancia: ");
                    gfx_draw_string(&disp, 0, 10, 1, "Muito Grande");
                    gfx_draw_line(&disp, 0, 27, 10, 27);
                    gfx_show(&disp);
                }
                
            } else {
                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, "Distancia: ");
                gfx_draw_string(&disp, 0, 10, 1, "ERROR");
                gfx_draw_line(&disp, 0, 27, 15, 27);
                gfx_show(&disp);
            }
       } 
    }
}

int main() {
    stdio_init_all();

    gpio_init(TRIGGER);
    gpio_set_dir(TRIGGER, GPIO_OUT);

    gpio_init(ECHO);
    gpio_set_dir(ECHO, GPIO_IN);

    xQueueTime = xQueueCreate(32, sizeof(int));
    xSemaphoreTrigger = xSemaphoreCreateBinary();
    xQueueDistance = xQueueCreate(32, sizeof(float));

    gpio_set_irq_enabled_with_callback(ECHO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &pin_callback);

    xTaskCreate(echo_task, "ECHO_Task", 4096, NULL, 1, NULL);
    xTaskCreate(trigger_task, "TRIGGER_Task", 4096, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED_Task", 4096, NULL, 1, NULL);
    vTaskStartScheduler();

    while (true)
        ;
}
