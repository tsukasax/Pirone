#include "mpu6050.h"

float angle_gyro[3] = {0, 0, 0};
float angle_accel[3] = {0, 0, 0};
float target_value[4] = {0, 0, 0, 0};
float Kp[4] = {1.3, 1.3, 0, 4.0};
float Ki[4] = {0.02, 0.02, 0, 0.02};
float Kd[4] = {18, 18, 0, 0};
float pid[4] = {0, 0, 0, 0};
float throttle = 0;
float errors[4] = {0, 0, 0, 0};
float error_sum[4] = {0, 0, 0, 0};
float delta_err[4] = {0, 0, 0, 0};
float before_error[4] = {0, 0, 0, 0};
float complement_angle[4] = {0, 0, 0, 0};
float lowpass_angle[4] = {0, 0, 0, 0};
long gyro_offset[3] = {0, 0, 0};
long accel_offset[3] = {0, 0, 0};
int mpu_accel[3] = {0, 0, 0};
int mpu_gyro[3] = {0, 0, 0};
int mpu_accel_sum[3] = {0, 0, 0,};
int mpu_gyro_sum[3] = {0, 0, 0};
float pulse_length[4] = {0, 0, 0, 0};
int16_t acceleration[3], gyro[3];

/*******************
MPU6050の初期化
*******************/
void mpu6050_init() {
    // printf("MPU6050初期化\n\n");

    uint8_t addr = 0x68;
    uint8_t buf[] = {0x6B, 0x80}; // MPU6050をリセット
    i2c_write_blocking(i2c_default, addr, buf, 2, false);
    sleep_ms(100);

    uint8_t buf2[] = {0x6B, 0x00}; // MPU6050のスタート※これが無いと動かない！！
    i2c_write_blocking(i2c_default, addr, buf2, 2, false);
    sleep_ms(100);

    uint8_t buf3[] = {0x19, 0x07}; // サンプリング1kHz 、ジャイロ8kHz
    i2c_write_blocking(i2c_default, addr, buf3, 2, false);
    sleep_ms(100);

    uint8_t buf4[] = {0x1B, 0x08}; // +-500 °/sとして、GYRO_CONFIGの設定
    i2c_write_blocking(i2c_default, addr, buf4, 2, false);
    sleep_ms(100);

    uint8_t buf5[] = {0x1C, 0x10}; // +-8gとして、ACCEL_CONFIGの設定
    i2c_write_blocking(i2c_default, addr, buf5, 2, false);

    uint8_t buf6[] = {0x1A, 0x03}; // ローパスフィルタ 43Hz
    i2c_write_blocking(i2c_default, addr, buf6, 2, false);
}

/*******************
MPU6050からのデータ取得
*******************/
void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3]) {
    uint8_t buffer[14];
    uint8_t val = 0x3B;
    uint8_t addr = 0x68;
    i2c_write_blocking(i2c_default, addr, &val, 1, true);
    i2c_read_blocking(i2c_default, addr, buffer, 14, false);

    accel[0] = buffer[0] << 8 | buffer[1];
    accel[1] = buffer[2] << 8 | buffer[3];
    accel[2] = buffer[4] << 8 | buffer[5];
    gyro[0] = buffer[8] << 8 | buffer[9];
    gyro[1] = buffer[10] << 8 | buffer[11];
    gyro[2] = buffer[12] << 8 | buffer[13];
}

/*******************
MPU6050キャリブレーション
    CALIBRATION_SAMPLE数での平均を算出
*******************/
void mpu6050_calibrate() {
    // printf("MPU6050キャリブレーション 開始\n");
    gyro_offset[X] = 0;
    gyro_offset[Y] = 0;
    gyro_offset[Z] = 0;
    // accel_offset[X] = 0;
    // accel_offset[Y] = 0;
    // accel_offset[Z] = 0;

    for (int i = 0; i < CALIBRATION_SAMPLE; i++) {
        mpu6050_read_raw(acceleration, gyro);
        
        gyro_offset[X] += gyro[X];
        gyro_offset[Y] += gyro[Y];
        gyro_offset[Z] += gyro[Z];
        // accel_offset[X] += acceleration[X];
        // accel_offset[Y] += acceleration[Y];
        // accel_offset[Z] += acceleration[Z];

        sleep_ms(3);
    }
    gyro_offset[X] /= CALIBRATION_SAMPLE;
    gyro_offset[Y] /= CALIBRATION_SAMPLE;
    gyro_offset[Z] /= CALIBRATION_SAMPLE;
    // accel_offset[X] /= CALIBRATION_SAMPLE;
    // accel_offset[Y] /= CALIBRATION_SAMPLE;
    // accel_offset[Z] /= CALIBRATION_SAMPLE;
    // accel_offset[Z] -= 4096;

    // printf("MPU6050キャリブレーション 終了\n\n");
    // printf("X: %d, Y: %d, Z: %d\n", gyro_offset[X], gyro_offset[Y], gyro_offset[Z]);
}

