/**
   Информационная панель системы кв152
   автономная по wifi, данные берет из mqtt
   Изменено: 2020-05-22

   Подключение NodeMCU к LCD1604 через i2c
   NodeMCU I2C 1602 LCD
   Vin VCC
   GND GND
   D1  SCL
   D2  SDA

   Подключение датчика движения
   Vin VCC
   GND GND
   D3  OUT

   Что сделать:
   1. В случае потери связи WiFi - переподключение
   2. В случае потери связи с MQTT - переподключение
   3. Блок данных от MQTT приходит с разделителем на строки в виде |, реализовать разделение на строки
   4. Проверить, что вывод данных их MQTT блока происходит по строкам (4 строки LCD), а не через строку
   5 Проверить, в списке доступных Wifi точек подключения появился какой-то ESP8266, если это наш NodeMCU инфопанели
     найти настройки модуля и убрать режим точки доступа или запаролить
   6. В идеале м.б. в одной квартире (подключенных к одному роутеру) несколько инфо-панелей, каждая из которых
      может выводитб свою индивидуальную информацию и отправлять индивидуальный сигнал о движении, т.е. работать
      с индивидуальными топиками MQTT. М.б. в роутере организовать явное выделение специфического IP для каждой
      инфопанеле, а инфопанель будет обращаться к топику, который будет именован общим префиксом, плюс IP инфопанели.
*/

#include <kv152_cfg.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>  // This library is already built in to the Arduino IDE
#include <LiquidCrystal_I2C.h> //This library you can add via Include Library > Manage Library > 

#define DEBUG false
#define LCD1602 false
#define LCD2004 true
#define LCDI2CADRESS 0x27

/*0x27 means the address of this 1602 I2C LCD display,different lcd may have the different address,
                                     if the LCD do not work,please read below post about how to get the right address for your I2C LCD dispaly:
                                     http://osoyoo.com/2014/12/07/16x2-i2c-liquidcrystal-displaylcd/ */
#if LCD1602                                     
LiquidCrystal_I2C lcd(LCDI2CADRESS, 16, 2);
#endif
#if LCD2004                                     
LiquidCrystal_I2C lcd(LCDI2CADRESS, 20, 4);
#endif


const char* ssid      = WIFISSID;
const char* password  = WIFIPASSWORD;

const char* mqtt_server = MQTTSERVER;
const char *clientUser = MQTTUSER;
const char *clientPwd  = MQTTPASSWORD;
const char *clientId = "InfoPanelESP";
    

#define MQTT_TOPIC_IN "kv152/infopanel"
#define MQTT_TOPIC_OUT "kv152/motion"

const int PIR = 0;
int  PIRst = LOW;
int  PIRst_old = LOW;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;





/**
   Отобразить строку на LCD
   @param int row0 строка 0..3
   @param int col0 столбец с которого отобразиться строка 0...19
   @param char* строка
*/
void LcdPrint(uint8_t  row0, uint8_t col0, char *str, unsigned int len) {
  #if DEBUG
  Serial.print("void LcdPrint(int ");
  Serial.print(row0);
   Serial.print(", int ");
   Serial.print(col0);
   Serial.print(", char '");
   Serial.print(str);
   Serial.print("', unsigned int ");
   Serial.print(len);
   Serial.println(")");
#endif

  uint8_t row = row0;
  uint8_t col = col0;



  int i;
  for (i = 0; i < len; i++) {
    //Во входящей строке спец символ | означает переход на новую строку
    if ( str[i]  == '|' ) {
      row++;
      col = 0;
      i++;//пропускаем символ разделитель
      if ( row >= 4 ) break;
    }
    lcd.setCursor(col, row ); lcd.write( str[i] );

    
    col++; if ( col >= 20 ) break;
  }
}






void setup_wifi() {
  char msg[21];
  delay(100);
  // We start by connecting to a WiFi network
#if DEBUG
  Serial.print("Connecting to ");
  Serial.println(ssid);
#endif
 strcpy(msg, "WiFi .." ); LcdPrint(0, 0, msg, strlen(msg)  );
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
#if DEBUG
    Serial.print(".");
#endif
  }
  randomSeed(micros());
#if DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#endif
  strcpy(msg, "WiFi ON" ); LcdPrint(0, 0, msg, strlen(msg)  );
  //LcdPrint(0,9, WiFi.localIP() );


#if DEBUG
#if LCD1602 
Serial.println( "Используем LCD1602" );
#endif

#if LCD2004 
Serial.println( "Используем LCD2004" );
#endif

Serial.print( "use LCD address" );
Serial.println(LCDI2CADRESS);
 
#endif
}


/**
 * MQTT колбэк подписки.
 * Пришло сообщение от сервера, отобразим его на панели
 */
void callback(char* topic, byte* payload, unsigned int length) {
#if DEBUG
  Serial.print("Command from MQTT broker is : [");
  Serial.print(topic);
  Serial.println("]");
  Serial.print(" publish data is:");
#endif
  lcd.clear();


#if DEBUG
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
#endif

  //Сообщение печатаем с начала панели, внутри процедуры длинное сообщение разделиться на подстроки
  LcdPrint(0, 0, (char *)payload, length  );


#if DEBUG
Serial.println();
#endif
} //end callback






void reconnect() {
  char msg[21];
  // Loop until we're reconnected
  while (!client.connected())  {
#if DEBUG
    Serial.print("Attempting MQTT connection...");
#endif

    strcpy(msg, "MQTT .." ); LcdPrint(1, 0, msg, strlen(msg)  );



    // Attempt to connect
    //if you MQTT broker has clientID,username and password
    //please change following line to    if (client.connect(clientId,userName,passWord))
    if (client.connect(clientId, clientUser, clientPwd  ))
    {
#if DEBUG
      Serial.println("connected");
#endif
      strcpy(msg, "MQTT ON" ); LcdPrint(1, 0, msg, strlen(msg)  );

      //once connected to MQTT broker, subscribe command if any
      client.subscribe(MQTT_TOPIC_IN);

    } else {
#if DEBUG
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
#endif
      strcpy(msg, "MQTT Err" ); LcdPrint(1, 0, msg, strlen(msg)  );

      // Wait 6 seconds before retrying
      delay(6000);
    }
  }
} //end reconnect()





void setup() {
  char msg[21];
#if DEBUG
  Serial.begin(115200);
#endif
  lcd.init();   // initializing the LCD
  lcd.backlight(); // Enable or Turn On the backlight
 
  strcpy(msg, "CTAPT..." ); LcdPrint(0, 0, msg, strlen(msg)  );
  
  setup_wifi();


 

  client.setServer(mqtt_server, 1883);

  pinMode (PIR, INPUT);

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.setCallback(callback);
  client.loop();


  PIRst = digitalRead(PIR);
  if (PIRst_old == LOW && PIRst == HIGH) {
#if DEBUG
    Serial.println("motion on");
#endif
    lcd.backlight();
    client.publish(MQTT_TOPIC_OUT, "1");
  }

  if (PIRst_old == HIGH && PIRst == LOW) {
#if DEBUG
    Serial.println("motion off");
#endif
    lcd.noBacklight();
    client.publish(MQTT_TOPIC_OUT, "0");
  }
  PIRst_old = PIRst;

}
