import processing.serial.*;

// シリアル通信用変数定義
int ch1, ch2, ch3, ch4;

// 画面描画用変数定義
PFont myFont_1, myFont_2, myFont_3, myFont_4;
int val_text_posi = -1150;
float maxRange = 1200;
float offsetY = 900;
String text_title = "Pirone プロポからの受信信号";
String text_team_name = "Pirone Teams.";

// （以下、環境に合わせて設定変更）
String Font_Name_1 = "ＭＳ Ｐゴシック";
String Font_Name_2 = "Meiryo UI Bold Italic";
String Font_Name_3 = "HGP創英角ｺﾞｼｯｸUB";
String Font_Name_4 = "Yu Gothic UI Bold";
String com_port = "COM6";
int baudrate = 115200;
int FRATE = 50;

// クラスのインスタンス
Serial myPort;
graphMonitor ReceiverGraph;


void setup() {
  size(1360, 700, P3D);
  frameRate(FRATE);
  smooth(4);
  
  myPort = new Serial(this, com_port, baudrate);
  
  myFont_1 = createFont(Font_Name_1, 30);
  myFont_2 = createFont(Font_Name_2, 30);
  myFont_3 = createFont(Font_Name_3, 30);
  myFont_4 = createFont(Font_Name_4, 30);
  
  ReceiverGraph = new graphMonitor(100, 50, 1000, 600);
}


void draw() {
  background(#015F0D);
  ReceiverGraph.graphDraw(ch1, ch2, ch3, ch4);
}

class graphMonitor {
    String TITLE;
    int X_POSITION, Y_POSITION;
    int X_LENGTH, Y_LENGTH;
    float [] y1, y2, y3, y4;
    
    graphMonitor(int _X_POSITION, int _Y_POSITION, int _X_LENGTH, int _Y_LENGTH) {
      X_POSITION = _X_POSITION;
      Y_POSITION = _Y_POSITION;
      X_LENGTH   = _X_LENGTH;
      Y_LENGTH   = _Y_LENGTH;
      y1 = new float[X_LENGTH];
      y2 = new float[X_LENGTH];
      y3 = new float[X_LENGTH];
      y4 = new float[X_LENGTH];
      for (int i = 0; i < X_LENGTH; i++) {
        y1[i] = 0;
        y2[i] = 0;
        y3[i] = 0;
        y4[i] = 0;
      }
    }

    void graphDraw(float _y1, float _y2, float _y3, float _y4) {
      y1[X_LENGTH - 1] = _y1;
      y2[X_LENGTH - 1] = _y2;
      y3[X_LENGTH - 1] = _y3;
      y4[X_LENGTH - 1] = _y4;
      for (int i = 0; i < X_LENGTH - 1; i++) {
        y1[i] = y1[i + 1];
        y2[i] = y2[i + 1];
        y3[i] = y3[i + 1];
        y4[i] = y4[i + 1];
      }
      pushMatrix();

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
      textFont(myFont_2);
      textSize(20);
      text(text_team_name,1050,-5);
      
      // Y軸目盛値表示
      textFont(myFont_3);
      textSize(24);
      textAlign(RIGHT);
      text(nf((maxRange / 2 + offsetY), 0, 0), -5, Y_LENGTH / 2);
      text(nf((maxRange + offsetY), 0, 0), -5, 18);
      text(int(offsetY), -5, Y_LENGTH); 
      text(nf((maxRange / 4 + offsetY), 0, 0), -5, Y_LENGTH * 3 / 4);
      text(nf((maxRange * 3 / 4 + offsetY), 0, 0), -5, Y_LENGTH / 4);

      // X軸目盛値その他表示
      textFont(myFont_3);
      textSize(18);
      textAlign(LEFT);
      text("画面更新  ： " + FRATE + " フレート", 1030, 300);
      text("通信ポート： " + com_port, 1030, 330);
      text("BAUDRATE  ： " + baudrate, 1030, 360);
      text(0 + "S", X_LENGTH, 620);

      // 取得データのライン描画
      translate(0, Y_LENGTH);
      scale(1, -1);
      strokeWeight(2);
      
      for (int i = 0; i < X_LENGTH - 1; i++) {
        // ch1のライン
        stroke(255, 255, 0);
        line(i, (y1[i] * Y_LENGTH / maxRange) - (Y_LENGTH * offsetY / maxRange), i + 1, (y1[i + 1] * Y_LENGTH / maxRange) - (Y_LENGTH * offsetY / maxRange));
        // ch2のライン
        stroke(255, 0, 255);
        line(i, (y2[i] * Y_LENGTH / maxRange) - (Y_LENGTH * offsetY / maxRange), i + 1, (y2[i + 1] * Y_LENGTH / maxRange) - (Y_LENGTH * offsetY / maxRange));
        // ch3のライン
        stroke(255, 255, 255);
        line(i, (y3[i] * Y_LENGTH / maxRange) - (Y_LENGTH * offsetY / maxRange), i + 1, (y3[i + 1] * Y_LENGTH / maxRange) - (Y_LENGTH * offsetY / maxRange));
        // ch4のライン
        stroke(0, 255, 255);
        line(i, (y4[i] * Y_LENGTH / maxRange) - (Y_LENGTH * offsetY / maxRange), i + 1, (y4[i + 1] * Y_LENGTH / maxRange) - (Y_LENGTH * offsetY / maxRange));
      }
      
      // 各数値の表示枠描画
      scale(1, -1);
      textFont(myFont_4);
      textSize(18);        
      translate(0, Y_LENGTH);
      fill(0);
      stroke(0);
      rect(1130, val_text_posi, 100, 25);
      rect(1130, val_text_posi + 50, 100, 25);
      rect(1130, val_text_posi + 100, 100, 25);
      rect(1130, val_text_posi + 150, 100, 25);
      // 補足線描画
      stroke(255, 255, 0);
      line(1040,val_text_posi + 20,1110,val_text_posi + 20);
      stroke(255, 0, 255);
      line(1040,val_text_posi + 70,1110,val_text_posi + 70);
      stroke(255, 255, 255);
      line(1040,val_text_posi + 120,1110,val_text_posi + 120);
      stroke(0, 255, 255);
      line(1040,val_text_posi + 170,1110,val_text_posi + 170);
      
      fill(255);
      text(nf(ch1, 0, 0), 1180, val_text_posi + 20);
      text(nf(ch2, 0, 0), 1180, val_text_posi + 70);
      text(nf(ch3, 0, 0), 1180, val_text_posi + 120);
      text(nf(ch4, 0, 0), 1180, val_text_posi + 170);
      textFont(myFont_4);
      textSize(18);
      text("ch1",1080,val_text_posi + 10);
      text("ch2",1080,val_text_posi + 60);
      text("ch3",1080,val_text_posi + 110);
      text("ch4",1080,val_text_posi + 160);
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
    ch1 = int(temp[0]);
    ch2 = int(temp[1]);
    ch3 = int(temp[2]);
    ch4 = int(temp[3]);
  }
}
