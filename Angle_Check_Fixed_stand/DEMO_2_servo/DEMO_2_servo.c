#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

// 信号ピン
#define SERVO2_PIN 11           // PITCH;PWM:5B
#define SERVO3_PIN 12           // YAW;PWM:6A

// PWM周波数50Hz(20ms)
float div_value = 125;          // クロックの分周
uint16_t wrap_value = 20000;    // 一周期のカウント数

uint16_t servo2_duty = 1500;    // PITCH 初期値0°のDuty値
uint16_t servo3_duty = 1500;    // YAW 初期値0°のDuty値
uint16_t servo2_volume;         // PITCH ボリューム位置
uint16_t servo3_volume;         // YAW ボリューム位置
uint16_t conv_data;

/*******************
サーボモータの移動関数
*******************/
void servo_move(uint slice, uint chan, int pulse_start, int pulse_stop) {
    if (pulse_start < pulse_stop) {
        for (int i = pulse_start; i <= pulse_stop; i++) {
            pwm_set_chan_level(slice, chan, i);
            sleep_ms(2);
        }
    }else {
        for (int i = pulse_start; i >= pulse_stop; i--) {
            pwm_set_chan_level(slice, chan, i);
            sleep_ms(2);
        }
    }
}

/*******************
数値範囲の変換用関数
*******************/
float Map(float value, float bstart, float bstop, float astart, float astop)
{
    return astart + (astop - astart) * ((value - bstart) / (bstop - bstart));
}


/*******************
メイン関数
*******************/
int main() {
    stdio_init_all();

    adc_init();
    adc_gpio_init(27);      // PITCH
    adc_gpio_init(28);      // YAW

    // PWM設定
    gpio_set_function(SERVO2_PIN, GPIO_FUNC_PWM);
    gpio_set_function(SERVO3_PIN, GPIO_FUNC_PWM);

    uint servo2_slice_num = pwm_gpio_to_slice_num(SERVO2_PIN);
    uint servo3_slice_num = pwm_gpio_to_slice_num(SERVO3_PIN);

    pwm_config servo2_config = pwm_get_default_config();
    pwm_config servo3_config = pwm_get_default_config();

    pwm_config_set_clkdiv(&servo2_config, div_value);
    pwm_config_set_clkdiv(&servo3_config, div_value);
    
    pwm_set_wrap(servo2_slice_num, wrap_value);
    pwm_set_wrap(servo3_slice_num, wrap_value);

    pwm_init(servo2_slice_num, &servo2_config, false);
    pwm_init(servo3_slice_num, &servo3_config, false);

    // サーボホーン初期位置へ移動
    pwm_set_chan_level(servo2_slice_num, PWM_CHAN_B, servo2_duty);
    pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, servo3_duty);

    // PWM起動
    pwm_set_enabled(servo2_slice_num, true);
    pwm_set_enabled(servo3_slice_num, true);
    sleep_ms(500);

    while (true) {
        // PITCHボリューム値検出とサーボモータ駆動
        adc_select_input(1);
        servo2_volume = adc_read();
        conv_data = Map(servo2_volume, 0, 4095, 1000, 2000);

        if (conv_data > servo2_duty + 12 || conv_data < servo2_duty - 12) {
            servo_move(servo2_slice_num, PWM_CHAN_B, servo2_duty, conv_data);
            printf("PITCH: Volume = %d, Duty = %d\n", conv_data, servo2_duty);
            servo2_duty = conv_data;
            
        }

        // YAWボリューム値検出とサーボモータ駆動
        adc_select_input(2);
        servo3_volume = adc_read();
        conv_data = Map(servo3_volume, 0, 4095, 1000, 2000);

        if (conv_data > servo3_duty + 12 || conv_data < servo3_duty - 12) {
            servo_move(servo3_slice_num, PWM_CHAN_A, servo3_duty, conv_data);
            printf("YAW: Volume = %d, Duty = %d\n", conv_data, servo3_duty);
            servo3_duty = conv_data;
        }
        sleep_ms(50);
    }
}
