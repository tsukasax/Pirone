from rp2 import PIO, StateMachine, asm_pio
from machine import Pin, Timer, PWM, I2C, WDT
import time, utime, math

##########
# パルス幅検出用アセンブリプログラム
# HighからLowになる時点までをカウントダウン@asm_pio(autopush=True, push_thresh=16)
###############
@asm_pio(autopush=True, push_thresh=16)
def pulse_width() :
    label('start')
    pull(noblock)			# TXFIFOへ値が無ければXレジスタの値をosrへ代入
    mov(x, osr)				# osrの値をXレジスタに代入（これ以降の基準値として待避）
    mov(y, osr)				# osrの値をYレジスタに代入（カウント用変数として使用）
    wait(0, pin, 0)
    wait(1, pin, 0)
    label('loop')
    jmp(pin, 'countdown')	# 1cycle
    in_(y, 16)				# pinがLowになればYレジスタをisrへ16ビット分代入、即autopush
    irq(rel(0))				# 自身のSM番号の割り込みを発生
    jmp('start')
    label('countdown')
    jmp(y_dec, 'loop')		# 1cycle(全2cycle)


##########
# プロポから受信発生の割り込み
###############
def hdlr0(arg):
    global rc_connection_flag, receiver_pulse, calibrate_flag
    
    # パルス幅検出用カウントダウン値を取得
    # アセンブリプログラムが2命令(1us)なので初期値から減算でパルス幅(us)が求められる
    for index in range(len(ch_list)):
        receiver_pulse[index] = ch_list[index].get()
        receiver_pulse[index] = 5_000 - receiver_pulse[index]
        
    if calibrate_flag == True:
        if receiver_pulse[3] > 1_800 and receiver_pulse[2] < 1_100 and receiver_pulse[0] < 1_100 and receiver_pulse[1] < 1_100 :
            if (rc_connection_flag==False): print("プロポ接続")
            rc_connection_flag = True
            First_Loop = False
            led_pin(1)
            
        if receiver_pulse[0] > 1_800 and receiver_pulse[1] < 1_100 and receiver_pulse[2] < 1_100 and receiver_pulse[3] < 1_100 :
            if (rc_connection_flag==True): print("プロポ切断")
            rc_connection_flag = False
    
    if receiver_pulse[0] < 1_100 and receiver_pulse[1] < 1_100 and receiver_pulse[2] < 1_100 and receiver_pulse[3] < 1_100 :
        calibrate_flag = True


##########
# メインタイマー
#   タイマー周期は「TIMER_FREQ」で設定
#	デフォルト：250(4msec)
###############
def main_count(main_timer):
    global tim1_flag, tim_count, tim200ms, tim500ms
    
    tim_count += 1
    tim1_flag = True
        
    if tim_count % 50 == 0:		# 200msのフラグ
        tim200ms = True
    
    if tim_count % 125 == 0:	# 500msのフラグ
        tim500ms = True


##########
# MPU6050初期化
###############
def MPU6050_Initialize():
    # PWR_MGMT_1=0x80 -> MPU6050をリセット
    i2c.writeto_mem(MPU6050_ADDR, PWR_MGMT_1, b'\x80')
    time.sleep(0.1)
    # PWR_MGMT_1=0 -> MPU6050のスタート
    i2c.writeto_mem(MPU6050_ADDR, PWR_MGMT_1, b'\x00')
    time.sleep(0.1)
    # SMPLRT_DIV=7（サンプリング1kHz 、ジャイロ8kHz)
    i2c.writeto_mem(MPU6050_ADDR, SMPLRT_DIV, b'\x07')
    time.sleep(0.1)
    # FS_SEL=1 -> +-500 °/sとして、GYRO_CONFIGの設定
    i2c.writeto_mem(MPU6050_ADDR, GYRO_CONFIG, b'\x08')
    time.sleep(0.1)
    # AFS_SEL=2 -> +-8gとして、ACCEL_CONFIGの設定
    i2c.writeto_mem(MPU6050_ADDR, ACCEL_CONFIG, b'\x10')
    time.sleep(0.1)
    # ローパスフィルタの設定(Set Digital Low Pass Filter about ~43Hz)
    i2c.writeto_mem(MPU6050_ADDR, FILTER_CONFIG, b'\x03')
    time.sleep(0.1)


