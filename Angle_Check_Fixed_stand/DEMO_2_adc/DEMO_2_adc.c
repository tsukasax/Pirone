#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

int main() {
    stdio_init_all();
    adc_init();

    adc_gpio_init(26);
    adc_gpio_init(27);
    adc_gpio_init(28);

    while (1) {
        const float conversion_factor = 3.3f / (1 << 12);

        // ROLL
        adc_select_input(0);
        uint16_t result_1 = adc_read();
        float vol_1 = result_1 * conversion_factor;

        // PITCH
        adc_select_input(1);
        uint16_t result_2 = adc_read();
        float vol_2 = result_2 * conversion_factor;

        // YAW
        adc_select_input(2);
        uint16_t result_3 = adc_read();
        float vol_3 = result_3 * conversion_factor;
        
        printf("ROLL : %0.1f V, PITCH : %0.1f V, YAW : %0.1f V\n", vol_1, vol_2, vol_3);
        sleep_ms(200);
    }
}
