#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

// 信号ピン
#define SERVO2_PIN 11                   // PITCH;PWM:5B
#define SERVO3_PIN 12                   // YAW;PWM:6A
#define DIR_PIN 14                      // ステッピングモータ:DIR
#define STEP_PIN 15                     // ステッピングモータ:STEP

// PWM周波数50Hz(20ms)
float div_value = 125;                  // クロックの分周
uint16_t wrap_value = 20000;            // 一周期のカウント数

uint16_t servo2_duty = 1500;            // PITCH 初期値0°のDuty値
uint16_t servo3_duty = 1500;            // YAW 初期値0°のDuty値
uint16_t servo2_volume;                 // PITCH ボリューム位置
uint16_t servo3_volume;                 // YAW ボリューム位置
uint16_t pitch_conv_data;               // 範囲変換後サーボ移動量
uint16_t yaw_conv_data;                 // 範囲変換後サーボ移動量
uint16_t roll_conv_data;                // 範囲変換後サーボ移動量
uint16_t roll_volume;                   // ROLL ボリューム位置 
uint16_t before_roll_volume = 800;     // 前回のROLLボリューム位置
uint16_t move_value;                    // ステッピングモータ移動量
bool stepper_zero = false;


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
GPIOコールバック関数
*******************/
void gpio_callback(uint gpio, uint32_t events) {
    // Put the GPIO event(s) that just happened into event_str
    // so we can print it
    printf("マグネット検出、原点へ復帰 %d\n", gpio);
    stepper_zero = true;
    // コールバックの割り込みは1回のみで終了
    gpio_set_irq_enabled(2, GPIO_IRQ_EDGE_FALL, false);
}


/*******************
メイン関数
*******************/
int main() {
    stdio_init_all();
    sleep_ms(1000);
    
    printf("Hello Hall Sensor\n");

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
    sleep_ms(100);

    adc_init();
    adc_gpio_init(26);      // ROLL
    adc_gpio_init(27);      // PITCH
    adc_gpio_init(28);      // YAW

    // ステッピングモータ用GPIO設定
    gpio_init(DIR_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_init(STEP_PIN);
    gpio_set_dir(STEP_PIN, GPIO_OUT);

    gpio_put(DIR_PIN, 0);
    gpio_put(STEP_PIN, 0);

    sleep_ms(1000);

    // ステッピングモータの原点復帰
    gpio_set_irq_enabled_with_callback(2, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_put(DIR_PIN, 0);
    for (int i = 0; i < 800; i++) {
        if (stepper_zero) continue;
        gpio_put(STEP_PIN, 1);
        sleep_us(5000);
        gpio_put(STEP_PIN, 0);
        sleep_us(5000);
    }
    gpio_put(DIR_PIN, 1);
    for (int i = 0; i < 222; i++) {
        gpio_put(STEP_PIN, 1);
        sleep_us(2000);
        gpio_put(STEP_PIN, 0);
        sleep_us(2000);
    }
    sleep_ms(2000);


    while (true) {
        adc_select_input(0);
        roll_volume = adc_read();
        roll_conv_data = Map(roll_volume, 0, 4095, 0, 1600);
        // printf("volume : %d, conv : %d\n", roll_volume, roll_conv_data);
        
        if (roll_conv_data > before_roll_volume + 15 || roll_conv_data < before_roll_volume - 15) {
            if (roll_conv_data > before_roll_volume) {
                gpio_put(DIR_PIN, 0);
                move_value = roll_conv_data - before_roll_volume;
            }else {
                gpio_put(DIR_PIN, 1);
                move_value = before_roll_volume - roll_conv_data;
            }

            for (int i = 0; i < move_value; i++) {
                gpio_put(STEP_PIN, 1);
                sleep_us(1800);
                gpio_put(STEP_PIN, 0);
                sleep_us(1800);
            }
            before_roll_volume = roll_conv_data;
            
        }

        // PITCHボリューム値検出とサーボモータ駆動
        adc_select_input(1);
        servo2_volume = adc_read();
        pitch_conv_data = Map(servo2_volume, 0, 4095, 1000, 2000);
        // printf("volume : %d, conv : %d\n", servo2_volume, pitch_conv_data);

        if (pitch_conv_data > servo2_duty + 20 || pitch_conv_data < servo2_duty - 20) {
            servo_move(servo2_slice_num, PWM_CHAN_B, servo2_duty, pitch_conv_data);
            // printf("PITCH: Volume = %d, Duty = %d\n", conv_data, servo2_duty);
            servo2_duty = pitch_conv_data;
        }

        // YAWボリューム値検出とサーボモータ駆動
        adc_select_input(2);
        servo3_volume = adc_read();
        yaw_conv_data = Map(servo3_volume, 0, 4095, 1000, 2000);

        if (yaw_conv_data > servo3_duty + 20 || yaw_conv_data < servo3_duty - 20) {
            servo_move(servo3_slice_num, PWM_CHAN_A, servo3_duty, yaw_conv_data);
            // printf("YAW: Volume = %d, Duty = %d\n", conv_data, servo3_duty);
            servo3_duty = yaw_conv_data;
        }
        sleep_ms(20);
    }
}
