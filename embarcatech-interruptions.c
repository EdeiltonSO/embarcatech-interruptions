#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "led_matrix.pio.h"

#define BLINK_GPIO_PIN      13
#define BLINK_PERIOD_ON_MS  50
#define BLINK_PERIOD_OFF_MS 150

#define MATRIX_GPIO_PIN 7
#define FRAME_DIMENSION 5
#define I_L             1.0

#define INCREMENT_BUTTON_GPIO_PIN 5
#define DECREMENT_BUTTON_GPIO_PIN 6

#define DEBOUNCING_TIME_MS 500

static const double no_number[5][5] = {
    {0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0}, // apagados
    {0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0}
};

static const double numbers[10][5][5] = {
    {
        {I_L, I_L, I_L, I_L, I_L},
        {I_L, 0.0, 0.0, 0.0, I_L},
        {I_L, 0.0, 0.0, 0.0, I_L}, // 0
        {I_L, 0.0, 0.0, 0.0, I_L},
        {I_L, I_L, I_L, I_L, I_L}
    },
    {
        {0.0, 0.0, I_L, 0.0, 0.0},
        {0.0, 0.0, I_L, 0.0, 0.0},
        {0.0, 0.0, I_L, 0.0, 0.0}, // 1
        {0.0, 0.0, I_L, 0.0, 0.0},
        {0.0, I_L, I_L, I_L, 0.0}
    },
    {
        {I_L, I_L, I_L, I_L, I_L},
        {0.0, 0.0, 0.0, 0.0, I_L},
        {I_L, I_L, I_L, I_L, I_L}, // 2
        {I_L, 0.0, 0.0, 0.0, 0.0},
        {I_L, I_L, I_L, I_L, I_L}
    },
    {
        {I_L, I_L, I_L, I_L, I_L},
        {0.0, 0.0, 0.0, 0.0, I_L},
        {0.0, I_L, I_L, I_L, I_L}, // 3
        {0.0, 0.0, 0.0, 0.0, I_L},
        {I_L, I_L, I_L, I_L, I_L}
    },
    {
        {I_L, 0.0, 0.0, 0.0, I_L},
        {I_L, 0.0, 0.0, 0.0, I_L},
        {I_L, I_L, I_L, I_L, I_L}, // 4
        {0.0, 0.0, 0.0, 0.0, I_L},
        {0.0, 0.0, 0.0, 0.0, I_L}
    },
    {
        {I_L, I_L, I_L, I_L, I_L},
        {I_L, 0.0, 0.0, 0.0, 0.0},
        {I_L, I_L, I_L, I_L, I_L}, // 5
        {0.0, 0.0, 0.0, 0.0, I_L},
        {I_L, I_L, I_L, I_L, I_L}
    },
    {
        {I_L, I_L, I_L, I_L, I_L},
        {I_L, 0.0, 0.0, 0.0, 0.0},
        {I_L, I_L, I_L, I_L, I_L}, // 6
        {I_L, 0.0, 0.0, 0.0, I_L},
        {I_L, I_L, I_L, I_L, I_L}
    },
    {
        {I_L, I_L, I_L, I_L, I_L},
        {0.0, 0.0, 0.0, 0.0, I_L},
        {0.0, 0.0, 0.0, I_L, 0.0}, // 7
        {0.0, 0.0, I_L, 0.0, 0.0},
        {0.0, 0.0, I_L, 0.0, 0.0}
    },
    {
        {I_L, I_L, I_L, I_L, I_L},
        {I_L, 0.0, 0.0, 0.0, I_L},
        {I_L, I_L, I_L, I_L, I_L}, // 8
        {I_L, 0.0, 0.0, 0.0, I_L},
        {I_L, I_L, I_L, I_L, I_L}
    },
    {
        {I_L, I_L, I_L, I_L, I_L},
        {I_L, 0.0, 0.0, 0.0, I_L},
        {I_L, I_L, I_L, I_L, I_L}, // 9
        {0.0, 0.0, 0.0, 0.0, I_L},
        {I_L, I_L, I_L, I_L, I_L}
    },
};

uint32_t matrix_rgb(double r, double g, double b);

static void button_irq_handler(uint gpio, uint32_t events);

int current_number, last_time = 0;

// Configurações da PIO
PIO pio;
uint offset;
uint sm;

