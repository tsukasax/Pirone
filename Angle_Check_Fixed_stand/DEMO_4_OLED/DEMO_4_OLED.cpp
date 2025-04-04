#include <stdio.h>
#include <cstring>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico-ssd1306/ssd1306.h"
#include "pico-ssd1306/textRenderer/TextRenderer.h"
#include "pico-ssd1306/shapeRenderer/ShapeRenderer.h"
#include "hardware/i2c.h"

using namespace pico_ssd1306;

// 信号ピン
#define SERVO2_PIN 11                   // PITCH;PWM:5B
#define SERVO3_PIN 12                   // YAW;PWM:6A
#define DIR_PIN 14                      // ステッピングモータ:DIR
#define STEP_PIN 15                     // ステッピングモータ:STEP
#define ZERO_PIN 2                      // ホールセンサー（原点検出用）
#define MODE_PIN 6                      // モード切り替え
#define GO_PIN 7                        // 動作実行
#define servo2_duty_ini 1520
#define servo3_duty_ini 1500

#define ZERO_EVENT GPIO_IRQ_EDGE_FALL   // ホールセンサー検出方法

// PWM周波数50Hz(20ms)
float div_value = 125;                  // クロックの分周
uint16_t wrap_value = 20000;            // 一周期のカウント数

uint16_t servo2_duty = servo2_duty_ini; // PITCH 初期値0°のDuty値
uint16_t servo3_duty = servo3_duty_ini; // YAW 初期値0°のDuty値
uint16_t servo2_volume;                 // PITCH ボリューム位置
uint16_t servo3_volume;                 // YAW ボリューム位置
uint16_t pitch_conv_data;               // 範囲変換後サーボ移動量
uint16_t yaw_conv_data;                 // 範囲変換後サーボ移動量
uint16_t roll_conv_data;                // 範囲変換後サーボ移動量
uint16_t roll_volume;                   // ROLL ボリューム位置 
uint16_t before_roll_volume = 800;     // 前回のROLLボリューム位置
uint16_t move_value;                    // ステッピングモータ移動量
bool stepper_zero = false;
uint16_t mode_no = 1;
uint16_t dense_vol;
bool dense_roll_change = false;
bool dense_pitch_change = false;
bool dense_yaw_change = false;

char str_roll[256];
char str_pitch[256];
char str_yaw[256];

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
ステッピングモータ原点復帰関数
*******************/
void stepper_zero_move(uint gpio, uint32_t events) {
    stepper_zero = false;
    gpio_set_irq_enabled(gpio, events, true);

    // ホールセンサ検知
    gpio_put(DIR_PIN, 0);
    for (int i = 0; i < 800; i++) {
        if (stepper_zero) continue;
        gpio_put(STEP_PIN, 1);
        sleep_us(5000);
        gpio_put(STEP_PIN, 0);
        sleep_us(5000);
    }
    // 原点復帰
    gpio_put(DIR_PIN, 1);
    for (int i = 0; i < 213; i++) {
        gpio_put(STEP_PIN, 1);
        sleep_us(2000);
        gpio_put(STEP_PIN, 0);
        sleep_us(2000);
    }
    sleep_ms(1000);
    gpio_set_irq_enabled(gpio, events, false);
}

/*******************
GPIOコールバック関数（ホールセンサー検出）
*******************/
void gpio_callback(uint gpio, uint32_t events) {
    stepper_zero = true;
    // コールバックの割り込みは1回のみで終了
    // gpio_set_irq_enabled(gpio, events, false);
}

/*******************
ステッピングモータ動作関数
*******************/
void stepper_move(uint16_t dir, uint16_t loopx, uint16_t sleepx, uint16_t sleepy) {
    gpio_put(DIR_PIN, dir);
    for (int i = 0; i < loopx; i++) {
        gpio_put(STEP_PIN, 1);
        sleep_us(sleepx);
        gpio_put(STEP_PIN, 0);
        sleep_us(sleepx);
    }
    sleep_ms(sleepy);
}