/*******************
MPU6050 姿勢角度の取得
    センサからのサンプリング時間：1ms
    センサからのサンプリング回数：5回
    姿勢角度取得のサンプリング周波数：200Hz（5ms）
*******************/
void Real_Angles() {
    // 角速度のキャリブレーションによる補正
    mpu_gyro[X] -= (int)gyro_offset[X];
    mpu_gyro[Y] -= (int)gyro_offset[Y];
    mpu_gyro[Z] -= (int)gyro_offset[Z];
    // 加速度のキャリブレーションによる補正
    // mpu_accel[X] -= (int)accel_offset[X];
    // mpu_accel[Y] -= (int)accel_offset[Y];
    // mpu_accel[Z] -= (int)accel_offset[Z];

    // 角速度での角度計算
    angle_gyro[X] += (-(float)mpu_gyro[X] / (MPU6050_FREQ * GYRO_LSB));
    angle_gyro[Y] += (-(float)mpu_gyro[Y] / (MPU6050_FREQ * GYRO_LSB));

    angle_gyro[Y] += angle_gyro[X] * sin((float)mpu_gyro[Z] * (PI / (MPU6050_FREQ * GYRO_LSB * 180)));
    angle_gyro[X] += angle_gyro[Y] * sin((float)mpu_gyro[Z] * (PI / (MPU6050_FREQ * GYRO_LSB * 180)));

    // 加速度での角度計算
    float acc_total_vector = sqrt(pow((float)mpu_accel[X], 2) + pow((float)mpu_accel[Y], 2) + pow((float)mpu_accel[Z], 2));

    if (abs(mpu_accel[X]) < acc_total_vector) {
        angle_accel[X] = asin((float)mpu_accel[Y] / acc_total_vector) * (180 / PI);
    }

    if (abs(mpu_accel[Y]) < acc_total_vector) {
        angle_accel[Y] = asin((float)mpu_accel[X] / acc_total_vector) * (180 / PI);
    }

    // ジャイロドリフト対策  
    angle_gyro[X] = angle_gyro[X] * 0.9995 + (-angle_accel[X] * 0.0005);
    angle_gyro[Y] = angle_gyro[Y] * 0.9995 + (angle_accel[Y] * 0.0005);

    // 相補フィルタ
    complement_angle[ROLL]  = complement_angle[ROLL]  * 0.9 + angle_gyro[X] * 0.1;
    complement_angle[PITCH] = complement_angle[PITCH] * 0.9 + angle_gyro[Y] * 0.1;
    complement_angle[YAW]   = -(float)mpu_gyro[Z] / (MPU6050_FREQ * GYRO_LSB);

    // ローパスフィルタ(10Hz cutoff frequency)
    lowpass_angle[ROLL]  = 0.7 * lowpass_angle[ROLL]  + 0.3 * (-(float)mpu_gyro[X] / GYRO_LSB);
    lowpass_angle[PITCH] = 0.7 * lowpass_angle[PITCH] + 0.3 * (-(float)mpu_gyro[Y] / GYRO_LSB);
    lowpass_angle[YAW]   = 0.7 * lowpass_angle[YAW]   + 0.3 * ((float)mpu_gyro[Z] / GYRO_LSB);
}

/*******************
 目標値の設定
*******************/
void Target_Set() {
    for (int i = 0; i < 4; i++) {target_value[i] = 0;}

    if (receiver_pulse[THROTTLE] > 1050) {
        target_value[YAW] = Diff_Value(0, receiver_pulse[YAW]);
    }
    
    target_value[PITCH] = Diff_Value(complement_angle[PITCH], receiver_pulse[PITCH]);
    target_value[ROLL] = Diff_Value(complement_angle[ROLL], receiver_pulse[ROLL]);
}

