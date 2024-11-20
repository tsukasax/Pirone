#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define MOTOR1_PIN 14      // PWM:7A
#define MOTOR2_PIN 17      // PWM:0B
#define MOTOR3_PIN 15      // PWM:7B
#define MOTOR4_PIN 16      // PWM:0A

// arduinoのPWM周波数490Hzに合わせて約490Hzで設定
float div_value = 125;      // クロックの分周
uint16_t wrap_value = 2040; // 一周期のカウント数

float motor1_duty = 1000;
float motor2_duty = 1000;
float motor3_duty = 1000;
float motor4_duty = 1000;

uint pwm_mask_set;

volatile bool timer_flag_1s = false;
uint32_t timer_loop = 0;

/*******************
タイマー割り込み処理
*******************/
bool timer_callback(struct repeating_timer *t) {
    timer_loop += 1;
    if (timer_loop % 1000 == 0) timer_flag_1s = true;   // 1秒のカウント
    return true;
}


/*******************
メイン関数
*******************/
int main() {
    stdio_init_all();
    sleep_ms(1000);     // USBの認識待ち
    printf("Hello Motor!\n");

    gpio_set_function(MOTOR1_PIN, GPIO_FUNC_PWM);
    gpio_set_function(MOTOR2_PIN, GPIO_FUNC_PWM);
    gpio_set_function(MOTOR3_PIN, GPIO_FUNC_PWM);
    gpio_set_function(MOTOR4_PIN, GPIO_FUNC_PWM);

    // motor1の設定
    // motor1とmotor3は同一スライスNo.となるので統合
    uint motor1_slice_num = pwm_gpio_to_slice_num(MOTOR1_PIN);
    pwm_config motor1_config = pwm_get_default_config();
    pwm_config_set_clkdiv(&motor1_config, div_value);
    pwm_init(motor1_slice_num, &motor1_config, false);
    pwm_set_wrap(motor1_slice_num, wrap_value);
    pwm_set_chan_level(motor1_slice_num, PWM_CHAN_A, motor1_duty);
    pwm_set_chan_level(motor1_slice_num, PWM_CHAN_B, motor3_duty);

    // motor2の設定
    // motor2とmotor4は同一スライスNo.となるので統合
    uint motor2_slice_num = pwm_gpio_to_slice_num(MOTOR2_PIN);
    pwm_config motor2_config = pwm_get_default_config();
    pwm_config_set_clkdiv(&motor2_config, div_value);
    pwm_init(motor2_slice_num, &motor2_config, false);
    pwm_set_wrap(motor2_slice_num, wrap_value);
    pwm_set_chan_level(motor2_slice_num, PWM_CHAN_B, motor2_duty);
    pwm_set_chan_level(motor2_slice_num, PWM_CHAN_A, motor4_duty);

    // PWMの全スライス同時スタート
    pwm_mask_set = (0x01 << motor1_slice_num) | (0x01 << motor2_slice_num);
    // printf("mask: %d\n\n", pwm_mask_set);
    pwm_set_mask_enabled(pwm_mask_set);

    // タイマー割り込みの設定、1ms毎
    struct repeating_timer timer;
    add_repeating_timer_ms(-1, timer_callback, NULL, &timer);


    while (true) {
        // 一秒毎に実行
        if (timer_flag_1s) {
            // rand()はint型でMax.が「2,147,483,647」となる。これを「0～wrap_value」の範囲へ変換
            // 式は「最小値 + rand() * (最大値 - 最小値 + 1.0) / (1.0 + RAND_MAX))」となる
            int rand_value_1 = (0 + rand() * (wrap_value - 0 + 1.0) / (1.0 + RAND_MAX));
            int rand_value_2 = (0 + rand() * (wrap_value - 0 + 1.0) / (1.0 + RAND_MAX));
            int rand_value_3 = (0 + rand() * (wrap_value - 0 + 1.0) / (1.0 + RAND_MAX));
            int rand_value_4 = (0 + rand() * (wrap_value - 0 + 1.0) / (1.0 + RAND_MAX));

            // printf("RAND_MAX: %d\n\n", RAND_MAX);
            printf("ランダム数： %d, %d, %d, %d\n", rand_value_1, rand_value_2, rand_value_3, rand_value_4);
            
            // デューティ値をランダムに変更
            pwm_set_chan_level(motor1_slice_num, PWM_CHAN_A, rand_value_1);
            pwm_set_chan_level(motor2_slice_num, PWM_CHAN_B, rand_value_2);
            pwm_set_chan_level(motor1_slice_num, PWM_CHAN_B, rand_value_3);
            pwm_set_chan_level(motor2_slice_num, PWM_CHAN_A, rand_value_4);
            
            timer_flag_1s = false;
        }
    }
}