/*******************
組み合わせ動作-Mode-1
*******************/
void various_move_1() {
    // 原点復帰
    stepper_zero_move(ZERO_PIN, ZERO_EVENT);

    stepper_move(0, 100, 1500, 200);    // ロール右
    sleep_ms(1000);
    stepper_move(1, 100, 1500, 200);    // 原点
    stepper_move(0, 100, 1000, 200);    // ロール右
    sleep_ms(1000);
    stepper_move(1, 100, 1500, 200);    // 原点
    stepper_move(0, 100, 500, 200);    // ロール右
    sleep_ms(1000);
    stepper_move(1, 100, 1500, 200);    // 原点
}

/*******************
組み合わせ動作-Mode-2
*******************/
void various_move_2(uint slice_num) {
    pwm_set_chan_level(slice_num, PWM_CHAN_B, servo2_duty_ini);
    sleep_ms(200);
    uint16_t growth = 3;

    for (int j = 1; j < 4; j++) {
        for (int i = servo2_duty_ini; i <= servo2_duty_ini + 300; i += growth * j) {
            pwm_set_chan_level(slice_num, PWM_CHAN_B, i);
            sleep_ms(10);
        }
        sleep_ms(100);
        for (int i = servo2_duty_ini + 300; i >= servo2_duty_ini - 200; i -= growth * j) {
            pwm_set_chan_level(slice_num, PWM_CHAN_B, i);
            sleep_ms(10);
        }
        sleep_ms(100);
        for (int i = servo2_duty_ini - 200; i <= servo2_duty_ini; i += growth * j) {
            pwm_set_chan_level(slice_num, PWM_CHAN_B, i);
            sleep_ms(10);
        }
        sleep_ms(200);
        pwm_set_chan_level(slice_num, PWM_CHAN_B, servo2_duty_ini);
    }
}

/*******************
組み合わせ動作-Mode-3
*******************/
void various_move_3(uint slice_num) {
    // Stepper、Servo原点復帰
    stepper_zero_move(ZERO_PIN, ZERO_EVENT);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, servo2_duty_ini);

    stepper_move(1, 150, 1000, 200);    // ロール左
    stepper_move(0, 300, 1000, 200);    // ロール右
    stepper_move(1, 150, 1000, 200);    // ロール左（原点復帰）

    for (int i = servo2_duty_ini; i <= servo2_duty_ini + 200; i += 3) {
        pwm_set_chan_level(slice_num, PWM_CHAN_B, i);
        sleep_ms(20);
    }
    sleep_ms(100);
    for (int i = servo2_duty_ini + 200; i >= servo2_duty_ini - 100; i -= 3) {
        pwm_set_chan_level(slice_num, PWM_CHAN_B, i);
        sleep_ms(20);
    }
    sleep_ms(100);
    for (int i = servo2_duty_ini - 100; i <= servo2_duty_ini; i += 3) {
        pwm_set_chan_level(slice_num, PWM_CHAN_B, i);
        sleep_ms(20);
    }
    sleep_ms(500);
    stepper_move(0, 100, 500, 200);    // ロール右
    for (int i = servo2_duty_ini; i <= servo2_duty_ini + 200; i += 4) {
        pwm_set_chan_level(slice_num, PWM_CHAN_B, i);
        sleep_ms(20);
    }
    sleep_ms(200);
    stepper_move(1, 100, 500, 200);    // ロール左
    for (int i = servo2_duty_ini + 200; i >= servo2_duty_ini; i -= 4) {
        pwm_set_chan_level(slice_num, PWM_CHAN_B, i);
        sleep_ms(20);
    }
    pwm_set_chan_level(slice_num, PWM_CHAN_B, servo2_duty_ini);
    sleep_ms(200);

}

