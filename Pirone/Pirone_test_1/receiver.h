#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "mpu6050.h"

#ifndef RECEIVER_H
#define RECEIVER_H

#define freq_1s 1                       // 1Hzの点滅
#define freq_01s 10                     // 10Hzの点滅
#define freq_1ms 1000                   // 1000Hzの点滅

extern PIO pio_0;
extern PIO pio_1;
extern uint sm_0;
extern uint sm_1;
extern uint sm_2;
extern uint sm_3;

extern bool rc_connect_flag;
extern bool calibrate_flag;

void irq_handler();

#endif
