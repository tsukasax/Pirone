#include "receiver.h"

uint32_t receiver_pulse[4] = {1500, 1500, 1000, 1500};

/*******************
プロポから受信発生の割り込み処理
*******************/
void irq_handler(){
    pio0_hw->irq = 1;
    // チャンネル毎のパルス幅を算出
    receiver_pulse[ROLL] = 5000 - pio_sm_get(pio_0, sm_0);
    receiver_pulse[PITCH] = 5000 - pio_sm_get(pio_0, sm_1);
    receiver_pulse[THROTTLE] = 5000 - pio_sm_get(pio_0, sm_2);
    receiver_pulse[YAW] = 5000 - pio_sm_get(pio_0, sm_3);

    // 左下・左下でキャリブレーション開始
    if (receiver_pulse[ROLL] < 1100 && receiver_pulse[PITCH] < 1100 && 
            receiver_pulse[THROTTLE] < 1100 && receiver_pulse[YAW] < 1100) {
        calibrate_flag = true;
    }

    // 逆ハでプロポに接続、ハでプロポから切断
    if (loop_start) {
        if (receiver_pulse[ROLL] < 1100 && receiver_pulse[PITCH] < 1100 &&
                receiver_pulse[THROTTLE] < 1100 && receiver_pulse[YAW] > 1800) {
            if (rc_connect_flag == false) {
                // printf("プロポ接続しました。\n");
                rc_connect_flag = true;
                pio_sm_put_blocking(pio_1, sm_0, 0x00000f00);     // 青色
            }
        }
        if (receiver_pulse[ROLL] > 1800 && receiver_pulse[PITCH] < 1100 &&
                receiver_pulse[THROTTLE] < 1100 && receiver_pulse[YAW] < 1100) {
            if (rc_connect_flag == true) {
                // printf("プロポ切断しました。\n");
                rc_connect_flag = false;
                pio_sm_put_blocking(pio_1, sm_0, 0x00000000);     // 消灯
            }
        }
    }
}