/*******************
 目標値の設定(差分計算)
*******************/
float Diff_Value(float angle, float pulse) {
    float set_point = 0;
    angle = MinMax(angle, DIFF_ANGLE_MIN, DIFF_ANGLE_MAX) * DIFF_ANGLE_COEFFICIENT;
    
    if (pulse > 1508) {
        set_point = pulse - 1508;
    }else if (pulse < 1492) {
        set_point = pulse - 1492;
    }
    set_point -= angle;
    set_point /= 3;

    return set_point;
}

/*******************
 MinMaxの算出
*******************/
float MinMax(float value, float min_value, float max_value) {
    if (value > max_value) {
        value = max_value;
    }else if (value < min_value) {
        value = min_value;
    }
    return value;
}

/*******************
 目標値との差分（エラー）算出
*******************/
void Error_Process() {
    throttle = receiver_pulse[THROTTLE];
    if (rc_connect_flag == false || throttle <= 1300) {
    // if (rc_connect_flag == false) {
        for (size_t i = 0; i < 4; i++) {
            errors[i] = 0;
            error_sum[i] = 0;
            delta_err[i] = 0;
            before_error[i] = 0;
        }
    }else {
        // 測定値と目標値との誤差算出：比例
        errors[ROLL]  = lowpass_angle[ROLL] - target_value[ROLL];
        errors[PITCH] = lowpass_angle[PITCH] - target_value[PITCH];
        errors[YAW]   = lowpass_angle[YAW] - target_value[YAW];

        // 誤差の合計 : 積分
        error_sum[ROLL] += errors[ROLL];
        error_sum[PITCH] += errors[PITCH];
        error_sum[YAW] += errors[YAW];

        // 誤差の合計を許容範囲内に抑える
        error_sum[ROLL] = MinMax(error_sum[ROLL], -ERROR_TOLERANCE/Ki[ROLL], ERROR_TOLERANCE/Ki[ROLL]);
        error_sum[PITCH] = MinMax(error_sum[PITCH], -ERROR_TOLERANCE/Ki[PITCH], ERROR_TOLERANCE/Ki[PITCH]);
        error_sum[YAW] = MinMax(error_sum[YAW], -ERROR_TOLERANCE/Ki[YAW], ERROR_TOLERANCE/Ki[YAW]);

        // 誤差の前回との差分 : 微分
        delta_err[ROLL] = errors[ROLL] - before_error[ROLL];
        delta_err[PITCH] = errors[PITCH] - before_error[PITCH];
        delta_err[YAW] = errors[YAW] - before_error[YAW];

        // 誤差の保存
        before_error[ROLL] = errors[ROLL];
        before_error[PITCH] = errors[PITCH];
        before_error[YAW] = errors[YAW];
    }
}

/*******************
 PID演算
*******************/
void PID_Calculation() {
    throttle = receiver_pulse[THROTTLE];
    // 初期化
    for (size_t i = 0; i < 4; i++) {
        pid[i] = 0;
        pulse_length[i] = throttle;
    }

    if (throttle >= 1050) {
        // PID = e.Kp + ∫e.Ki + Δe.Kd
        pid[ROLL] = (errors[ROLL] * Kp[ROLL]) + (error_sum[ROLL] * Ki[ROLL]) + (delta_err[ROLL] * Kd[ROLL]);
        pid[PITCH] = (errors[PITCH] * Kp[PITCH]) + (error_sum[PITCH] * Ki[PITCH]) + (delta_err[PITCH] * Kd[PITCH]);
        pid[YAW] = (errors[YAW] * Kp[YAW]) + (error_sum[YAW] * Ki[YAW]) + (delta_err[YAW] * Kd[YAW]);

        // PID値の許容範囲
        pid[ROLL]  = MinMax(pid[ROLL], -PID_TOLERANCE, PID_TOLERANCE);        // 右側で+、左側で-
        pid[PITCH] = MinMax(pid[PITCH], -PID_TOLERANCE, PID_TOLERANCE);       // 前傾で-、後傾で+
        pid[YAW]   = MinMax(pid[YAW], -PID_TOLERANCE, PID_TOLERANCE);         // 時計回りで＋、反時計回りで-

        // モータ出力の演算
        pulse_length[MOTOR1] = throttle - pid[ROLL] + pid[PITCH] + pid[YAW];
        pulse_length[MOTOR2] = throttle + pid[ROLL] + pid[PITCH] - pid[YAW];
        pulse_length[MOTOR3] = throttle - pid[ROLL] - pid[PITCH] - pid[YAW];
        pulse_length[MOTOR4] = throttle + pid[ROLL] - pid[PITCH] + pid[YAW];
    }
}
