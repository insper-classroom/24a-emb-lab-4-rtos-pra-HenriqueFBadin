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

const uint TRIGGER = 7;
const uint ECHO = 8;

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

QueueHandle_t xQueueTime;
SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueDistance;

void oled1_btn_led_init(void) {
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

void pin_callback(uint gpio, uint32_t events) {

    absolute_time_t fall_time;
    absolute_time_t rise_time;

    if (events == 0x4) 
    { // fall edge
        fall_time = get_absolute_time();
        int dtempo = absolute_time_diff_us(rise_time, fall_time);
        xQueueReset(xQueueTime);
        xQueueSendFromISR(xQueueTime, &dtempo, NULL);
    }
    if (events == 0x8)
    {
        rise_time = get_absolute_time();
    }
}

void echo_task(void *p) {
    while (true) {
        int dtempo;
        if (xQueueReceive(xQueueTime, &dtempo, portMAX_DELAY)) {
            printf("Tempo do Echo: %d us\n", dtempo);
            float distance_cm = (dtempo / 10000.0) * 340.0 / 2.0;
            printf("Distancia: %.2f m\n", distance_cm);
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

    char cnt = 15;
    while (1) {

        float distancia;
        char str_dist[50];
        if (xQueueReceive(xQueueDistance, &distancia, 0) == pdTRUE)
        {
            if (xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(1000)) == pdTRUE)
            {
                xQueueReceive(xQueueDistance, &distancia, portMAX_DELAY);
                sprintf(str_dist, "Distancia: %.2f m\n", distancia);
            }
            else
            {
                sprintf(str_dist, "ERRO\n");
            }
        }
        
        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 1, "A Distancia é:");
        gfx_draw_string(&disp, 0, 10, 1, str_dist);
        gfx_draw_line(&disp, 15, 27, cnt,
                        27);
        if (++cnt == 112)
            cnt = 15;

        gfx_show(&disp);
    }
}

void oled1_demo_1(void *p) {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    char cnt = 15;
    while (1) {

        if (gpio_get(BTN_1_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_1_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 1 - ON");
            gfx_show(&disp);
        } else if (gpio_get(BTN_2_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_2_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 2 - ON");
            gfx_show(&disp);
        } else if (gpio_get(BTN_3_OLED) == 0) {
            cnt = 15;
            gpio_put(LED_3_OLED, 0);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "LED 3 - ON");
            gfx_show(&disp);
        } else {

            gpio_put(LED_1_OLED, 1);
            gpio_put(LED_2_OLED, 1);
            gpio_put(LED_3_OLED, 1);
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "PRESSIONE ALGUM");
            gfx_draw_string(&disp, 0, 10, 1, "BOTAO");
            gfx_draw_line(&disp, 15, 27, cnt,
                          27);
            vTaskDelay(pdMS_TO_TICKS(50));
            if (++cnt == 112)
                cnt = 15;

            gfx_show(&disp);
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
    xQueueDistance = xQueueCreate(32, sizeof(int));

    gpio_set_irq_enabled_with_callback(ECHO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &pin_callback);

    xTaskCreate(echo_task, "ECHO_Task", 4096, NULL, 1, NULL);
    xTaskCreate(trigger_task, "TRIGGER_Task", 4096, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED_Task", 4096, NULL, 1, NULL);

    // xTaskCreate(oled1_demo_1, "Demo 1", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