##########
# MPU6050から加速度、温度、角速度データの取得関数
###############
def MPU6050_Read(i2c):
    global acc_raw,gyro_raw
    
    # 0x3Bアドレスから14バイト読み込み
    data = i2c.readfrom_mem(MPU6050_ADDR, ACCEL_XOUT_H, 14)
    # 2バイトずつ
    acc_raw[X] = ToSigned(data[0] << 8 | data[1])
    acc_raw[Y] = ToSigned(data[2] << 8 | data[3])
    acc_raw[Z] = ToSigned(data[4] << 8 | data[5])
    temp = ToSigned(data[6] << 8 | data[7])
    gyro_raw[X] = ToSigned(data[8] << 8 | data[9])
    gyro_raw[Y] = ToSigned(data[10] << 8 | data[11])
    gyro_raw[Z] = ToSigned(data[12] << 8 | data[13])


##########
# MPU6050キャリブレーション
#   CALIBRATION_SAMPLE数での平均を算出
###############
def MPU6050_Calibrate():
    global gyro_offset,gyro_raw
    
    for i in range(CALIBRATION_SAMPLE):
        MPU6050_Read(i2c)

        gyro_offset[X] += int(gyro_raw[X])
        gyro_offset[Y] += int(gyro_raw[Y])
        gyro_offset[Z] += int(gyro_raw[Z])

        time.sleep_ms(3)

    # 平均値の算出
    gyro_offset[X] /= CALIBRATION_SAMPLE
    gyro_offset[Y] /= CALIBRATION_SAMPLE
    gyro_offset[Z] /= CALIBRATION_SAMPLE


##########
# 取得データの正負判定関数
###############
def  ToSigned(unsigne_data):
    if unsigne_data & (0x01 << 15) :
        return -1 * ((unsigne_data ^ 0xffff) + 1)
    return unsigne_data


##########
# MinMaxの算出
###############
def MinMax(value, min_value, max_value):
    if value > max_value:
        value = max_value
    elif value < min_value:
        value = min_value
    
    return value


##########
# 現在の姿勢角度の算出
###############
def Real_Angles():
    global First_Loop, gyro_raw, gyro_angle, complement_angle, lowpassfilter_angle
    
    acc_angle = [0, 0, 0]
    acc_total_vector = 0
    
    # 角速度を初期オフセット値で補正
    gyro_raw[X] -= gyro_offset[X]
    gyro_raw[Y] -= gyro_offset[Y]
    gyro_raw[Z] -= gyro_offset[Z]

    # 角速度から角度算出
    gyro_angle[X] += (-gyro_raw[X] / (TIMER_FREQ * SSF_GYRO))
    gyro_angle[Y] += (-gyro_raw[Y] / (TIMER_FREQ * SSF_GYRO))

    # Z軸のベクトル補正
    gyro_angle[X] += gyro_angle[Y] * math.sin(gyro_raw[Z] * (PI / (TIMER_FREQ * SSF_GYRO * 180)))
    gyro_angle[Y] += gyro_angle[X] * math.sin(gyro_raw[Z] * (PI / (TIMER_FREQ * SSF_GYRO * 180)))

    # 3D加速度ベクトル : √(X² + Y² + Z²)
    acc_total_vector = math.sqrt(math.pow(acc_raw[X], 2) + math.pow(acc_raw[Y], 2) + math.pow(acc_raw[Z], 2))
    
    # 加速度から角度算出
    if abs(acc_raw[X]) < acc_total_vector:
        # ラジアンに変換
        acc_angle[X] = math.asin(acc_raw[Y] / acc_total_vector) * (180 / PI)
        
    if abs(acc_raw[Y]) < acc_total_vector:
        # ラジアンに変換
        acc_angle[Y] = math.asin(acc_raw[X] / acc_total_vector) * (180 / PI)
    
    if First_Loop:
        # 角度のドリフト調整 ---> 値の微調整が必要!!
        gyro_angle[X] = gyro_angle[X] * 0.95 + (-acc_angle[X] * 0.05)
        gyro_angle[Y] = gyro_angle[Y] * 0.95 + (acc_angle[Y] * 0.05)
    else:
        gyro_angle[X] = acc_angle[X]
        gyro_angle[Y] = acc_angle[Y]
        First_Loop = True
    
    # 相補フィルター
    complement_angle[ROLL] = (complement_angle[ROLL] * 0.9) + (gyro_angle[X] * 0.1)
    complement_angle[PITCH] = (complement_angle[PITCH] * 0.9) + (gyro_angle[Y] * 0.1)
