
//importing the required libraries

#include <Adafruit_GFX.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

//Inetializing the WiFi clients
WiFiClient espClient;
PubSubClient mqttClient(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//Variables needed for LDR,motor and NODE-RED
char tempAr[6];
char LDR_ar[6];
float val_LDR = 0.0;
int min_angle = 30;
float CF = 0.75;
int angle = 0;
bool isScheduledON = false;
unsigned long scheduledOnTime;

//Inetializing the motor
Servo servo_motor;


//defining the values needed for display

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_RESET -1
#define SCREEN_ADDRESS 0x3c

//defining the pins connected

#define dht_pin 32
#define buzzer 4
#define led 2
#define pb_cancel 5
#define pb_ok 25
#define pb_down 26
#define pb_up 27

const int servo_pin = 18;
const int LDR_pin = 36;
const float GAMMA = 0.7;
const float RL = 50;
//defining the values needed to get real time

#define NTP_SERVER  "pool.ntp.org"
float time_zone = 5.5;
float UTC_OFFSET = time_zone * 3600;
#define UTC_OFFSET_DST 0

//Inetializing the display

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, SCREEN_RESET);


//variables needed for time and alarm

int seconds = 0;
int minutes = 0;
int hours = 0;
int days = 0;
int month = 0;
int year = 0;


//variables needed for alarm and tem/hum

bool alarm_enabled = true;
int n_alarm = 3;
int alarm_hours[] = {0, 1, 0};
int alarm_minutes[] = {1, 10, 0};
bool alarm_triggered[] = {false, false, false};
bool Temp_high = false;
bool Temp_low = false;
bool Hum_high = false;
bool Hum_low = false;



//variables needed for buzzer

int n_tunes = 8;

int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_high = 523;

int tunes[] = {C, D, E, F, G, A, B, C_high};

//variables needed for MENUE

int current_mode = 0;
int n_mode = 5;
String modes[] = {"1            Set     Time zone", "2            Set      Alarm 1", "3            Set      Alarm 2", "4            Set      Alarm 3", "5           Disable    Alarms"};

//the UTC time zones

String time_zones[] = {"-12:00", "-11:00", "-10:00", "-09:30", "-09:00", "-08:00", "-07:00", "-06:00", "-05:00", "-04:00", "-03:30", "-03:00", "-02:00", "-01:00", " 00:00", "+01:00", "+02:00", "+03:00", "+03:30", "+04:00", "+04:30", "+05:00", "+05:30", "+05:45", "+06:00", "+06:30", "+07:00", "+08:00", "+08:45", "+09:00", "+09:30", "+10:00", "+10:30", "+11:00", "+12:00", "+12:45", "+13:00", "+14:00"};

//Inetializing the dhtsensor variable
DHTesp dhtsensor;

void setup() {

  //Inetialising the pins to Input/Output

  Serial.begin(115200);
  pinMode(buzzer, OUTPUT);
  pinMode(led, OUTPUT);
  pinMode(pb_cancel, INPUT);
  pinMode(pb_ok, INPUT);
  pinMode(pb_down, INPUT);
  pinMode(pb_up, INPUT);

  dhtsensor.setup(dht_pin, DHTesp::DHT22);

  //Inetializing the display

  if (! display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("display is not working");
    for (;;);
  }

  //Inetializing the wifi for time display

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_text("connecting to Wi-FI", 0, 0, 2);
  }
  //function to setup MQTT client
  SetupMqtt();

  //connecting the motor to the pin
  servo_motor.attach(servo_pin, 500, 2400);

  timeClient.begin();

  display.clearDisplay();
  print_text("connected to Wi-FI", 0, 0, 2);

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  display.display();
  delay(2000);

  display.clearDisplay();

  print_text("welcome  To Medibox", 10, 10, 2);
  delay(2000);
  display.clearDisplay();

}

void loop() {
  if (!mqttClient.connected()) {
    connectToBroker();
  }
  //getting the flux from the function
  float lux = light_intensity();

  mqttClient.loop();

  updateTemperature();

  String(lux, 2).toCharArray(LDR_ar, 6);

  Serial.println(tempAr);

  mqttClient.publish("ENTC-TEMP", tempAr);
  mqttClient.publish("ENTC-INTE", LDR_ar);

  checkSchedule();

  delay(1000);

  angle = min_angle + (180 - min_angle) * CF * lux;
  servo_motor.write(angle);

  delay(1000);

  update_time_with_check_alarm();   //updating the time and checking the alarm

  if (digitalRead(pb_ok) == LOW) {
    delay(200);
    go_to_menue();  //if menue button is pressed
  }

  check_temp();  //checking for temp/Humidi

}




