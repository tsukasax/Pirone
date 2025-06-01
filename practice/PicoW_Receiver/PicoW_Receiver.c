#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "receiver.pio.h"

#define RCV_CH_1 6
#define RCV_CH_2 7
#define RCV_CH_3 8
#define RCV_CH_4 9

PIO pio_0 = pio0;
uint sm_0 = 0;
uint sm_1 = 1;
uint sm_2 = 2;
uint sm_3 = 3;

uint32_t ch1w, ch2w, ch3w, ch4w;

volatile bool timer_flag_2s = false;        // 2秒フラグ
volatile bool timer_flag_1s = false;        // 1秒フラグ
volatile bool timer_flag_50ms = false;      // 50ミリ秒フラグ
uint32_t timer_loop = 0;

/*******************
タイマー割り込み処理
*******************/
bool timer_callback(struct repeating_timer *t) {
    ++timer_loop;
    if (timer_loop % 50 == 0) timer_flag_50ms = true;   // Serial出力
    if (timer_loop % 1000 == 0) timer_flag_1s = true;
    if (timer_loop % 2000 == 0) timer_flag_2s = true;
    return true;
}

/*******************
PIO 割り込み処理
*******************/
void irq_handler(){
    pio0_hw->irq = 1;

    ch1w = 5000 - pio_sm_get(pio_0, sm_0);
    ch2w = 5000 - pio_sm_get(pio_0, sm_1);
    ch3w = 5000 - pio_sm_get(pio_0, sm_2);
    ch4w = 5000 - pio_sm_get(pio_0, sm_3);
}


/*******************
メイン関数
*******************/
int main()
{
    stdio_init_all();
    sleep_ms(1000);     // USBポート認識待ち遅延

    // プロポからの受信信号用 PIO設定
    uint offset_0 = pio_add_program(pio_0, &receiver_program);    
    receiver_program_init(pio_0, sm_0, offset_0, RCV_CH_1);
    receiver_program_init(pio_0, sm_1, offset_0, RCV_CH_2);
    receiver_program_init(pio_0, sm_2, offset_0, RCV_CH_3);
    receiver_program_init(pio_0, sm_3, offset_0, RCV_CH_4);

    // PIO IRQハンドラの設定
    irq_set_exclusive_handler(PIO0_IRQ_0, irq_handler);
    irq_set_enabled(PIO0_IRQ_0, true);
    pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS;

    // 1msでのtimer割り込み
    struct repeating_timer timer;
    add_repeating_timer_ms(-1, timer_callback, NULL, &timer);

    // PIO0の全smを同時スタート
    pio_enable_sm_mask_in_sync(pio_0, 15);

    while (true) {
        if (timer_flag_50ms) {
            printf("%d,%d,%d,%d\n", ch1w, ch2w, ch3w, ch4w);
            timer_flag_50ms = false;
        }
    }
}
