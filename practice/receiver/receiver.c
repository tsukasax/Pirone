#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "receiver.pio.h"

#define RCV_CH_1 6

uint32_t ch1w;

int main()
{
    stdio_init_all();

    // PIO 
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &receiver_program);
    uint sm_1 = pio_claim_unused_sm(pio, true);
    
    receiver_program_init(pio, sm_1, offset, RCV_CH_1);
    
    while (true) {
        ch1w = 5000 - pio_sm_get_blocking(pio, sm_1);
        printf("ch1 = %d\n", ch1w);
        sleep_ms(50);
    }
}
