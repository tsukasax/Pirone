#include "motor.h"

// arduinoのPWM周波数490Hzに合わせて約490Hzで設定
float div_value = 125;      // クロックの分周
uint16_t wrap_value = 2040; // 一周期のカウント数

uint16_t motor1_duty = 0;
uint16_t motor2_duty = 0;
uint16_t motor3_duty = 0;
uint16_t motor4_duty = 0;

/*******************
 モーター(PWM)初期化
*******************/
void Motor_Init() {
    gpio_set_function(MOTOR2_PIN, GPIO_FUNC_PWM);
    gpio_set_function(MOTOR4_PIN, GPIO_FUNC_PWM);
    gpio_set_function(MOTOR3_PIN, GPIO_FUNC_PWM);
    gpio_set_function(MOTOR1_PIN, GPIO_FUNC_PWM);

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
    uint pwm_mask_set = (0x01 << motor1_slice_num) | (0x01 << motor2_slice_num);
    // printf("mask: %d\n\n", pwm_mask_set);
    pwm_set_mask_enabled(pwm_mask_set);
}


/*******************
 モーター出力
*******************/
void Motor_Output() {
    if (rc_connect_flag) {
        pulse_length[MOTOR1] = MinMax(pulse_length[MOTOR1], 1000, 2000);
        pulse_length[MOTOR2] = MinMax(pulse_length[MOTOR2], 1000, 2000);
        pulse_length[MOTOR3] = MinMax(pulse_length[MOTOR3], 1000, 2000);
        pulse_length[MOTOR4] = MinMax(pulse_length[MOTOR4], 1000, 2000);
    }else {
        for (int i = 0; i < 4; i++) {pulse_length[i] = 0;}
    }
}
