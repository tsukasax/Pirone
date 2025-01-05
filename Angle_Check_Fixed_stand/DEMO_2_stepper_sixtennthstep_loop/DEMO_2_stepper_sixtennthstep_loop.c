#include <stdio.h>
#include "pico/stdlib.h"

#define DIR_PIN 14
#define STEP_PIN 15

int main() {
    stdio_init_all();

    // GPIOの初期化
    gpio_init(DIR_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_init(STEP_PIN);
    gpio_set_dir(STEP_PIN, GPIO_OUT);

    gpio_put(DIR_PIN, 0);
    gpio_put(STEP_PIN, 0);

    while (true) {
        // 時計回りに360°回転（3200ステップ）
        gpio_put(DIR_PIN, 1);
        for (int i = 0; i < 3200; i++) {
            gpio_put(STEP_PIN, 1);
            sleep_us(100);
            gpio_put(STEP_PIN, 0);
            sleep_us(100);
        }
        sleep_ms(1000);

        // 反時計回りに360°回転（3200ステップ）
        gpio_put(DIR_PIN, 0);
        for (int i = 0; i < 3200; i++) {
            gpio_put(STEP_PIN, 1);
            sleep_us(100);
            gpio_put(STEP_PIN, 0);
            sleep_us(100);
        }
        sleep_ms(1000);

        // 時計回りに90°回転（800ステップ）
        gpio_put(DIR_PIN, 1);
        for (int i = 0; i < 800; i++) {
            gpio_put(STEP_PIN, 1);
            sleep_us(1000);
            gpio_put(STEP_PIN, 0);
            sleep_us(1000);
        }
        sleep_ms(1000);

        // 反時計回りに180°回転（1600ステップ）
        gpio_put(DIR_PIN, 0);
        for (int i = 0; i < 1600; i++) {
            gpio_put(STEP_PIN, 1);
            sleep_us(1000);
            gpio_put(STEP_PIN, 0);
            sleep_us(1000);
        }
        sleep_ms(1000);

        // 時計回りに90°回転（800ステップ）
        gpio_put(DIR_PIN, 1);
        for (int i = 0; i < 800; i++) {
            gpio_put(STEP_PIN, 1);
            sleep_us(1000);
            gpio_put(STEP_PIN, 0);
            sleep_us(1000);
        }
        sleep_ms(1000);
    }
}
