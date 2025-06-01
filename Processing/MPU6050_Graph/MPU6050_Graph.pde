import processing.serial.*;

// シリアル通信用変数定義
float ax_raw, ay_raw, az_raw, gx_raw, gy_raw, gz_raw;

// 画面描画用変数定義
PFont myFont_1, myFont_2, myFont_3, myFont_4;
int y_text_posi = -350;
int x_text_posi = 0;
int offset = 0;
int y_offset = 0;
float maxRange = 0;
String text_title;
String text_team_name = "Pirone Teams.";
float value_1, value_2, value_3;
String name_1, name_2, name_3;

// （以下、環境に合わせて設定変更）
String Font_Name_1 = "ＭＳ Ｐゴシック";
String Font_Name_2 = "Meiryo UI Bold Italic";
String Font_Name_3 = "HGP創英角ｺﾞｼｯｸUB";
String Font_Name_4 = "Yu Gothic UI Bold";
String com_port = "COM5";
int baudrate = 115200;
int FRATE = 50;

// クラスのインスタンス
Serial myPort;
graphMonitor Accel_Graph;
graphMonitor Gyro_Graph;


void setup() {
  size(1360, 800, P3D);
  frameRate(FRATE);
  smooth(4);
  
  myPort = new Serial(this, com_port, baudrate);
  Accel_Graph = new graphMonitor(100, 50, 1000, 300);
  Gyro_Graph = new graphMonitor(100, 450, 1000, 300);
  
  myFont_1 = createFont(Font_Name_1, 30);
  myFont_2 = createFont(Font_Name_2, 30);
  myFont_3 = createFont(Font_Name_3, 30);
  myFont_4 = createFont(Font_Name_4, 30);
   
}