#     complement_angle[YAW] = -gyro_raw[Z] / SSF_GYRO
    complement_angle[YAW] = -gyro_raw[Z] / (TIMER_FREQ * SSF_GYRO)
    
    # ローパスフィルタ(10Hz cutoff frequency)
    lowpassfilter_angle[ROLL]  = 0.7 * lowpassfilter_angle[ROLL]  + 0.3 * (gyro_raw[X] / SSF_GYRO)
    lowpassfilter_angle[PITCH] = 0.7 * lowpassfilter_angle[PITCH] + 0.3 * gyro_raw[Y] / SSF_GYRO
    lowpassfilter_angle[YAW]   = 0.7 * lowpassfilter_angle[YAW]   + 0.3 * gyro_raw[Z] / SSF_GYRO
    

##########
# 目標値の設定
###############
def Target_Value():
    global pid_value
    
    pid_value = [0, 0, 0, 0]
    
    if receiver_pulse[THROTTLE] > 1050:
        pid_value[YAW] = Target_Set_Value(0, receiver_pulse[YAW])
    
    pid_value[PITCH] = Target_Set_Value(complement_angle[PITCH], receiver_pulse[PITCH])
    pid_value[ROLL] = Target_Set_Value(complement_angle[ROLL], receiver_pulse[ROLL])


##########
# 目標値の設定（差分計算）
###############
def Target_Set_Value(angle, ch_pulse):
    set_point = 0
    angle = MinMax(angle, -20, 20)
    adjust = angle * 15
    
    if ch_pulse > 1508:
        set_point = ch_pulse - 1508
    elif ch_pulse <  1492:
        set_point = ch_pulse - 1492

    set_point -= adjust
    set_point /= 3

    return set_point


##########
# 誤差算出
###############
def Error_Process():
    global errors, error_sum, delta_err
    
    error_tolerance = 2		# 誤差範囲の許容
    
    # 測定値と目標値との誤差算出
    errors[YAW]   = lowpassfilter_angle[YAW] - pid_value[YAW]
    errors[PITCH] = lowpassfilter_angle[PITCH] - pid_value[PITCH]
    errors[ROLL]  = lowpassfilter_angle[ROLL] - pid_value[ROLL]

    # 誤差の合計 : 積分
    error_sum[YAW]   += errors[YAW]
    error_sum[PITCH] += errors[PITCH]
    error_sum[ROLL]  += errors[ROLL]

    # 許容範囲の算出
    error_sum[YAW]   = MinMax(error_sum[YAW],   -error_tolerance/Ki[YAW],   error_tolerance/Ki[YAW])
    error_sum[PITCH] = MinMax(error_sum[PITCH], -error_tolerance/Ki[PITCH], error_tolerance/Ki[PITCH])
    error_sum[ROLL]  = MinMax(error_sum[ROLL],  -error_tolerance/Ki[ROLL],  error_tolerance/Ki[ROLL])

    # 誤差の前回との差分 : 微分
    delta_err[YAW]   = errors[YAW]   - before_error[YAW]
    delta_err[PITCH] = errors[PITCH] - before_error[PITCH]
    delta_err[ROLL]  = errors[ROLL]  - before_error[ROLL]

    # 誤差の保存
    before_error[YAW]   = errors[YAW]
    before_error[PITCH] = errors[PITCH]
    before_error[ROLL]  = errors[ROLL]


