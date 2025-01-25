#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "asm.pio.h"
#include "mpu6050.h"
#include "receiver.h"
#include "motor.h"

/*** 全般 ***/
#define I2C_SDA_PIN 12
#define I2C_SCL_PIN 13
extern float angle_gyro[3];
extern float angle_accel[3];
extern float complement_angle[4];
extern int mpu_accel_sum[3];
extern int mpu_gyro_sum[3];
extern float target_value[4];

/*** PIO関連 ***/
PIO pio_0 = pio0;
PIO pio_1 = pio1; 
uint sm_0 = 0;
uint sm_1 = 1;
uint sm_2 = 2;
uint sm_3 = 3;

/*** Receiver関連 ***/
#define RCV_CH_1 6
#define RCV_CH_2 7
#define RCV_CH_3 8
#define RCV_CH_4 9
bool rc_connect_flag = false;

/*** タイマー関連 ***/
bool tim5ms = false;           // 5m秒フラグ
bool tim30ms = false;          // 30m秒フラグ
bool tim50ms = false;          // 50m秒フラグ
bool tim100ms = false;         // 100m秒フラグ
bool tim500ms = false;         // 500m秒フラグ
bool tim1s = false;            // 1秒フラグ
bool tim2s = false;            // 2秒フラグ
uint32_t timer_loop = 0;
int t_time;


/*** MPU6050関連 ***/
bool calibrate_flag = false;
extern int mpu_accel[3];
extern int mpu_gyro[3];
extern int16_t acceleration[3], gyro[3];
extern float error_sum[4];
extern float errors[4];
extern float lowpass_angle[4];

/*** PID演算関連 ***/
extern float pulse_length[4];

/*** モーター出力関連 ***/
extern uint16_t motor1_duty;
extern uint16_t motor2_duty;
extern uint16_t motor3_duty;
extern uint16_t motor4_duty;


/*******************
タイマー割り込み処理
*******************/
bool timer_callback(struct repeating_timer *t) {
    // int t_start = time_us_32();
    timer_loop += 1;

    // MPU6050からデータを取得し加算(1ms毎に取得し加算)
    mpu6050_read_raw(acceleration, gyro);
    mpu_accel_sum[X] += acceleration[X];
    mpu_accel_sum[Y] += acceleration[Y];
    mpu_accel_sum[Z] += acceleration[Z];
    mpu_gyro_sum[X] += gyro[X];
    mpu_gyro_sum[Y] += gyro[Y];
    mpu_gyro_sum[Z] += gyro[Z];

    if (timer_loop % 5 == 0) {
        tim5ms = true;         // 200Hz,メインループ

        // 5回平均
        mpu_accel[X] = mpu_accel_sum[X] / MPU6050_SAMPLE;
        mpu_accel[Y] = mpu_accel_sum[Y] / MPU6050_SAMPLE;
        mpu_accel[Z] = mpu_accel_sum[Z] / MPU6050_SAMPLE;
        mpu_gyro[X] = mpu_gyro_sum[X] / MPU6050_SAMPLE;
        mpu_gyro[Y] = mpu_gyro_sum[Y] / MPU6050_SAMPLE;
        mpu_gyro[Z] = mpu_gyro_sum[Z] / MPU6050_SAMPLE;

        for (int i = 0; i < 3; i++) {
        mpu_accel_sum[i] = 0;
        mpu_gyro_sum[i] = 0;
        }
    }

    if (timer_loop % 30 == 0) tim30ms = true;       // 33Hz,シリアル出力
    if (timer_loop % 50 == 0) tim50ms = true;       // 20Hz,
    if (timer_loop % 100 == 0) tim100ms = true;     // 10Hz,
    if (timer_loop % 500 == 0) tim500ms = true;     // 2Hz,
    if (timer_loop % 1000 == 0) tim1s = true;       // 1Hz,
    if (timer_loop % 2000 == 0) tim2s = true;       // 0.5Hz,

    // int t_stop = time_us_32();
    // t_time = t_stop - t_start;
    return true;
}



