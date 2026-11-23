#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "shell.h"
#include "ztimer/periodic.h"

#define LED_PERIOD_MS  (500U)

static ztimer_periodic_t led_timer;
static uint8_t led_state;

static void leds_off(void)
{
    LED0_OFF;
    LED1_OFF;
    LED2_OFF;
}

static bool led_timer_cb(void *arg)
{
    (void)arg;

    leds_off();

    switch (led_state) {
        case 0:
            LED0_ON;
            break;

        case 1:
            LED1_ON;
            break;

        default:
            LED2_ON;
            break;
    }

    led_state++;

    if (led_state >= 3U) {
        led_state = 0U;
    }

    return ZTIMER_PERIODIC_KEEP_GOING;
}

static int cmd_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    puts("Telecomunicaciones 2 - Laboratorio 1");
    puts("Plataforma: TelosB / MSP430F1611");
    puts("LED0 -> LED1 -> LED2, periodo: 500 ms");

    return 0;
}

static const shell_command_t commands[] = {
    { "info", "Muestra informacion del laboratorio", cmd_info },
    { NULL, NULL, NULL }
};

int main(void)
{
    char line_buf[SHELL_DEFAULT_BUFSIZE];

    puts("Hello World con LED - TelosB");
    puts("Secuencia LED0 -> LED1 -> LED2 cada 500 ms");

    leds_off();
    led_state = 0U;

    ztimer_periodic_init(ZTIMER_MSEC,
                         &led_timer,
                         led_timer_cb,
                         NULL,
                         LED_PERIOD_MS);

    ztimer_periodic_start(&led_timer);

    shell_run(commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
