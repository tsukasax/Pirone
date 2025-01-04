#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// 信号ピン
#define SERVO1_PIN 10      // PWM:5A
#define SERVO2_PIN 11      // PWM:5B
#define SERVO3_PIN 12      // PWM:6A
#define BUTTON_ROLL_LEFT 16
#define BUTTON_ROLL_RIGHT 17
#define BUTTON_PITCH_FRONT 18
#define BUTTON_PITCH_BACK 19
#define BUTTON_YAW_LEFT 20
#define BUTTON_YAW_RIGHT 21
#define BUTTON_ZERO 22

// PWM周波数50Hz(20ms)
float div_value = 125;          // クロックの分周
uint16_t wrap_value = 20000;    // 一周期のカウント数

uint16_t servo1_duty = 1450;    // 初期値0°のDuty値
uint16_t servo2_duty = 1450;    // 初期値0°のDuty値
uint16_t servo3_duty = 1450;    // 初期値0°のDuty値
uint16_t diff_servo1 = 90;     // 位置決め差分
uint16_t diff_servo2 = 0;       // 位置決め差分
uint16_t diff_servo3 = 70;      // 位置決め差分


void servo_speed(uint slice, uint chan, int pulse_start, int pulse_stop) {
    if (pulse_start < pulse_stop) {
        for (int i = pulse_start; i <= pulse_stop; i++) {
            pwm_set_chan_level(slice, chan, i);
            sleep_ms(1);
        }
    }else {
        for (int i = pulse_start; i >= pulse_stop; i--) {
            pwm_set_chan_level(slice, chan, i);
            sleep_ms(1);
        }
    }
}