/*******************
メイン関数
*******************/
int main() {
    stdio_init_all();
    sleep_ms(500);
    // printf("\n\nこんにちは、Pironeです。!!\n");

    // Pico内蔵LEDの点灯・点滅用 PIO設定
    uint offset_1 = pio_add_program(pio_1, &blink_program);
    blink_program_init(pio_1, sm_0, offset_1, PICO_DEFAULT_LED_PIN);
    pio_1->txf[sm_0] = (clock_get_hz(clk_sys) / (2 * freq_1s) - 3);
    // PIO1のSM0をスタート
    pio_sm_set_enabled(pio_1, sm_0, true);
    // printf("LED点滅スタートしました。\n\n");
    
    // プロポからの受信信号用 PIO設定
    uint offset_0 = pio_add_program(pio_0, &receiver_program);    
    receiver_program_init(pio_0, sm_0, offset_0, RCV_CH_1);
    receiver_program_init(pio_0, sm_1, offset_0, RCV_CH_2);
    receiver_program_init(pio_0, sm_2, offset_0, RCV_CH_3);
    receiver_program_init(pio_0, sm_3, offset_0, RCV_CH_4);

    // PIO0 IRQハンドラの設定
    irq_set_exclusive_handler(PIO0_IRQ_0, irq_handler);
    irq_set_enabled(PIO0_IRQ_0, true);
    pio0_hw->inte0 = PIO_IRQ0_INTE_SM0_BITS;

    // PIO0の全smを同時スタート、PIO1のSM0をスタート
    pio_enable_sm_mask_in_sync(pio_0, 15);        // テスト時はコメントアウト
    pio_sm_set_enabled(pio_1, sm_0, true);
    
    sleep_ms(1000);

    // I2C初期化
    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // MPU6050初期化
    mpu6050_init();

    // printf("キャリブレーション指令待機\n");
    // calibrate_flag = true;      // テスト用
    // rc_connect_flag = true;     // テスト用
    while (calibrate_flag == false) {sleep_ms(1);}

    // LED高速点滅
    pio_1->txf[sm_0] = (clock_get_hz(clk_sys) / (2 * freq_01s) - 3);
    
    // MPU6050キャリブレーション
    mpu6050_calibrate();

    // LED低速点滅
    pio_1->txf[sm_0] = (clock_get_hz(clk_sys) / (2 * freq_1s) - 3);

    // タイマー割り込みの設定、1ms毎(1kHz)
    printf("タイマー割り込み 開始\nプロポ接続待ち\n");
    struct repeating_timer timer;
    add_repeating_timer_ms(-1, timer_callback, NULL, &timer);
    
    // モータ(PWM)の初期化
    Motor_Init();

    while (true) {
        // 5m秒毎に実行(200Hz)
        if (tim5ms) {
            // 姿勢角度の計算
            Real_Angles();

            // 目標値の設定
            Target_Set();

            // 誤差算出
            Error_Process();

            // PID演算
            PID_Calculation();

            // モーター出力
            Motor_Output();

            tim5ms = false;
        }

        // 30m秒毎に実行(33Hz)
        if (tim30ms) {
            // printf("%d,%d,%d,%d\n", receiver_pulse[0], receiver_pulse[1], receiver_pulse[2], receiver_pulse[3]);
            // printf("%d, %d, %d, %d, %d, %d\n", mpu_accel[X], mpu_accel[Y], mpu_accel[Z], mpu_gyro[X], mpu_gyro[Y], mpu_gyro[Z]);
            // printf("%.2f, %.2f\n", angle_accel[X], angle_accel[Y]);
            // printf("%.2f, %.2f, %.2f, %.2f\n", angle_gyro[X], angle_gyro[Y], angle_accel[X], angle_accel[Y]);
            // printf("acc_total_vector : %.5f\n", acc_total_vector);
            // printf("%.2f, %.2f, %.2f\n", target_value[ROLL], target_value[PITCH], target_value[YAW]);
            // printf("%.2f, %.2f, %.2f\n", error_sum[ROLL], error_sum[PITCH], error_sum[YAW]);
            // printf("%.2f, %.2f, %.2f\n", pid[ROLL], pid[PITCH], pid[YAW]);
            // printf("%.2f, %.2f, %.2f,%.2f, %.2f, %.2f\n", lowpass_angle[ROLL], lowpass_angle[PITCH], lowpass_angle[YAW],target_value[ROLL], target_value[PITCH], target_value[YAW]);
            // printf("%d, %d, %d,%.2f, %.2f, %.2f\n", receiver_pulse[ROLL], receiver_pulse[PITCH], receiver_pulse[YAW],target_value[ROLL], target_value[PITCH], target_value[YAW]);
            // printf("%.2f, %.2f, %.2f,%.2f, %.2f, %.2f\n", errors[ROLL], errors[PITCH], errors[YAW], error_sum[ROLL], error_sum[PITCH], error_sum[YAW]);
            // printf("%.2f, %.2f, %.2f\n", complement_angle[ROLL], complement_angle[PITCH], complement_angle[YAW]);
            printf("%.2f, %.2f, %.2f, %.2f\n", pulse_length[MOTOR1], pulse_length[MOTOR2], pulse_length[MOTOR3], pulse_length[MOTOR4]);
            // printf("%d, %d, %d, %d\n", motor1_duty, motor2_duty, motor3_duty, motor4_duty);
            // printf("%d\n", t_time);
            tim30ms = false;
        }
        sleep_us(10);
        // 500m秒毎に実行
        // if (tim500ms) {
            // int t_start = time_us_32();
            // int t_stop = time_us_32();
            // printf("Time = %d\n", t_stop - t_start);            
            // tim500ms = false;
        // }
    }
}