##########
# PID演算
###############
def PID_Calculation():
    global pulse_length_motor1, pulse_length_motor2, pulse_length_motor3, pulse_length_motor4
    
    yaw_pid = 0
    pitch_pid = 0
    roll_pid = 0
    throttle = receiver_pulse[THROTTLE]
    pid_tolerance = 300		# PID値の許容範囲
    
    # モータ出力の初期化
    pulse_length_motor1 = throttle
    pulse_length_motor2 = throttle
    pulse_length_motor3 = throttle
    pulse_length_motor4 = throttle

    # スロットルのレバーが下がっていれば計算しない
    if throttle >= 1050:
        # PID = e.Kp + ∫e.Ki + Δe.Kd
        yaw_pid   = (errors[YAW]   * Kp[YAW])   + (error_sum[YAW]   * Ki[YAW])   + (delta_err[YAW]   * Kd[YAW])
        pitch_pid = (errors[PITCH] * Kp[PITCH]) + (error_sum[PITCH] * Ki[PITCH]) + (delta_err[PITCH] * Kd[PITCH])
        roll_pid  = (errors[ROLL]  * Kp[ROLL])  + (error_sum[ROLL]  * Ki[ROLL])  + (delta_err[ROLL]  * Kd[ROLL])

        # PID値の許容範囲
        roll_pid  = MinMax(roll_pid, -pid_tolerance, pid_tolerance)		# 右側で+、左側で-
        pitch_pid = MinMax(pitch_pid, -pid_tolerance, pid_tolerance)	# 前傾で-、後傾で+
        yaw_pid   = MinMax(yaw_pid, -pid_tolerance, pid_tolerance)		# 時計回りで＋、反時計回りで-

        # モータ出力の演算
        pulse_length_motor1 = throttle - roll_pid + pitch_pid + yaw_pid
        pulse_length_motor2 = throttle + roll_pid + pitch_pid - yaw_pid
        pulse_length_motor3 = throttle - roll_pid - pitch_pid - yaw_pid
        pulse_length_motor4 = throttle + roll_pid - pitch_pid + yaw_pid

    # モータ出力の許容範囲
    pulse_length_motor1 = MinMax(pulse_length_motor1, 1000, 2000)
    pulse_length_motor2 = MinMax(pulse_length_motor2, 1000, 2000)
    pulse_length_motor3 = MinMax(pulse_length_motor3, 1000, 2000)
    pulse_length_motor4 = MinMax(pulse_length_motor4, 1000, 2000)


##########
# モータ出力
###############
def Motor_Output():
    global motor_1_duty, motor_2_duty, motor_3_duty, motor_4_duty, receiver_pulse, rc_connection_flag
    
    throttle = receiver_pulse[THROTTLE]
    
    if rc_connection_flag == False :	# プロポ接続無し時はモータ出力、誤差値を0とする
        motor_1_duty = 0
        motor_2_duty = 0
        motor_3_duty = 0
        motor_4_duty = 0
        
        errors[YAW]   = 0
        errors[PITCH] = 0
        errors[ROLL]  = 0

        error_sum[YAW]   = 0
        error_sum[PITCH] = 0
        error_sum[ROLL]  = 0

        before_error[YAW]   = 0
        before_error[PITCH] = 0
        before_error[ROLL]  = 0
    else :
        # プロポの信号とPWM範囲のスケーリング
        motor_1_duty = (2 ** 16 - 1) * (pulse_length_motor1 - 800)/1_400
        motor_2_duty = (2 ** 16 - 1) * (pulse_length_motor2 - 800)/1_400
        motor_3_duty = (2 ** 16 - 1) * (pulse_length_motor3 - 800)/1_400
        motor_4_duty = (2 ** 16 - 1) * (pulse_length_motor4 - 800)/1_400
                    
        if throttle < 1_100 :		# Trottleが1100より小さければPWM Dutyを0とする
            motor_1_duty = 0
            motor_2_duty = 0
            motor_3_duty = 0
            motor_4_duty = 0
        
        # モータへ出力
        motor_1.duty_u16(int(motor_1_duty))
        motor_2.duty_u16(int(motor_2_duty))
        motor_3.duty_u16(int(motor_3_duty))
        motor_4.duty_u16(int(motor_4_duty))


