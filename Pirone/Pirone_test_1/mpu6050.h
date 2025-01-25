#include "hardware/i2c.h"
#include "hardware/clocks.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef MPU6050_H
#define MPU6050_H

#define X 0
#define Y 1
#define Z 2
#define ROLL     0
#define PITCH    1
#define THROTTLE 2
#define YAW      3
#define MOTOR1   0
#define MOTOR2   1
#define MOTOR3   2
#define MOTOR4   3
#define CALIBRATION_SAMPLE 1000         // キャリブレーションのサンプル回数
#define MPU6050_SAMPLE 5                // MPU6050からのサンプリング数（5回平均）
#define MPU6050_FREQ 200                // MPU6050からのサンプリング周波数（5ms）
#define GYRO_LSB 65.5                   // ±500°/s時
#define ACCEL_LSB 4096                  // ±8g時
#define PI 3.14

#define DIFF_ANGLE_MIN -20              // 目標角度 最小
#define DIFF_ANGLE_MAX 20               // 目標角度 最大
#define DIFF_ANGLE_COEFFICIENT 15       // 目標値 係数
#define PID_TOLERANCE 200               // PID制御範囲の許容
#define ERROR_TOLERANCE 400               // 誤差範囲の許容

extern uint32_t receiver_pulse[4];
extern bool rc_connect_flag;


// MPU6050初期化、データ読み込み、キャリブレーション
void mpu6050_init();
void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3]);
void mpu6050_calibrate();

// 現在の姿勢角度の算出
void Real_Angles();

// 目標値の設定
float MinMax(float value, float min_value, float max_value);
float Diff_Value(float angle, float pulse);
void Target_Set();

// 誤差の算出
void Error_Process();

// PID処理
void PID_Calculation();

#endif
