#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "wifi.h"

#include "lwip/apps/httpd.h"
#include "lwip/init.h"

#include "hardware/flash.h"
#include "pico/flash.h"

#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "pico/multicore.h"

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
PIO pio_0 = pio0;       // プロポ受信用
PIO pio_1 = pio1;       // RGB LED点灯用
uint sm_0 = 0;
uint sm_1 = 1;
uint sm_2 = 2;
uint sm_3 = 3;
uint offset;

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
extern float Kp[4];
extern float Ki[4];
extern float Kd[4];

/*** モーター出力関連 ***/
extern uint16_t motor1_duty;
extern uint16_t motor2_duty;
extern uint16_t motor3_duty;
extern uint16_t motor4_duty;

/*** RGB_LED関連 ***/
#define IS_RGBW false
#define WS2812_PIN 28

/*** httpd関連 ***/
void httpd_init(void);
uint8_t task_mode = 0;


/************************
UDP送信
*************************/
#define UDP_PORT 4444               // 送信先ポート
#define BEACON_MSG_LEN_MAX 32
#define BEACON_TARGET "192.168.1.3" // 送信先IPアドレス
#define BEACON_INTERVAL_MS 100      // 100ms間隔で送信

void run_udp_beacon() {
    struct udp_pcb* pcb = udp_new();

    ip_addr_t addr;
    ipaddr_aton(BEACON_TARGET, &addr);

    while (true) {
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, BEACON_MSG_LEN_MAX+1, PBUF_RAM);
        char *req = (char *)p->payload;
        memset(req, 0, BEACON_MSG_LEN_MAX+1);
        // snprintf(req, BEACON_MSG_LEN_MAX, "%.2f,%.2f,%.2f\n", Kp[0], Kp[1], Kp[3]);
        snprintf(req, BEACON_MSG_LEN_MAX, "%d,%d,%d,%d\n", motor1_duty, motor2_duty, motor3_duty, motor4_duty);
        err_t er = udp_sendto(pcb, p, &addr, UDP_PORT);
        pbuf_free(p);
        if (er != ERR_OK) {
            printf("Failed to send UDP packet! error=%d", er);
        } else {
            // printf("Sent packet %.2f,%.2f,%.2f\n", kpid[0], kpid[1], kpid[2]);
        }
        sleep_ms(BEACON_INTERVAL_MS);
    }
}


/**************************
 * フラッシュメモリ操作
 **************************/
// フラッシュメモリへ書き込み
static void write_flash(void) {
    // 最終ブロックを指定
    const uint32_t FLASH_TARGET_OFFSET = 0x1F0000;
    uint8_t write_data[FLASH_PAGE_SIZE];
    size_t i;
    size_t j = 0;
    float data_float;

    // 書き込みデータのセット（少数を整数にして、2バイトで格納）
    for (i = 0; i < 4; i++) {
        data_float = Kp[i] * 100;
        write_data[j++] = (int)data_float % 0x100;
        write_data[j++] = (int)data_float / 0x100;
    }
    for (i = 0; i < 4; i++) {
        data_float = Ki[i] * 100;
        write_data[j++] = (int)data_float % 0x100;
        write_data[j++] = (int)data_float / 0x100;
    }
    for (i = 0; i < 4; i++) {
        data_float = Kd[i] * 100;
        write_data[j++] = (int)data_float % 0x100;
        write_data[j++] = (int)data_float / 0x100;
    }

    // フラッシュ消去し書き込み
    uint32_t ints = save_and_disable_interrupts();
    multicore_lockout_start_blocking();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, write_data, FLASH_PAGE_SIZE);
    multicore_lockout_end_blocking();
    restore_interrupts(ints);
    printf("フラッシュメモリを消去しました。\n");
}

// フラッシュメモリから読み込み
uint16_t read_data[12];

void read_flash(void) {
    // 最終ブロックを指定
    const uint32_t FLASH_TARGET_OFFSET = 0x1F0000;
    const uint8_t *flash_data = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

    read_data[0] = flash_data[1] << 8 | flash_data[0];
    read_data[1] = flash_data[3] << 8 | flash_data[2];
    read_data[2] = flash_data[5] << 8 | flash_data[4];
    read_data[3] = flash_data[7] << 8 | flash_data[6];
    read_data[4] = flash_data[9] << 8 | flash_data[8];
    read_data[5] = flash_data[11] << 8 | flash_data[10];
    read_data[6] = flash_data[13] << 8 | flash_data[12];
    read_data[7] = flash_data[15] << 8 | flash_data[14];
    read_data[8] = flash_data[17] << 8 | flash_data[16];
    read_data[9] = flash_data[19] << 8 | flash_data[18];
    read_data[10] = flash_data[21] << 8 | flash_data[20];
    read_data[11] = flash_data[23] << 8 | flash_data[22];
}


/**************************
 * 画面更新
 **************************/
static const char *ssi_tags[] = {"kp_roll", "kp_pitch", "kp_throt", "kp_yaw",
                                "ki_roll", "ki_pitch", "ki_throt", "ki_yaw",
                                "kd_roll", "kd_pitch", "kd_throt", "kd_yaw", "message"};