/*******************
組み合わせ動作-ボリューム操作
*******************/
void volume_move(uint servo_2, uint servo_3, SSD1306& disp) {
    adc_select_input(0);
    roll_volume = adc_read();
    roll_conv_data = Map(roll_volume, 0, 4095, 0, 1600);
    
    if (roll_conv_data > before_roll_volume + 20 || roll_conv_data < before_roll_volume - 20) {
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
        
        // OLEDへ表示
        fillRect(&disp, 70, 0, 120, 19, WriteMode::SUBTRACT);
        disp.sendBuffer();
        sprintf(str_roll,"%d", roll_conv_data);
        drawText(&disp, font_12x16, str_roll, 70 ,0);
        disp.sendBuffer();
    }

    // PITCHボリューム値検出とサーボモータ駆動
    adc_select_input(1);
    servo2_volume = adc_read();
    pitch_conv_data = Map(servo2_volume, 0, 4095, 1000, 2000);

    if (pitch_conv_data > servo2_duty + 20 || pitch_conv_data < servo2_duty - 20) {
        servo_move(servo_2, PWM_CHAN_B, servo2_duty, pitch_conv_data);
        servo2_duty = pitch_conv_data;

        // OLEDへ表示
        fillRect(&disp, 70, 20, 120, 39, WriteMode::SUBTRACT);
        disp.sendBuffer();
        sprintf(str_pitch,"%d", servo2_duty);
        drawText(&disp, font_12x16, str_pitch, 70 ,20);
        disp.sendBuffer();
    }

    // YAWボリューム値検出とサーボモータ駆動
    adc_select_input(2);
    servo3_volume = adc_read();
    yaw_conv_data = Map(servo3_volume, 0, 4095, 1000, 2000);

    if (yaw_conv_data > servo3_duty + 20 || yaw_conv_data < servo3_duty - 20) {
        servo_move(servo_3, PWM_CHAN_A, servo3_duty, yaw_conv_data);
        servo3_duty = yaw_conv_data;

        // OLEDへ表示
        fillRect(&disp, 70, 40, 120, 59, WriteMode::SUBTRACT);
        disp.sendBuffer();
        sprintf(str_yaw,"%d", servo3_duty);
        drawText(&disp, font_12x16, str_yaw, 70 ,40);
        disp.sendBuffer();
    }
    sleep_ms(20);
}

/*******************
組み合わせ動作-ボリューム操作（微小操作）
*******************/
void volume_move_dense(uint servo_2, uint servo_3, SSD1306& disp) {
    adc_select_input(0);
    roll_volume = adc_read();
    roll_conv_data = Map(roll_volume, 0, 4095, 0, 100);
    
    if (roll_conv_data >= 60 && roll_conv_data <= 70) {
        gpio_put(DIR_PIN, 0);
        move_value = 1;
        dense_vol += 1;
        dense_roll_change = true;
    }else if (roll_conv_data >= 30 && roll_conv_data <= 40) {
        gpio_put(DIR_PIN, 1);
        move_value = 1;
        dense_vol -= 1;
        dense_roll_change = true;
    }
    
    if (dense_roll_change) {
        for (int i = 0; i < move_value; i++) {
            gpio_put(STEP_PIN, 1);
            sleep_us(1800);
            gpio_put(STEP_PIN, 0);
            sleep_us(1800);
        }
        // OLEDへ表示
        fillRect(&disp, 70, 0, 120, 19, WriteMode::SUBTRACT);
        disp.sendBuffer();
        sprintf(str_roll,"%d", dense_vol);
        drawText(&disp, font_12x16, str_roll, 70 ,0);
        disp.sendBuffer();
        dense_roll_change = false;
        sleep_ms(300);
    }
        
    // PITCHボリューム値検出とサーボモータ駆動
    adc_select_input(1);
    servo2_volume = adc_read();
    pitch_conv_data = Map(servo2_volume, 0, 4095, 0, 100);

    if (pitch_conv_data >= 60 && pitch_conv_data <= 70) {
        servo_move(servo_2, PWM_CHAN_B, servo2_duty, servo2_duty + 2);
        servo2_duty += 2;
        dense_pitch_change = true;
    }else if (pitch_conv_data >= 30 && pitch_conv_data <= 40) {
        servo_move(servo_2, PWM_CHAN_B, servo2_duty, servo2_duty - 2);
        servo2_duty -= 2;
        dense_pitch_change = true;
    }

    if (dense_pitch_change) {
        // OLEDへ表示
        fillRect(&disp, 70, 20, 120, 39, WriteMode::SUBTRACT);
        disp.sendBuffer();
        sprintf(str_pitch,"%d", servo2_duty);
        drawText(&disp, font_12x16, str_pitch, 70 ,20);
        disp.sendBuffer();
        dense_pitch_change = false;
        sleep_ms(300);
    }

    // YAWボリューム値検出とサーボモータ駆動
    adc_select_input(2);
    servo3_volume = adc_read();
    yaw_conv_data = Map(servo3_volume, 0, 4095, 1000, 2000);

    if (yaw_conv_data > servo3_duty + 20 || yaw_conv_data < servo3_duty - 20) {
        servo_move(servo_3, PWM_CHAN_A, servo3_duty, yaw_conv_data);
        servo3_duty = yaw_conv_data;

        // OLEDへ表示
        fillRect(&disp, 70, 40, 120, 59, WriteMode::SUBTRACT);
        disp.sendBuffer();
        sprintf(str_yaw,"%d", servo3_duty);
        drawText(&disp, font_12x16, str_yaw, 70 ,40);
        disp.sendBuffer();
    }
    sleep_ms(20);
}


