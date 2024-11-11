#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "receiver.pio.h"

#define RCV_CH_1 6
#define RCV_CH_2 7
#define RCV_CH_3 8
#define RCV_CH_4 9

uint32_t ch1w, ch2w, ch3w, ch4w;

PIO pio = pio0;
uint sm_0 = 0;
uint sm_1 = 1;
uint sm_2 = 2;
uint sm_3 = 3;

static void irq_handler(){
    irq_clear(PIO0_IRQ_0);
    pio0_hw->irq = 1;

    // int t_start = time_us_32();
    ch1w = 5000 - pio_sm_get(pio, sm_0);
    ch2w = 5000 - pio_sm_get(pio, sm_1);
    ch3w = 5000 - pio_sm_get(pio, sm_2);
    ch4w = 5000 - pio_sm_get(pio, sm_3);

    // printf("%d\n", ch1w);
    // printf("ch1:%d,ch2:%d\n", ch1w, ch2w);
    printf("ch1:%d,ch2:%d,ch3:%d,ch4:%d\n", ch1w, ch2w, ch3w, ch4w);
    // printf("%d,%d,%d,%d\n", ch1w, ch2w, ch3w, ch4w);
    // printf("Time = %d\n", time_us_32() - t_start);
}

int main()
{
    stdio_init_all();
    sleep_ms(1000);

    uint offset = pio_add_program(pio, &receiver_program);    
    receiver_program_init(pio, sm_0, offset, RCV_CH_1);
    receiver_program_init(pio, sm_1, offset, RCV_CH_2);
    receiver_program_init(pio, sm_2, offset, RCV_CH_3);
    receiver_program_init(pio, sm_3, offset, RCV_CH_4);

    // IRQハンドラの設定
    irq_set_exclusive_handler(PIO0_IRQ_0, irq_handler);
    irq_set_enabled(PIO0_IRQ_0, true);
    pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS;

    
    while (true) {
        sleep_ms(100);
        // printf("ch1:%d\n", ch1w);
        // printf("ch1:%d,ch2:%d,ch3:%d,ch4:%d\n", ch1w, ch2w, ch3w, ch4w);
    }
}