float light_intensity() {
  val_LDR = analogRead(LDR_pin);
  val_LDR = map(val_LDR, 4095, 0, 1024, 0);
  float vol = val_LDR / 1024. *5;
  float res = 2000 * vol / (1 - vol / 5);
  float lux = pow(RL * 1e3 * pow(10, GAMMA) / res, (1 / GAMMA)) / 85168.02;
  //Serial.println(lux);
  return lux;
}


void buzzerOn(bool on) {
  if (on) {
    tone(buzzer, 256);
  } else {
    noTone(buzzer);
  }
}

void checkSchedule() {
  if (isScheduledON) {
    unsigned long currentTime = getTime();
    if (currentTime > scheduledOnTime) {
      buzzerOn(true);
      isScheduledON = false;
      Serial.println("Scheduled ON");
      mqttClient.publish("button_on_off", "1");
      mqttClient.publish("Sche_on", "0");
    }
  }
}


void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32-345464")) {
      Serial.print("connected");
      mqttClient.subscribe("switch");
      mqttClient.subscribe("buzz");
      mqttClient.subscribe("min_angle");
      mqttClient.subscribe("cont_fac");
    } else {
      Serial.print("failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}


unsigned long getTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}


void SetupMqtt() {
  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(receiveCallback);
}

void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  Serial.println("] ");
  if (strcmp(topic, "min_angle") == 0) {
    min_angle = atoi(payloadCharAr);
  } else if (strcmp(topic, "cont_fac") == 0) {
    CF == atoi(payloadCharAr);
  }
  else if (strcmp(topic, "buzz") == 0) {
    buzzerOn(payloadCharAr[0] == '1');
  } else if (strcmp(topic, "switch") == 0) {
    if (payloadCharAr[0] == 'N') {
      isScheduledON = false;
    } else {
      isScheduledON = true;
      scheduledOnTime = atol(payloadCharAr);
    }
  }
}

void updateTemperature() {
  TempAndHumidity data = dhtsensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr, 6);
}

//fuction for printing text

void print_text(String text, int coloumn, int row, int text_size) {
  display.clearDisplay();
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(coloumn, row);
  display.println(text);
  display.display();
}

//the front view of the display function

void print_display() {

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println( String(days) + "/" + String(month) + "/" + "20" + String(year));
  display.setCursor(20, 24);
  display.println( String(hours) + ":" + String(minutes) + ":" + String(seconds));

  display.setTextSize(1.5);
  display.setCursor(0, 50);
  if (Temp_high) {
    display.println("Temp High");
    digitalWrite(led, HIGH);
    delay(500);
    digitalWrite(led, LOW);
  }
  else if (Temp_low) {
    display.println("Temp Low");
    digitalWrite(led, HIGH);
    delay(500);
    digitalWrite(led, LOW);
  }
  else {
    display.println("");
    digitalWrite(led, LOW);
  }

  display.setTextSize(1.5);
  display.setCursor(65, 50);
  if (Hum_high) {
    display.println("Humid High");
    digitalWrite(led, HIGH);
    delay(500);
    digitalWrite(led, LOW);
  }
  else if (Hum_low) {
    display.println("Humid Low");
    digitalWrite(led, HIGH);
    delay(500);
    digitalWrite(led, LOW);
  }
  else {
    display.println("");
    digitalWrite(led, LOW);
  }

  display.display();

}

//updating the time using NTP server

void update_time() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);

  char timeMonth[3];
  strftime(timeMonth, 3, "%m", &timeinfo);
  month = atoi(timeMonth);

  char timeYear[3];
  strftime(timeYear, 3, "%y", &timeinfo);
  year = atoi(timeYear);


}

//function for ringing the alarm

void ring_alarm() {
  display.clearDisplay();
  print_text("medicine time!", 0, 0, 2);

  digitalWrite(led, HIGH);
  bool break_happened = false;

  while (break_happened == false && digitalRead(pb_cancel) == HIGH) {
    for (int j = 0; j < n_tunes; j++) {
      if (digitalRead(pb_cancel) == LOW) {
        delay(200);
        break_happened = true;
        break;
      }
      tone(buzzer, tunes[j]);
      delay(500);
      noTone(buzzer);
      delay(2);

    }

  }

  digitalWrite(led, LOW);
  display.clearDisplay();


}

//checking for alarms

void update_time_with_check_alarm() {
  update_time();
  print_display();

  if (alarm_enabled) {
    for (int i = 0; i < n_alarm; i++) {
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        ring_alarm();
        alarm_triggered[i] = true;
      }

    }
  }

}

//checking for button pressed
int the_button_pressed() {
  while (true) {
    if (digitalRead(pb_ok) == LOW) {
      delay(200);
      return pb_ok;
    }
    else if (digitalRead(pb_up) == LOW) {
      delay(200);
      return pb_up;
    }
    else if (digitalRead(pb_down) == LOW) {
      delay(200);
      return pb_down;
    }
    else if (digitalRead(pb_cancel) == LOW) {
      delay(200);
      return pb_cancel;
    }
    update_time();
  }
}