/*******************
メイン関数
*******************/
int main() {
    stdio_init_all();
    sleep_ms(1000);     // USBの認識待ち
    printf("Hello Servo Motor!\n");

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    gpio_init(BUTTON_ROLL_LEFT);
    gpio_init(BUTTON_ROLL_RIGHT);
    gpio_init(BUTTON_PITCH_FRONT);
    gpio_init(BUTTON_PITCH_BACK);
    gpio_init(BUTTON_YAW_LEFT);
    gpio_init(BUTTON_YAW_RIGHT);
    gpio_init(BUTTON_ZERO);
    gpio_set_dir(BUTTON_ROLL_LEFT, GPIO_IN);
    gpio_set_dir(BUTTON_ROLL_RIGHT, GPIO_IN);
    gpio_set_dir(BUTTON_PITCH_FRONT, GPIO_IN);
    gpio_set_dir(BUTTON_PITCH_BACK, GPIO_IN);
    gpio_set_dir(BUTTON_YAW_LEFT, GPIO_IN);
    gpio_set_dir(BUTTON_YAW_RIGHT, GPIO_IN);
    gpio_set_dir(BUTTON_ZERO, GPIO_IN);
    gpio_pull_up(BUTTON_ROLL_LEFT);
    gpio_pull_up(BUTTON_ROLL_RIGHT);
    gpio_pull_up(BUTTON_PITCH_FRONT);
    gpio_pull_up(BUTTON_PITCH_BACK);
    gpio_pull_up(BUTTON_YAW_LEFT);
    gpio_pull_up(BUTTON_YAW_RIGHT);
    gpio_pull_up(BUTTON_ZERO);

    // PWM設定
    gpio_set_function(SERVO1_PIN, GPIO_FUNC_PWM);
    gpio_set_function(SERVO2_PIN, GPIO_FUNC_PWM);
    gpio_set_function(SERVO3_PIN, GPIO_FUNC_PWM);
    uint servo1_slice_num = pwm_gpio_to_slice_num(SERVO1_PIN);
    uint servo3_slice_num = pwm_gpio_to_slice_num(SERVO3_PIN);
    pwm_config servo1_config = pwm_get_default_config();
    pwm_config servo3_config = pwm_get_default_config();
    pwm_config_set_clkdiv(&servo1_config, div_value);
    pwm_config_set_clkdiv(&servo3_config, div_value);
    pwm_init(servo1_slice_num, &servo1_config, false);
    pwm_init(servo3_slice_num, &servo3_config, false);
    pwm_set_wrap(servo1_slice_num, wrap_value);
    pwm_set_wrap(servo3_slice_num, wrap_value);
    pwm_set_chan_level(servo1_slice_num, PWM_CHAN_A, servo1_duty + diff_servo1);
    pwm_set_chan_level(servo1_slice_num, PWM_CHAN_B, servo2_duty + diff_servo2);
    pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, servo3_duty + diff_servo3);
    // PWM起動
    pwm_set_enabled(servo1_slice_num, true);
    pwm_set_enabled(servo3_slice_num, true);
    sleep_ms(500);

    while (true) {
        if (!gpio_get(BUTTON_ROLL_LEFT)) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            pwm_set_chan_level(servo1_slice_num, PWM_CHAN_A, servo1_duty + diff_servo1);
            // servo_speed(servo1_slice_num, PWM_CHAN_A, servo1_duty + diff_servo1, 1260 + diff_servo1);
            // servo1_duty = 1260;
            sleep_ms(500);
            pwm_set_chan_level(servo1_slice_num, PWM_CHAN_A, 1260 + diff_servo1);
            sleep_ms(500);
            gpio_put(PICO_DEFAULT_LED_PIN, false);
        }
        if (!gpio_get(BUTTON_ROLL_RIGHT)) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            pwm_set_chan_level(servo1_slice_num, PWM_CHAN_A, servo1_duty + diff_servo1);
            // servo_speed(servo1_slice_num, PWM_CHAN_A, servo1_duty + diff_servo1, 1640 + diff_servo1);
            // servo1_duty = 1640;
            sleep_ms(500);
            pwm_set_chan_level(servo1_slice_num, PWM_CHAN_A, 1640 + diff_servo1);
            sleep_ms(500);
            gpio_put(PICO_DEFAULT_LED_PIN, false);
        }
        if (!gpio_get(BUTTON_PITCH_FRONT)) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            pwm_set_chan_level(servo1_slice_num, PWM_CHAN_B, servo2_duty + diff_servo2);
            sleep_ms(500);
            pwm_set_chan_level(servo1_slice_num, PWM_CHAN_B, 1640 + diff_servo2);
            sleep_ms(500);
            gpio_put(PICO_DEFAULT_LED_PIN, false);
        }
        if (!gpio_get(BUTTON_PITCH_BACK)) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            pwm_set_chan_level(servo1_slice_num, PWM_CHAN_B, servo2_duty + diff_servo2);
            sleep_ms(500);
            pwm_set_chan_level(servo1_slice_num, PWM_CHAN_B, 1260 + diff_servo2);
            sleep_ms(500);
            gpio_put(PICO_DEFAULT_LED_PIN, false);
        }
        if (!gpio_get(BUTTON_YAW_LEFT)) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, servo3_duty + diff_servo3);
            sleep_ms(500);
            pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, 1767 + diff_servo3);
            sleep_ms(500);
            gpio_put(PICO_DEFAULT_LED_PIN, false);
        }
        if (!gpio_get(BUTTON_YAW_RIGHT)) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, servo3_duty + diff_servo3);
            sleep_ms(500);
            pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, 1133 + diff_servo3);
            sleep_ms(500);
            gpio_put(PICO_DEFAULT_LED_PIN, false);
        }
        if (!gpio_get(BUTTON_ZERO)) {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            pwm_set_chan_level(servo1_slice_num, PWM_CHAN_A, servo1_duty + diff_servo1);
            pwm_set_chan_level(servo1_slice_num, PWM_CHAN_B, servo2_duty + diff_servo2);
            pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, servo3_duty + diff_servo3);
            sleep_ms(500);
            gpio_put(PICO_DEFAULT_LED_PIN, false);
        }

        // pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, 1133 + diff_servo3);
        // sleep_ms(2000);
        // pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, servo3_duty + diff_servo3);
        // sleep_ms(2000);
        // pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, 1767 + diff_servo3);
        // sleep_ms(2000);
        // pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, servo3_duty + diff_servo3);
        // sleep_ms(2000);
    }
}
