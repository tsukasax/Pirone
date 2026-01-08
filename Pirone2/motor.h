#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "mpu6050.h"

#ifndef MOTOR_H
#define MOTOR_H

#define MOTOR1_PIN 14      // PWM:7A
#define MOTOR2_PIN 17      // PWM:0B
#define MOTOR3_PIN 15      // PWM:7B
#define MOTOR4_PIN 16      // PWM:0A

extern float pulse_length[4];

void Motor_Init();
void Motor_Output();

#endif