u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen) {
    size_t printed;
    switch (iIndex) {
        case 0: {   // "kp_roll"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Kp[0]);
            break;
        }
        case 1: {   // "kp_pitch"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Kp[1]);
            break;
        }
        case 2: {   // "kp_throt"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Kp[2]);
            break;
        }
        case 3: {   // "kp_yaw"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Kp[3]);
            break;
        }
        case 4: {   // "ki_roll"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Ki[0]);
            break;
        }
        case 5: {   // "ki_pitch"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Ki[1]);
            break;
        }
        case 6: {   // "ki_throt"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Ki[2]);
            break;
        }
        case 7: {   // "ki_yaw"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Ki[3]);
            break;
        }
        case 8: {   // "kd_roll"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Kd[0]);
            break;
        }
        case 9: {   // "kd_pitch"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Kd[1]);
            break;
        }
        case 10: {   // "kd_throt"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Kd[2]);
            break;
        }
        case 11: {   // "kd_yaw"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", Kd[3]);
            break;
        }
        case 12: {   // "message"
            if (task_mode == 1) {
                printed = snprintf(pcInsert, iInsertLen, "pid係数を変更しました！");
            }else if (task_mode == 2) {
                printed = snprintf(pcInsert, iInsertLen, "フラッシュメモリから読み込みました！");
            }else if (task_mode == 3) {
                printed = snprintf(pcInsert, iInsertLen, "フラッシュメモリへ書き込みました！");
            }else {
                printed = snprintf(pcInsert, iInsertLen, "kpは、%.2f です。", Kp[0]);
            }
            break;
        }
        default: {  // 不明
            printed = 0;
            break;
        }
    }
    task_mode = 0;
    return (u16_t)printed;
}


/**************************
 * POST（ボタンクリック）送信
 **************************/
static void *current_connection;

err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
        u16_t http_request_len, int content_len, char *response_uri,
        u16_t response_uri_len, u8_t *post_auto_wnd) {
    if (memcmp(uri, "/pid", 4) == 0 && current_connection != connection) {
        current_connection = connection;
        *post_auto_wnd = 1;
        task_mode = 1;
        return ERR_OK;
    }else if (memcmp(uri, "/read", 5) == 0 && current_connection != connection) {
        current_connection = connection;
        *post_auto_wnd = 1;
        read_flash();
        task_mode = 2;
        return ERR_OK;
    }else if (memcmp(uri, "/write", 6) == 0 && current_connection != connection) {
        current_connection = connection;
        *post_auto_wnd = 1;
        write_flash();
        task_mode = 3;
        return ERR_OK;
    }
    return ERR_VAL;
}

char *httpd_param_value(struct pbuf *p, const char *param_name, char *value_buf,
    size_t value_buf_len) {
    size_t param_len = strlen(param_name);
    u16_t param_pos = pbuf_memfind(p, param_name, param_len, 0);
    if (param_pos != 0xFFFF) {
        u16_t param_value_pos = param_pos + param_len;
        u16_t param_value_len = 0;
        u16_t tmp = pbuf_memfind(p, "&", 1, param_value_pos);
        if (tmp != 0xFFFF) {
            param_value_len = tmp - param_value_pos;
        }else {
            param_value_len = p->tot_len - param_value_pos;
        }
        if (param_value_len > 0 && param_value_len < value_buf_len) {
            char *result = (char *)pbuf_get_contiguous(p, value_buf, value_buf_len,
            param_value_len, param_value_pos);
            if (result) {
                result[param_value_len] = 0;
                return result;
            }
        }
    }
    return NULL;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
    err_t ret = ERR_VAL;
    size_t i;
    char str[30];
    char buf[30];
    char *val;

    LWIP_ASSERT("NULL pbuf", p != NULL);

    if (current_connection == connection && task_mode == 1) {
        for (i = 0; i < 4; i++) {
            sprintf(str, "%s=", ssi_tags[i]);
            val = httpd_param_value(p, str, buf, sizeof(buf));
            printf("%s : %.2f --> %s\n", ssi_tags[i], Kp[i], val);
            Kp[i] = atof(val);
        }
        for (i = 0; i < 4; i++) {
            sprintf(str, "%s=", ssi_tags[i+4]);
            val = httpd_param_value(p, str, buf, sizeof(buf));
            printf("%s : %.2f --> %s\n", ssi_tags[i+4], Ki[i], val);
            Ki[i] = atof(val);
        }
        for (i = 0; i < 4; i++) {
            sprintf(str, "%s=", ssi_tags[i+8]);
            val = httpd_param_value(p, str, buf, sizeof(buf));
            printf("%s : %.2f --> %s\n", ssi_tags[i+8], Kd[i], val);
            Kd[i] = atof(val);
        }
        printf("PID係数を変更しました。\n");
        printf("\n");
        ret = ERR_OK;
    }else if (current_connection == connection && task_mode == 2) {
        for (i = 0; i < 4; i++) {
            printf("%s : %.2f --> %.2f\n", ssi_tags[i], Kp[i], (float)read_data[i] / 100);
            Kp[i] = (float)read_data[i] / 100;
        }
        for (i = 0; i < 4; i++) {
            printf("%s : %.2f --> %.2f\n", ssi_tags[i+4], Ki[i], (float)read_data[i+4] / 100);
            Ki[i] = (float)read_data[i+4] / 100;
        }
        for (i = 0; i < 4; i++) {
            printf("%s : %.2f --> %.2f\n", ssi_tags[i+8], Kd[i], (float)read_data[i+8] / 100);
            Kd[i] = (float)read_data[i+8] / 100;
        }
        printf("フラッシュメモリから読み込みました。\n");
        printf("\n");
        ret = ERR_OK;
    }else if (current_connection == connection && task_mode == 3) {
        for (i = 0; i < 4; i++) {
            printf("%s : %.2f\n", ssi_tags[i], Kp[i]);
        }
        for (i = 0; i < 4; i++) {
            printf("%s : %.2f\n", ssi_tags[i+4], Ki[i]);
        }
        for (i = 0; i < 4; i++) {
            printf("%s : %.2f\n", ssi_tags[i+8], Kd[i]);
        }
        printf("フラッシュメモリに書き込みました。\n");
        printf("\n");
        ret = ERR_OK;
    }
    pbuf_free(p);
    return ret;
}

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) {
    snprintf(response_uri, response_uri_len, "/index.shtml");
    current_connection = NULL;
}


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


