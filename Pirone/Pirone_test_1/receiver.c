#include "receiver.h"

uint32_t receiver_pulse[4] = {1500, 1500, 1500, 1500};

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

    if (receiver_pulse[ROLL] < 1100 && receiver_pulse[PITCH] < 1100 && 
            receiver_pulse[THROTTLE] < 1100 && receiver_pulse[YAW] < 1100) {
        calibrate_flag = true;
    }

    if (calibrate_flag) {
        if (receiver_pulse[ROLL] < 1100 && receiver_pulse[PITCH] < 1100 &&
                receiver_pulse[THROTTLE] < 1100 && receiver_pulse[YAW] > 1800) {
            if (rc_connect_flag == false) {
                // printf("プロポ接続しました。\n");
                rc_connect_flag = true;
                pio_1->txf[sm_0] = (clock_get_hz(clk_sys) / (2 * freq_1ms) - 3);
            }
        }
        if (receiver_pulse[ROLL] > 1800 && receiver_pulse[PITCH] < 1100 &&
                receiver_pulse[THROTTLE] < 1100 && receiver_pulse[YAW] < 1100) {
            if (rc_connect_flag == true) {
                // printf("プロポ切断しました。\n");
                rc_connect_flag = false;
                pio_1->txf[sm_0] = (clock_get_hz(clk_sys) / (2 * freq_1s) - 3);
            }
        }
    }
}
