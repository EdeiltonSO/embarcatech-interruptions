#include <stdio.h>
#include "pico/stdlib.h"

#define BLINK_GPIO_PIN      13
#define BLINK_PERIOD_ON_MS  50
#define BLINK_PERIOD_OFF_MS 150

int main()
{
    stdio_init_all();

    gpio_init(BLINK_GPIO_PIN);
    gpio_set_dir(BLINK_GPIO_PIN, GPIO_OUT);
    
    while (true) {
        gpio_put(BLINK_GPIO_PIN, true);
        sleep_ms(BLINK_PERIOD_ON_MS);
        gpio_put(BLINK_GPIO_PIN, false);
        sleep_ms(BLINK_PERIOD_OFF_MS);
    }
}