void draw() {
  background(#015F0D);
  
  Accel_Graph.graphDraw(ax_raw, ay_raw, az_raw, "accel");
  Gyro_Graph.graphDraw(gx_raw, gy_raw, gz_raw, "gyro");
}

class graphMonitor {
    String TITLE;
    int X_POSITION, Y_POSITION;
    int X_LENGTH, Y_LENGTH;
    float [] y11, y12, y13;
    
    graphMonitor(int _X_POSITION, int _Y_POSITION, int _X_LENGTH, int _Y_LENGTH) {
      X_POSITION = _X_POSITION;
      Y_POSITION = _Y_POSITION;
      X_LENGTH   = _X_LENGTH;
      Y_LENGTH   = _Y_LENGTH;
      y11 = new float[X_LENGTH];
      y12 = new float[X_LENGTH];
      y13 = new float[X_LENGTH];
      for (int i = 0; i < X_LENGTH; i++) {
        y11[i] = 0;
        y12[i] = 0;
        y13[i] = 0;
      }
    }

    void graphDraw(float _y11, float _y12, float _y13, String graph_name) {
      y11[X_LENGTH - 1] = _y11;
      y12[X_LENGTH - 1] = _y12;
      y13[X_LENGTH - 1] = _y13;
      for (int i = 0; i < X_LENGTH - 1; i++) {
        y11[i] = y11[i + 1];
        y12[i] = y12[i + 1];
        y13[i] = y13[i + 1];
      }
      pushMatrix();
      
      // グラフ種別による各変数の設定
      if (graph_name == "accel") {
        text_title = "Acceleration(加速度)信号";
        maxRange = 3;
        value_1 = ax_raw;
        value_2 = ay_raw;
        value_3 = az_raw;
        name_1 = "ax";
        name_2 = "ay";
        name_3 = "az";
        x_text_posi = 1200;
        offset = 0;
        y_offset = -24;
      }else{
        text_title = "Gyro(角速度)信号";
        maxRange = 1000;
        value_1 = gx_raw;
        value_2 = gy_raw;
        value_3 = gz_raw;
        name_1 = "gx";
        name_2 = "gy";
        name_3 = "gz";
        x_text_posi = 1180;
        offset  = 10;
        y_offset = 75;
      }
      
      // 画面補助線の描画
      translate(X_POSITION, Y_POSITION);
      fill(0);
      stroke(130);
      strokeWeight(1);
      rect(0, 0, X_LENGTH, Y_LENGTH);
      line(0, Y_LENGTH / 2, X_LENGTH, Y_LENGTH / 2);
      line(0, Y_LENGTH / 4, X_LENGTH, Y_LENGTH / 4);
      line(0, Y_LENGTH / 4 * 3, X_LENGTH, Y_LENGTH / 4 * 3);
      line(X_LENGTH / 2, 0, X_LENGTH / 2 , Y_LENGTH);
      line(X_LENGTH / 4, 0, X_LENGTH / 4 , Y_LENGTH);
      line(X_LENGTH / 4 * 3, 0, X_LENGTH / 4 * 3, Y_LENGTH);
      
      // タイトル表示
      textFont(myFont_1);
      textSize(32);
      fill(255);
      textAlign(LEFT, BOTTOM);
      text(text_title, 20, -5);
      
      // チーム名表示
      if (graph_name == "accel") {
        textFont(myFont_2);
        textSize(20);
        text(text_team_name,1050,-5);
      }
      
      // Y軸目盛値表示
      textFont(myFont_3);
      textSize(24);
      textAlign(RIGHT);
      text(nf(0, 0, 0), -5, Y_LENGTH / 2);
      text(nf((maxRange / 2), 0, 0), -5, 18);
      text(nf((-maxRange / 2), 0, 0), -5, Y_LENGTH); 
      text(nf((-maxRange / 4), 0, 0), -5, Y_LENGTH * 3 / 4);
      text(nf((maxRange / 4), 0, 0), -5, Y_LENGTH / 4);

      // X軸目盛値その他表示
      if (graph_name == "gyro") {
        textFont(myFont_3);
        textSize(18);
        textAlign(LEFT);
        text("画面更新  ： " + FRATE + " フレート", 1030, 220);
        text("通信ポート： " + com_port, 1030, 250);
        text("BAUDRATE  ： " + baudrate, 1030, 280);
      }

      // 取得データのライン描画
      translate(0, Y_LENGTH / 4);
      scale(1, -1);
      strokeWeight(2);
      
      for (int i = 0; i < X_LENGTH - 1; i++) {
        // ax_raw,gx_rawのライン
        stroke(255, 255, 0);
        line(i, (y11[i] * Y_LENGTH / maxRange) - (Y_LENGTH / maxRange) - y_offset, i + 1, (y11[i + 1] * Y_LENGTH / maxRange) - (Y_LENGTH / maxRange) - y_offset);
        // ay_raw,gy_rawのライン
        stroke(255, 0, 255);
        line(i, (y12[i] * Y_LENGTH / maxRange) - (Y_LENGTH / maxRange) - y_offset, i + 1, (y12[i + 1] * Y_LENGTH / maxRange) - (Y_LENGTH / maxRange) - y_offset);
        // az_raw,gz_rawのライン
        stroke(255, 255, 255);
        line(i, (y13[i] * Y_LENGTH / maxRange) - (Y_LENGTH / maxRange) - y_offset, i + 1, (y13[i + 1] * Y_LENGTH / maxRange) - (Y_LENGTH / maxRange) - y_offset);
      }
      
      // 各数値の表示枠描画
      scale(1, -1);
      textFont(myFont_4);
      textSize(18);        
      translate(0, Y_LENGTH);
      fill(0);
      stroke(0);
      rect(1130, y_text_posi, 100, 25);
      rect(1130, y_text_posi + 50, 100, 25);
      rect(1130, y_text_posi + 100, 100, 25);
      // 補足線描画
      stroke(255, 255, 0);
      line(1040,y_text_posi + 20,1110,y_text_posi + 20);
      stroke(255, 0, 255);
      line(1040,y_text_posi + 70,1110,y_text_posi + 70);
      stroke(255, 255, 255);
      line(1040,y_text_posi + 120,1110,y_text_posi + 120);
      
      fill(255);
      text(nf(value_1, 0, 0), x_text_posi - offset, y_text_posi + 20);
      text(nf(value_2, 0, 0), x_text_posi - offset, y_text_posi + 70);
      text(nf(value_3, 0, 0), x_text_posi - offset, y_text_posi + 120);
      textFont(myFont_4);
      textSize(18);
      text(name_1,x_text_posi - 120,y_text_posi + 10);
      text(name_2,x_text_posi - 120,y_text_posi + 60);
      text(name_3,x_text_posi - 120,y_text_posi + 110);
      scale(1, -1);
      
      popMatrix();
    }
}


// ******************
// シリアル通信待機
// ******************
void serialEvent(Serial myPort) {
  String str_data = null;
  String temp[];
  
  str_data = myPort.readStringUntil('\n');
  if (str_data != null){
    str_data = trim(str_data);
    temp = split(str_data,",");
    ax_raw = float(temp[0]);
    ay_raw = float(temp[1]);
    az_raw = float(temp[2]);
    gx_raw = float(temp[3]);
    gy_raw = float(temp[4]);
    gz_raw = float(temp[5]);
  }
}
