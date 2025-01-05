#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

// 信号ピン
#define DIR_PIN 14
#define STEP_PIN 15

uint16_t roll_volume;
uint16_t before_roll_volume = 2047;
uint16_t move_value;


/*******************
メイン関数
*******************/
int main() {
    stdio_init_all();

    // ADC初期化; GPIO26,ADC0を使用
    adc_init();
    adc_gpio_init(26);

    // ステッピングモータ用GPIO設定
    gpio_init(DIR_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_init(STEP_PIN);
    gpio_set_dir(STEP_PIN, GPIO_OUT);

    gpio_put(DIR_PIN, 0);
    gpio_put(STEP_PIN, 0);

    while (true) {
        // ADC0から値を読み込み
        adc_select_input(0);
        roll_volume = adc_read();
        
        if (roll_volume > before_roll_volume + 30 || roll_volume < before_roll_volume - 30) {
            if (roll_volume > before_roll_volume) {
                gpio_put(DIR_PIN, 0);
                move_value = roll_volume - before_roll_volume;
            }else {
                gpio_put(DIR_PIN, 1);
                move_value = before_roll_volume - roll_volume;
            }

            for (int i = 0; i < move_value; i++) {
                gpio_put(STEP_PIN, 1);
                sleep_us(700);
                gpio_put(STEP_PIN, 0);
                sleep_us(700);
            }
            printf("volume : %d, before : %d, move : %d\n", roll_volume, before_roll_volume, move_value);
            before_roll_volume = roll_volume;
        }        
        sleep_ms(10);
    }
}