//function taking care of the menue

void go_to_menue() {
  while (digitalRead(pb_cancel) == HIGH) {
    display.clearDisplay();
    print_text(modes[current_mode], 0, 0, 2);

    int pressed = the_button_pressed();

    if (pressed == pb_up) {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % n_mode;
    }

    else if (pressed == pb_down) {
      delay(200);
      current_mode -= 1;
      if (current_mode < 0) {
        current_mode = n_mode - 1;
      }
    }

    else if (pressed == pb_ok) {
      delay(200);
      //Serial.println(current_mode);
      run_mode(current_mode);
    }

    else if (pressed == pb_cancel) {
      delay(200);
      break;
    }

  }

}

//function for setting the time zone

void set_time_zone() {
  int tem_zone_num = 0;
  while (true) {
    display.clearDisplay();
    print_text("Enter zone            " + time_zones[tem_zone_num], 0, 0, 2);

    int pressed = the_button_pressed();

    if (pressed == pb_up) {
      delay(200);
      tem_zone_num += 1;
      tem_zone_num = tem_zone_num % 38;
    }

    else if (pressed == pb_down) {
      delay(200);
      tem_zone_num -= 1;
      if (tem_zone_num < 0) {
        tem_zone_num = 37;
      }
    }

    else if (pressed == pb_ok) {
      delay(200);
      String zone1 = time_zones[tem_zone_num].substring(0, 3);
      String zone2 = time_zones[tem_zone_num].substring(4, 6);
      time_zone = (float)zone1.toInt();

      if (zone2 == "30" && time_zone > 0) {
        time_zone += 0.5;
      } else if (zone2 == "45" && time_zone > 0) {
        time_zone += 0.75;
      } else if (zone2 == "30" && time_zone < 0) {
        time_zone -= 0.5;
      } else if (zone2 == "45" && time_zone < 0) {
        time_zone -= 0.75;
      }
      print_text(String(time_zone), 0, 0, 2);
      delay(1000);
      UTC_OFFSET = time_zone * 3600;
      configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
      break;
    }

    else if (pressed == pb_cancel) {
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_text("Time zone is set", 0, 0, 2);
  delay(1000);

}

//fuction for setting the alarm

void set_alarm(int alarm) {
  alarm_triggered[alarm] = false;
  alarm_enabled = true;
  int temp_hour = alarm_hours[alarm];
  while (true) {
    display.clearDisplay();
    print_text("Enter hour :" + String(temp_hour), 0, 0, 2);

    int pressed = the_button_pressed();

    if (pressed == pb_up) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }

    else if (pressed == pb_down) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0) {
        temp_hour = 23;
      }
    }

    else if (pressed == pb_ok) {
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    }

    else if (pressed == pb_cancel) {
      delay(200);
      break;
    }
  }

  int temp_minute = alarm_minutes[alarm];
  while (true) {
    display.clearDisplay();
    print_text("Enter minute :" + String(temp_minute), 0, 0, 2);

    int pressed = the_button_pressed();

    if (pressed == pb_up) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }

    else if (pressed == pb_down) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0) {
        temp_minute = 59;
      }
    }

    else if (pressed == pb_ok) {
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    }

    else if (pressed == pb_cancel) {
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_text("Alarm is   set", 0, 0, 2);
  delay(1000);

}

//checking the current mode and run it

void run_mode(int current) {
  if (current == 0) {
    set_time_zone();
  }

  else if (current == 1 || current == 2 || current == 3) {
    set_alarm(current - 1);
  }

  else if (current == 4) {
    alarm_enabled = false;
    print_text("All alarms disabled", 0, 0, 2);
    delay(1000);
  }
}

//checking for whether the Temp and Humidity in the given range

//blinking the light and display on screen if out of range

void check_temp() {
  TempAndHumidity data = dhtsensor.getTempAndHumidity();

  //checking whether the temperature is in the given range

  if (data.temperature > 32) {
    digitalWrite(led, HIGH);
    Temp_high = true;
    Temp_low = false;
  }
  else if (data.temperature < 26) {
    digitalWrite(led, HIGH);
    Temp_low = true;
    Temp_high = false;
  } else {
    digitalWrite(led, LOW);
    Temp_high = false;
    Temp_low = false;
  }

  //checking whether the humidity is in the given range

  if (data.humidity > 80) {
    Hum_high = true;
    Hum_low = false;
  }
  else if (data.humidity < 60) {
    Hum_low = true;
    Hum_high = false;
  } else {
    Hum_high = false;
    Hum_low = false;
  }
}