/**************************
 * Core1 メイン関数(WiFi)
 **************************/
ip4_addr_t ip, mask, gw;

void core1_main(void)
{
    // Initialise the Wi-Fi chip
    int rc = cyw43_arch_init();
    hard_assert(rc == PICO_OK);

    // Enable wifi station
    cyw43_arch_enable_sta_mode();

    // 固定IPアドレスの割り当て
    dhcp_release_and_stop(netif_default);
    IP4_ADDR(&ip, 192, 168, 1, 200);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 1, 1);
    netif_set_addr(netif_default, &ip, &mask, &gw);
    netif_set_up(netif_default);

    printf("Connecting to Wi-Fi...\n");
    rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
    hard_assert(rc == PICO_OK);
    printf("Connected.\n");
    // wifi_on = true;
    // Read the ip address in a human readable way
    uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
    printf("IP address %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

    // httpd初期化
    cyw43_arch_lwip_begin();
    httpd_init();
    http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
    cyw43_arch_lwip_end();
    run_udp_beacon();

    cyw43_arch_deinit();
}


/*******************
 * core0 メイン関数(Drone)
*******************/
bool loop_start = false;

int main() {
    stdio_init_all();
    sleep_ms(1000);
    // printf("\n\nこんにちは、Pironeです。!!\n");

    // Core1の起動（WiFi）
    multicore_launch_core1(core1_main);

    // フラッシュメモリ書き込み時のコア停止用
    multicore_lockout_victim_init();

    // RGB_LEDの点灯用 PIO設定
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio_1, &sm_0, &offset, WS2812_PIN, 1, true);
    hard_assert(success);
    ws2812_program_init(pio_1, sm_0, offset, WS2812_PIN, 800000, IS_RGBW);
    pio_sm_put_blocking(pio_1, sm_0, 0x00000000);     // 消灯

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
    pio_sm_put_blocking(pio_1, sm_0, 0x000f0000);     // 赤色
    while (calibrate_flag == false) {sleep_ms(1);}

    // RGB_LED 緑色点灯
    pio_sm_put_blocking(pio_1, sm_0, 0x0f000000);     // 緑色
    
    // MPU6050キャリブレーション
    mpu6050_calibrate();
    calibrate_flag = false;

    // RGB_LED 消灯
    pio_sm_put_blocking(pio_1, sm_0, 0x00000000);     // 消灯

    // タイマー割り込みの設定、1ms毎(1kHz)
    printf("タイマー割り込み 開始\nプロポ接続待ち\n");
    struct repeating_timer timer;
    add_repeating_timer_ms(-1, timer_callback, NULL, &timer);
    
    // モータ(PWM)の初期化
    Motor_Init();

    loop_start = true;

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

        // 500m秒毎に実行(2Hz)
        if (tim500ms) {
            if (calibrate_flag) {
                cancel_repeating_timer(&timer);     // タイマー一時停止
                rc_connect_flag = false;
                pio_sm_put_blocking(pio_1, sm_0, 0x0f000000);     // 緑色
                mpu6050_calibrate();
                pio_sm_put_blocking(pio_1, sm_0, 0x00000000);     // 消灯
                calibrate_flag = false;
                add_repeating_timer_ms(-1, timer_callback, NULL, &timer);      // タイマー再開
            }
            printf("%d,%d,%d,%d\n", receiver_pulse[0], receiver_pulse[1], receiver_pulse[2], receiver_pulse[3]);
            tim500ms = false;
        }
        sleep_us(10);
    }
}