/*******************
メイン関数
*******************/
int main() {
    stdio_init_all();
    sleep_ms(500);

    // GPIO初期化
    gpio_init(6);
    gpio_set_dir(6, GPIO_IN);
    gpio_pull_up(6);

    // I2C初期化
    i2c_init(i2c0, 1000000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);
    sleep_ms(250);

    // OLED初期化
    SSD1306 display = SSD1306(i2c0, 0x3C, Size::W128xH64);
    display.setOrientation(0);
    drawText(&display, font_8x8, "ROLL  :", 0 ,0);
    drawText(&display, font_8x8, "PITCH :", 0 ,20);
    drawText(&display, font_8x8, "YAW   :", 0 ,40);
    drawText(&display, font_12x16, "XXXX", 70 ,0);
    drawText(&display, font_12x16, "XXXX", 70 ,20);
    drawText(&display, font_12x16, "XXXX", 70 ,40);
    display.sendBuffer();

    // PWM初期設定
    gpio_set_function(SERVO2_PIN, GPIO_FUNC_PWM);       // PITCH
    gpio_set_function(SERVO3_PIN, GPIO_FUNC_PWM);       // YAW
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
    pwm_set_chan_level(servo2_slice_num, PWM_CHAN_B, servo2_duty);  // PITCHサーボホーン初期位置へ
    pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, servo3_duty);  // YAWサーボホーン初期位置へ
    pwm_set_enabled(servo2_slice_num, true);    // PWM起動
    pwm_set_enabled(servo3_slice_num, true);    // PWM起動
    sleep_ms(100);

    // ADC初期設定
    adc_init();
    adc_gpio_init(26);      // ROLL
    adc_gpio_init(27);      // PITCH
    adc_gpio_init(28);      // YAW

    // GPIO割り込み
    gpio_set_irq_enabled_with_callback(ZERO_PIN, ZERO_EVENT, true, &gpio_callback);
    // gpio_set_irq_enabled_with_callback(MODE_PIN, ZERO_EVENT, true, &gpio_modechange);

    // ステッピングモータ用GPIO設定
    gpio_init(DIR_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_init(STEP_PIN);
    gpio_set_dir(STEP_PIN, GPIO_OUT);
    gpio_put(DIR_PIN, 0);
    gpio_put(STEP_PIN, 0);
    sleep_ms(1000);

    // ステッピングモータの原点復帰
    stepper_zero_move(ZERO_PIN, ZERO_EVENT);
    
    // 初回OLED表示
    sprintf(str_pitch,"%d", servo2_duty);
    display.clear();
    display.sendBuffer();
    drawText(&display, font_8x8, "ROLL  :", 0 ,0);
    drawText(&display, font_8x8, "PITCH :", 0 ,20);
    drawText(&display, font_8x8, "YAW   :", 0 ,40);
    drawText(&display, font_12x16, "800", 70 ,0);
    drawText(&display, font_12x16, str_pitch, 70 ,20);
    drawText(&display, font_12x16, "1500", 70 ,40);
    display.sendBuffer();

    
    while (true) {
        if (!gpio_get(MODE_PIN)){
            if (mode_no == 5) mode_no = 0;
            mode_no++;

            switch (mode_no) {
                case 1:
                    sprintf(str_roll,"%d", dense_vol);
                    sprintf(str_pitch,"%d", servo2_duty);
                    sprintf(str_yaw,"%d", servo3_duty);
                    display.clear();
                    display.sendBuffer();
                    drawText(&display, font_8x8, "ROLL  :", 0 ,0);
                    drawText(&display, font_8x8, "PITCH :", 0 ,20);
                    drawText(&display, font_8x8, "YAW   :", 0 ,40);
                    drawText(&display, font_12x16, str_roll, 70 ,0);
                    drawText(&display, font_12x16, str_pitch, 70 ,20);
                    drawText(&display, font_12x16, str_yaw, 70 ,40);
                    display.sendBuffer();
                    sleep_ms(100);
                    break;
                case 2:
                    // display.clear();
                    // display.sendBuffer();
                    // drawText(&display, font_12x16, " Return!!", 0 ,5);
                    // display.sendBuffer();
                    // stepper_zero_move(ZERO_PIN, ZERO_EVENT);
                    // pwm_set_chan_level(servo2_slice_num, PWM_CHAN_B, servo2_duty_ini);
                    // pwm_set_chan_level(servo3_slice_num, PWM_CHAN_A, servo3_duty_ini);
                    // dense_vol = 800;
                    // sleep_ms(10);
                    dense_vol = roll_conv_data;
                    display.clear();
                    display.sendBuffer();
                    drawText(&display, font_8x8, "ROLL  :", 0 ,0);
                    drawText(&display, font_8x8, "PITCH :", 0 ,20);
                    drawText(&display, font_8x8, "YAW   :", 0 ,40);
                    drawText(&display, font_8x8, "Dense", 0 ,50);
                    sprintf(str_roll,"%d", dense_vol);
                    sprintf(str_pitch,"%d", servo2_duty);
                    sprintf(str_yaw,"%d", servo3_duty);
                    drawText(&display, font_12x16, str_roll, 70 ,0);
                    drawText(&display, font_12x16, str_pitch, 70 ,20);
                    drawText(&display, font_12x16, str_yaw, 70 ,40);
                    display.sendBuffer();
                    sleep_ms(100);
                    break;
                case 3:
                    display.clear();
                    display.sendBuffer();
                    drawRect(&display, 16, 0, 104, 24);
                    drawText(&display, font_12x16, "  Mode-1", 0 ,5);
                    drawText(&display, font_12x16, "ROLL", 0 ,40);
                    display.sendBuffer();
                    break;
                case 4:
                    display.clear();
                    display.sendBuffer();
                    drawRect(&display, 16, 0, 104, 24);
                    drawText(&display, font_12x16, "  Mode-2", 0 ,5);
                    drawText(&display, font_12x16, "PITCH", 0 ,40);
                    display.sendBuffer();
                    break;
                case 5:
                    display.clear();
                    display.sendBuffer();
                    drawRect(&display, 16, 0, 104, 24);
                    drawText(&display, font_12x16, "  Mode-3", 0 ,5);
                    drawText(&display, font_12x16, "ROLL-PITCH", 0 ,40);
                    display.sendBuffer();
                    break;
                
                default:
                    break;
            }
            sleep_ms(500);
        }

        if (mode_no == 1) {
            volume_move(servo2_slice_num, servo3_slice_num, display);
        }else if (mode_no == 2) {
            volume_move_dense(servo2_slice_num, servo3_slice_num, display);
        }else {
            if (!gpio_get(GO_PIN)) {
                switch (mode_no) {
                    case 3:
                        various_move_1();
                        break;
                    case 4:
                        various_move_2(servo2_slice_num);
                        break;
                    case 5:
                        various_move_3(servo2_slice_num);
                        break;
                    default:
                        break;
                }
            }
            sleep_ms(1);
        }
    }
}