##########
# メインプログラム
###############
# 定数定義
ROLL            		    = 0
PITCH           		    = 1
THROTTLE        		    = 2
YAW             		    = 3
PI              		    = 3.14
X               		    = 0
Y               		    = 1
Z               		    = 2
CALIBRATION_SAMPLE		= 1000  # キャリブレーションのサンプル回数
TIMER_FREQ				= 250   # メインタイマー周波数 4ms
SSF_GYRO        		    = 65.5  # 角速度感度 ±500°/s

# MPU6050レジスタ名とアドレス設定
MPU6050_ADDR    		    = 0x68
SMPLRT_DIV      		    = 0x19
GYRO_CONFIG     		    = 0x1B
ACCEL_CONFIG    		    = 0x1C
PWR_MGMT_1      		    = 0x6B
FILTER_CONFIG			= 0x1A
ACCEL_XOUT_H    		    = 0x3B
GYRO_XOUT_H     		    = 0x43

# 姿勢角用変数
gyro_raw        		    = [0, 0, 0]
acc_raw         		    = [0, 0, 0]
gyro_offset     		    = [0, 0, 0]
gyro_angle				= [0, 0, 0]
complement_angle    	= [0, 0, 0, 0]
lowpassfilter_angle		= [0, 0, 0, 0]
First_Loop      		    = False

# PIDコントロール
pid_value				= [0, 0, 0, 0]
receiver_pulse			= [1500, 1500, 1000, 1500]
errors					= [0, 0, 0, 0]
error_sum				= [0, 0, 0, 0]
delta_err				= [0, 0, 0, 0]
before_error			    = [0, 0, 0, 0]
Kp 						= [2.0, 2.0, 0, 0.8]
Ki						= [0.05, 0.04, 0, 0.02]
Kd						= [0, 0, 0, 0]
pulse_length_motor1		= 1000
pulse_length_motor2		= 1000
pulse_length_motor3		= 1000
pulse_length_motor4		= 1000

# モータ出力
motor_1_duty				= 0
motor_2_duty				= 0
motor_3_duty				= 0
motor_4_duty				= 0

# タイマーカウント
tim_count				= 1
tim1_flag       		    = False
tim200ms				    = False
tim500ms					= False

# レシーバ
rc_connection_flag		= False
ch_list					= ['ch1', 'ch2', 'ch3', 'ch4']

# その他
calibrate_flag			= False
k = 0

# インスタンスの生成
i2c = I2C(0, scl=Pin(13), sda=Pin(12), freq=400000)

# 内蔵LEDの設定
led_pin = Pin(25, Pin.OUT)
led_pin(0)

# タイマーの設定
tim = Timer()
tim.init(freq=TIMER_FREQ, mode=Timer.PERIODIC, callback=main_count)

# プロポ信号入力ピンの設定
pin6 = Pin(6, Pin.IN, Pin.PULL_UP)
pin7 = Pin(7, Pin.IN, Pin.PULL_UP)
pin8 = Pin(8, Pin.IN, Pin.PULL_UP)
pin9 = Pin(9, Pin.IN, Pin.PULL_UP)

# パルス幅検出用ステート・マシンの設定（4つ）、アセンブリプログラムは同一
# クロック周波数は2MHzとする(1命令が0.5us)
ch_list[0] = StateMachine(0, pulse_width, freq=2_000_000, in_base=pin6, jmp_pin=pin6)
ch_list[1] = StateMachine(1, pulse_width, freq=2_000_000, in_base=pin7, jmp_pin=pin7)
ch_list[2] = StateMachine(2, pulse_width, freq=2_000_000, in_base=pin8, jmp_pin=pin8)
ch_list[3] = StateMachine(3, pulse_width, freq=2_000_000, in_base=pin9, jmp_pin=pin9)

# SM0の割り込みが発生したらhdlr0関数を実行（SM0のみで検知とする）
ch_list[0].irq(hdlr0)

