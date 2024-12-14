import processing.serial.*;    // シリアル
import processing.opengl.*;    // OPENGL

// インスタンス
PShape Pirone;
Serial myPort;

String com_port = "COM6";
int baud_rate = 115200;

float []serial_data = new float [3];
boolean serial_flag = false;

void setup() {
  // キャンパスサイズ、OPENGL使用
  size(500, 500, OPENGL);

  // 3Dデータの読み込み、Z軸を反転しておく
  Pirone = loadShape("Pirone_001.obj");
  Pirone.scale(2);
  Pirone.rotateZ(PI);
  
  // シリアルポートの設定
  myPort = new Serial(this, com_port, baud_rate);
}

void draw() {
  // シリアルデータが届いたら実行
  if (serial_flag) { 
    background(0xb8f0ff);
    lights();
    
    // 3Dデータの座標軸を都度リセット
    Pirone.resetMatrix();
    Pirone.scale(2);
    Pirone.rotateZ(PI);
    
    translate(width / 2, height / 2, 0);   
    Pirone.rotateZ(radians(serial_data[0]));
    Pirone.rotateX(radians(serial_data[1]));
    Pirone.rotateY(radians(serial_data[2]));
    shape(Pirone);
    
    serial_flag = false;
  }
}

void serialEvent(Serial myPort) {
  String serial_raw = myPort.readStringUntil('\n');
  
  if (serial_raw != null) {
    serial_raw = trim(serial_raw);
    serial_data = float(split(serial_raw, ','));
    serial_flag = true;
  }
}