int main()
{
    stdio_init_all();
    gpio_init(BLINK_GPIO_PIN);
    gpio_set_dir(BLINK_GPIO_PIN, GPIO_OUT);

    gpio_init(INCREMENT_BUTTON_GPIO_PIN);
    gpio_set_dir(INCREMENT_BUTTON_GPIO_PIN, GPIO_IN);
    gpio_pull_up(INCREMENT_BUTTON_GPIO_PIN);

    gpio_init(DECREMENT_BUTTON_GPIO_PIN);
    gpio_set_dir(DECREMENT_BUTTON_GPIO_PIN, GPIO_IN);
    gpio_pull_up(DECREMENT_BUTTON_GPIO_PIN);

    gpio_set_irq_enabled_with_callback(INCREMENT_BUTTON_GPIO_PIN, GPIO_IRQ_EDGE_FALL, true, &button_irq_handler);
    gpio_set_irq_enabled_with_callback(DECREMENT_BUTTON_GPIO_PIN, GPIO_IRQ_EDGE_FALL, true, &button_irq_handler);

    current_number = 0;

    // Configurações da PIO
    pio = pio0;
    set_sys_clock_khz(128000, false);
    offset = pio_add_program(pio, &led_matrix_program);
    sm = pio_claim_unused_sm(pio, true);
    led_matrix_program_init(pio, sm, offset, MATRIX_GPIO_PIN);

    // Limpa a matriz de LEDs
    for (int j = 0; j < FRAME_DIMENSION; j++)
    {
        for (int k = 0; k < FRAME_DIMENSION; k++)
        {
            pio_sm_put_blocking(pio, sm, matrix_rgb(
                no_number[FRAME_DIMENSION - 1 - j][(j + 1) % 2 == 0 ? k : FRAME_DIMENSION - k - 1], 
                no_number[FRAME_DIMENSION - 1 - j][(j + 1) % 2 == 0 ? k : FRAME_DIMENSION - k - 1], 
                no_number[FRAME_DIMENSION - 1 - j][(j + 1) % 2 == 0 ? k : FRAME_DIMENSION - k - 1]
            ));
        }
    }

    // Garante tempo para limpar a matriz antes de desenhar o zero
    sleep_ms(1);
    
    // Desenha o zero inicial do contador
    for (int j = 0; j < FRAME_DIMENSION; j++)
    {
        for (int k = 0; k < FRAME_DIMENSION; k++)
        {
            pio_sm_put_blocking(pio, sm, matrix_rgb(0.0, 0.0, numbers[current_number][FRAME_DIMENSION - 1 - j][(j + 1) % 2 == 0 ? k : FRAME_DIMENSION - k - 1]));
        }
    }

    while (true) {
        gpio_put(BLINK_GPIO_PIN, true);
        sleep_ms(BLINK_PERIOD_ON_MS);
        gpio_put(BLINK_GPIO_PIN, false);
        sleep_ms(BLINK_PERIOD_OFF_MS);
    }
}

void button_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if (current_time - last_time > DEBOUNCING_TIME_MS*1000)
    {
        last_time = current_time;

        if (gpio == INCREMENT_BUTTON_GPIO_PIN)
        {
            current_number = (current_number == 9) ? 0 : current_number + 1;
            for (int i = 0; i < FRAME_DIMENSION; i++)
            {
                for (int j = 0; j < FRAME_DIMENSION; j++)
                { 
                    pio_sm_put_blocking(pio, sm, matrix_rgb(0.0, 0.0, numbers[current_number][FRAME_DIMENSION - 1 - i][(i + 1) % 2 == 0 ? j : FRAME_DIMENSION - j - 1]));
                }
            }
        }
        else
        {
            current_number = (current_number == 0) ? 9 : current_number - 1;
            for (int i = 0; i < FRAME_DIMENSION; i++)
            {
                for (int j = 0; j < FRAME_DIMENSION; j++)
                { 
                    pio_sm_put_blocking(pio, sm, matrix_rgb(0.0, 0.0, numbers[current_number][FRAME_DIMENSION - 1 - i][(i + 1) % 2 == 0 ? j : FRAME_DIMENSION - j - 1]));
                }
            }
        }
    }
}

uint32_t matrix_rgb(double r, double g, double b){
    unsigned char R, G, B;
    R = r * 255;
    G = g * 255;
    B = b * 255;
    return (G << 24) | (R << 16) | (B << 8);
}