# パルス幅検出用カウントダウン初期値を指定し送信FIFOへ送信
for ch_no in ch_list:
    ch_no.put(5_000)		# プロポの信号範囲が1〜2msとなるので、1usのループとして5,000(5ms)とする
    ch_no.active(1)			# パルス幅検出用ステート・マシンの起動

# DCモータ用PWM出力のオブジェクト生成
# Dutyの設定、16ビット（0～65535）、初期値は0
motor_1  = PWM(machine.Pin(14, machine.Pin.OUT))
motor_1.freq(490)			# 周波数：490Hz
motor_1.duty_u16(0)			# Duty：0%

motor_2  = PWM(machine.Pin(17, machine.Pin.OUT))
motor_2.freq(490)			# 周波数：490Hz
motor_2.duty_u16(0)			# Duty：0%

motor_3  = PWM(machine.Pin(15, machine.Pin.OUT))
motor_3.freq(490)			# 周波数：490Hz
motor_3.duty_u16(0)			# Duty：0%

motor_4  = PWM(machine.Pin(16, machine.Pin.OUT))
motor_4.freq(490)			# 周波数：490Hz
motor_4.duty_u16(0)			# Duty：0%


# MPU6050の初期化
MPU6050_Initialize()

# MPU6050のキャリブレーション
print("キャリブレーション指令待機")

while calibrate_flag == False:
    if tim500ms:
        led_pin.toggle()
        tim500ms = False    

led_pin(0)
print("キャリブレーション開始")
MPU6050_Calibrate()
print("キャリブレーション終了")
print(gyro_offset[X], gyro_offset[Y], gyro_offset[Z])
print("")

# WDTの設定
wdt = WDT(timeout=2000)  # タイムアウト 2s で有効化


# ループ処理
while True:
    wdt.feed()
    if tim1_flag:
        # データの取得
        MPU6050_Read(i2c)
        
        # 現在の姿勢角度の取得
        Real_Angles()
        
        # 目標値の設定
        Target_Value()
        
        # 誤差算出
        Error_Process()
        
        # PID演算
        PID_Calculation()
        
        # モータ出力
        Motor_Output()
        
        k += 1
        if k >= 15: # メインタイマー20回毎にシェルへ表示
            k = 0
#             print("X: ","{:.1f}".format(gyro_raw[X]),"|",
#               "Y: ","{:.1f}".format(gyro_raw[Y]),"|",
#               "Z: ","{:.1f}".format(gyro_raw[Z])) 
#             print("X: ","{:.1f}".format(gyro_angle[X]),"|",
#               "Y: ","{:.1f}".format(gyro_angle[Y]),"|",
#               "Z: ","{:.1f}".format(gyro_angle[Z]))            
#             print("ROLL: ","{:.1f}".format(pid_value[ROLL]),"|",
#               "PITCH: ","{:.1f}".format(pid_value[PITCH]),"|",
#               "YAW: ","{:.1f}".format(pid_value[YAW]))
#             print("ROLL: ","{:.1f}".format(complement_angle[ROLL]),"|",
#               "PITCH: ","{:.1f}".format(complement_angle[PITCH]),"|",
#               "YAW: ","{:.1f}".format(complement_angle[YAW]))
            print("C1: ","{:.1f}".format(motor_1_duty),"|",
              "C2: ","{:.1f}".format(motor_2_duty),"|",
              "C3: ","{:.1f}".format(motor_3_duty),"|",
              "C4: ","{:.1f}".format(motor_4_duty))
#             print("ROLL: ","{:.1f}".format(lowpassfilter_angle[ROLL]),"|",
#               "PITCH: ","{:.1f}".format(lowpassfilter_angle[PITCH]),"|",
#               "YAW: ","{:.1f}".format(lowpassfilter_angle[YAW]))
#             print("C1: ","{:.1f}".format(receiver_pulse[0]),"|",
#               "C2: ","{:.1f}".format(receiver_pulse[1]),"|",
#               "C3: ","{:.1f}".format(receiver_pulse[2]),"|",
#               "C4: ","{:.1f}".format(receiver_pulse[3]))

        tim1_flag = False
    
    if tim200ms:
        if rc_connection_flag == False:
            led_pin.toggle()
        
        tim200ms = False
