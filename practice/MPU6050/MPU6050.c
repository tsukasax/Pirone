#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define I2C_SDA_PIN 12
#define I2C_SCL_PIN 13


static int addr = 0x68;


static void mpu6050_init() {
    uint8_t buf[] = {0x6B, 0x80}; // MPU6050をリセット
    i2c_write_blocking(i2c_default, addr, buf, 2, false);
    sleep_ms(100);

    uint8_t buf2[] = {0x6B, 0x00}; // MPU6050のスタート※これが無いと動かない！！
    i2c_write_blocking(i2c_default, addr, buf2, 2, false);
    sleep_ms(100);

    uint8_t buf3[] = {0x19, 0x07}; // サンプリング1kHz 、ジャイロ8kHz
    i2c_write_blocking(i2c_default, addr, buf3, 2, false);
    sleep_ms(100);

    uint8_t buf4[] = {0x1B, 0x00}; // +-250 °/sとして、GYRO_CONFIGの設定
    i2c_write_blocking(i2c_default, addr, buf4, 2, false);
    sleep_ms(100);

    uint8_t buf5[] = {0x1C, 0x00}; // +-2gとして、ACCEL_CONFIGの設定
    i2c_write_blocking(i2c_default, addr, buf5, 2, false);
}

static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3]) {
    uint8_t buffer[14];
    uint8_t val = 0x3B;
    i2c_write_blocking(i2c_default, addr, &val, 1, true);
    i2c_read_blocking(i2c_default, addr, buffer, 14, false);

    accel[0] = buffer[0] << 8 | buffer[1];
    accel[1] = buffer[2] << 8 | buffer[3];
    accel[2] = buffer[4] << 8 | buffer[5];
    gyro[0] = buffer[8] << 8 | buffer[9];
    gyro[1] = buffer[10] << 8 | buffer[11];
    gyro[2] = buffer[12] << 8 | buffer[13];
}

int main() {
    stdio_init_all();
    sleep_ms(1000);
    // printf("Hello, MPU6050! Reading raw data from registers...\n");

    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    
    mpu6050_init();

    int16_t acceleration[3], gyro[3];
    float Ax, Ay, Az, Gx, Gy, Gz;

    while (1) {
        // int t_start = time_us_32();
        mpu6050_read_raw(acceleration, gyro);
        
        Ax = acceleration[0] / 16384.0;
        Ay = acceleration[1] / 16384.0;
        Az = acceleration[2] / 16384.0;
        Gx = gyro[0] / 131.0;
        Gy = gyro[1] / 131.0;
        Gz = gyro[2] / 131.0;

        // printf("%d",acceleration[2]);
        printf("Ax:%.2f, Ay:%.2f, Az:%.2f\n",
            Ax, Ay, Az);
        // printf("%.2f, %.2f, %.2f, %.2f, %.2f, %.2f\n",
        //     Ax, Ay, Az, Gx, Gy, Gz);

        // printf("Time = %d\n", time_us_32() - t_start);

        sleep_ms(100);
    }
}
