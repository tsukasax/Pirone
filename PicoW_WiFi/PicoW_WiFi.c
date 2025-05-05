#include <stdio.h>
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

void httpd_init(void);

//PID係数の初期化
float kpid[3] = {1.00, 2.00, 3.00};

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
        snprintf(req, BEACON_MSG_LEN_MAX, "%.2f,%.2f,%.2f\n", kpid[0], kpid[1], kpid[2]);
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
    for (i = 0; i < 3; i++) {
        data_float = kpid[i] * 100;
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
uint16_t read_data[3];

void read_flash(void) {
    // 最終ブロックを指定
    const uint32_t FLASH_TARGET_OFFSET = 0x1F0000;
    const uint8_t *flash_data = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

    read_data[0] = flash_data[1] << 8 | flash_data[0];
    read_data[1] = flash_data[3] << 8 | flash_data[2];
    read_data[2] = flash_data[5] << 8 | flash_data[4];
}


/**************************
 * 画面更新
 **************************/
static const char *ssi_tags[] = {"kp", "ki", "kd", "message"};

u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen) {
    size_t printed;
    switch (iIndex) {
        case 0: {   // "kp"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", kpid[0]);
            break;
        }
        case 1: {   // "ki"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", kpid[1]);
            break;
        }
        case 2: {   // "kd"
            printed = snprintf(pcInsert, iInsertLen, "%.2f", kpid[2]);
            break;
        }
        case 3: {   // "message"
            if (task_mode == 1) {
                printed = snprintf(pcInsert, iInsertLen, "pid係数を変更しました！");
            }else if (task_mode == 2) {
                printed = snprintf(pcInsert, iInsertLen, "フラッシュメモリから読み込みました！");
            }else if (task_mode == 3) {
                printed = snprintf(pcInsert, iInsertLen, "フラッシュメモリへ書き込みました！");
            }else {
                printed = snprintf(pcInsert, iInsertLen, "kpは、%.2f です。", kpid[0]);
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
    char str[10];
    char buf[10];
    char *val;

    LWIP_ASSERT("NULL pbuf", p != NULL);

    if (current_connection == connection && task_mode == 1) {
        for (i = 0; i < 3; i++) {
            sprintf(str, "%s=", ssi_tags[i]);
            val = httpd_param_value(p, str, buf, sizeof(buf));
            printf("%s : %.2f --> %s\n", ssi_tags[i], kpid[i], val);
            kpid[i] = atof(val);
        }
        printf("PID係数を変更しました。\n");
        printf("\n");
        ret = ERR_OK;
    }else if (current_connection == connection && task_mode == 2) {
        for (i = 0; i < 3; i++) {
            printf("%s : %.2f --> %.2f\n", ssi_tags[i], kpid[i], (float)read_data[i] / 100);
            kpid[i] = (float)read_data[i] / 100;
        }
        printf("フラッシュメモリから読み込みました。\n");
        printf("\n");
        ret = ERR_OK;
    }else if (current_connection == connection && task_mode == 3) {
        for (i = 0; i < 3; i++) {
            printf("%s : %.2f\n", ssi_tags[i], kpid[i]);
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




/**************************
 * Core1 メイン関数
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

/************************
タイマー割り込み関数（1秒間隔、時間差出力）
*************************/
uint32_t timer_count = 0;

bool repeating_timer_callback(__unused struct repeating_timer *t) {
    float interval = ((float)time_us_32() - (float)timer_count) / 1000000;
    printf("Repeat at %.4f 秒、kp = %.2f : ki = %.2f : kd = %.2f\n", interval, kpid[0], kpid[1], kpid[2]);
    timer_count = time_us_32();
    return true;
}


/**************************
 * core0 メイン関数
 **************************/
int main()
{
    stdio_init_all();

    // Core1の起動（WiFi）
    multicore_launch_core1(core1_main);

    // フラッシュメモリ書き込み時のコア停止用
    multicore_lockout_victim_init();

    struct repeating_timer timer;
    // タイマー割り込み(1ms)
    add_repeating_timer_ms(-1000, repeating_timer_callback, NULL, &timer);
    timer_count = time_us_32();

    while (true) {
        tight_loop_contents();
    }
}
