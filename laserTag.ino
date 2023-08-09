//WiFi Libraries
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "ESP8266httpUpdate.h"

//IR libraryfor ESP8266
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

//EEPROM library
//#include <EEPROM.h>

#include "SSD1306Wire.h"  //https://github.com/ThingPulse/esp8266-oled-ssd1306





#define VERSION 4
String ssid = "Point";         //my WiFi or Hotspot name
String ssidPass = "12345678";  //my WiFi Password

//Pin declarations
#define receiver_pin 0     // 38khz TSOP Receiver
#define transmiter_pin 12  // Infra Red Led----transmits coded IR pulses
#define trigger_pin 2      // Pushbutton (pulled-up) acts as trigger for gun
#define buzzer_pin 13      // (OPTIONAL) buzzer to indicate firing and receiving of signal

#define red_pin 14   //RGB red (OPTIONAL)
#define blue_pin 16  //RGB blue (OPTIONAL)

#define display_pin1 4  // (OPTIONAL) //SSD1306  display
#define display_pin2 5  // (OPTIONAL) //SSD1306  display

/*
    receiver_pin: Подключите к D3 (GPIO 0)
    transmiter_pin: Подключите к D6 (GPIO 12)
    trigger_pin: Подключите к D4 (GPIO 2)
    buzzer_pin: Подключите к D7 (GPIO 13)
    red_pin: Подключите к D5 (GPIO 14)
    blue_pin: Подключите к D0 (GPIO 16)
    display_pin1: Подключите к D2 (GPIO 4)
    display_pin2: Подключите к D1 (GPIO 5)
*/



SSD1306Wire display(0x3c, display_pin1, display_pin2, GEOMETRY_128_64);

IRrecv irrecv(receiver_pin);
decode_results results;

IRsend irsend(transmiter_pin);

//player details
int player_ID = 8;
int game_mode = 0;
int team_ID = player_ID / 7;
int score = 0;

//variables
int max_hp = 100;
int max_ammo = 50;
int hp = max_hp;      //hp indicates life of player at current time
int ammo = max_ammo;  //ammo indicates player bullets at current time
int last_hp = hp - 1;
int last_ammo = ammo - 1;
boolean control_fire = 1;
//if control_fire = 1 then the gun sends pulses only once when button is pressed
//if control_fire = 0 then the gun sends pulses as long as butto is pressed...continuous fire :)
boolean lastbutton_state = HIGH;
boolean button_state = HIGH;
boolean new_game = true;  //To reset EEPROM data and start a new game
int hit_damage = 5;       //hp will reduce by this amount when got hit
int EEPROM_address = 0;   //EEPROM base address
uint8_t hit_IP = 0;       //IP address of player who made hit
unsigned long timeout = 30000;
byte upgrade = 0;


unsigned long int update_db = 0;
int update_db_interval = 5000;  //update database of website every 1000 milliseconds
String weapoint = "Gun";
int reset = 0, prev_reset = 0;
int port = 80;                                //default port
String response;

//session variables
int time_minutes = 5;                                 //Game wil get reset after 5 minutes...i.e., session over
unsigned long time_limit = time_minutes * 60 * 1000;  //converting session time into milliseconds
unsigned long present_ms = 0, time_ms = 0;

HTTPClient http;  //http client object to communicate with website

void setup() {
  pinMode(trigger_pin, INPUT_PULLUP); 
  pinMode(buzzer_pin, OUTPUT);
  pinMode(red_pin, OUTPUT);
  pinMode(blue_pin, OUTPUT);

  //Serial.begin(115200);
  //connecting to WiFi or Hotspot
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, ssidPass);

  display.init();
  display.flipScreenVertically();

  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Welcome...");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 15, "Try connect to...");
  display.drawString(0, 42, "WiFi: " + ssid);
  display.drawString(0, 52, "Password: " + ssidPass);
  display.display();

  present_ms = millis();

  int tries = 10;
  while (--tries && WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient client;
    http.begin(client, "http://www.onclick.lv/esp/?mac=" + WiFi.macAddress() + "&ip=" + WiFi.localIP().toString());
    int httpCode = http.GET();
    http.end();

    req_server();  //fetch player details
  }



  //connection successfull
  /*
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP by my mobile/router
*/
  /*
  //EEPROM
  EEPROM.begin(4);
  //hp = EEPROM.read(0);
  //ammo = EEPROM.read(1);
  EEPROM.write(0, hp);
  EEPROM.commit();
  EEPROM.write(1, ammo);
  EEPROM.commit();
  //assigning 4 bytes of memory in EEPROM
*/


  //Setting up OLED display I2C protocol.
  //SCL--->D5
  //SDA--->D3
  //initialising display



  if (digitalRead(trigger_pin) == LOW && WiFi.status() == WL_CONNECTED) {
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Upgrade...");
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 15, "You software: v" + (String)VERSION);
    display.drawString(0, 32, "Please wait...");
    display.drawProgressBar(0, 45, 126, 10, 60);
    display.display();
    WiFiClient client;
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, "http://www.onclick.lv/lasertag/laserTag.ino.bin");
  } else {
    display_hp_ammo();
    time_ms = millis();  //taking the elapsed milliseconds in to account :)
    //IR rx and tx
    irsend.begin();
    irrecv.enableIRIn();  // Start the receiver
  }
}


void loop() {

  present_ms = millis();

  /*
  if (reset != 0 && prev_reset == 0) {
    new_game = true;
    time_ms = present_ms;
  } else {
    new_game = false;
  }

  prev_reset = reset;
  if (new_game == true) {
    //reset hp and ammo to their previous values
    hp = max_hp;
    ammo = max_ammo;
    new_game = false;
    update_EE();
  } else {
    hp = EEPROM.read(0);
    ammo = EEPROM.read(1);
  }
*/

  if (present_ms - time_ms < time_limit) {
    if (present_ms - update_db > update_db_interval) {
      display_hp_ammo();
      req_server();
      update_db = present_ms;
    }

    tx_rx_check();  //Checking for trigger press(tx) and IR signals(rx)

    if (last_hp != hp || last_ammo != ammo && hp > 0 && ammo > 0) {
      display_hp_ammo();
      last_hp = hp;
      last_ammo = ammo;
    }
  } else {
    display_game_over();
    //req_server();
    delay(100);
  }
}

/*
void update_EE() {
  EEPROM.write(0, hp);
  EEPROM.commit();
  EEPROM.write(1, ammo);
  EEPROM.commit();
}
*/

void player_dead() {
  tone(buzzer_pin, 50);
  digitalWrite(red_pin, HIGH);
  display_player_dead();
  delay(5000);
  noTone(buzzer_pin);
  digitalWrite(red_pin, LOW);
  ammo = max_ammo;
  hp = max_hp;
}

void no_ammo() {
  tone(buzzer_pin, 50);
  digitalWrite(red_pin, HIGH);
  digitalWrite(blue_pin, HIGH);
  display_ammo_over();
  delay(100);
  noTone(buzzer_pin);
  delay(5000);
  digitalWrite(red_pin, LOW);
  digitalWrite(blue_pin, LOW);
  ammo = max_ammo;
}

void player_hit() {
  if (hp <= 0) {
    player_dead();
  } else {
    hp = hp - hit_damage;
    //update_EE();
  }
}


void got_hit(uint16_t data) {
  display.invertDisplay();
  tone(buzzer_pin, 100);
  digitalWrite(blue_pin, HIGH);
  delay(100);
  noTone(buzzer_pin);
  digitalWrite(blue_pin, LOW);
  player_hit();
  display.normalDisplay();
}

void tx_rx_check() {
  // If button pressed, send the code.
  button_state = digitalRead(trigger_pin);
  if (lastbutton_state == LOW && button_state == HIGH) {
    Serial.println("Trigger Released");
    irrecv.enableIRIn();  // Re-enable receiver
  }
  if (control_fire == 1) {
    if (button_state == LOW && lastbutton_state == HIGH) {
      delay(5);  //debounce the button
      button_state = digitalRead(trigger_pin);
      if (button_state == LOW && lastbutton_state == HIGH) {
        //trigger pressed
        if (ammo > 0 && hp > 0) {
          ammo--;
    if (weapoint == "Gun") {
      irsend.sendNEC(0x7100, 16);
      max_ammo = 50;
    }
    if (weapoint == "Rifle") {
      irsend.sendNEC(0x7200, 16);
      max_ammo = 10;
    }
    if (weapoint == "Shotgun") {
      irsend.sendNEC(0x7300, 16);
      max_ammo = 25;
    }
    if (ammo > max_ammo) {
      ammo = max_ammo;
    }
        //  irsend.sendNEC(0x7100, 16);
          //Serial.println("Pressed, sending");
          //Serial.println(String(0x1100, HEX));
          delay(50);  // Wait a bit between retransmissions
        } else if (ammo <= 0) {
          no_ammo();
        } else {
          player_dead();
        }
      }
    } else if (irrecv.decode(&results)) {
      //Serial.println((uint16_t)results.value, HEX);
      if ((uint16_t)results.value == 0x7100) {
        hit_damage = 5;
        got_hit((uint16_t)results.value);
      } else if ((uint16_t)results.value == 0x7300) {
        hit_damage = 10;
        got_hit((uint16_t)results.value);
      } else if ((uint16_t)results.value == 0x7200) {
        hit_damage = 15;
        got_hit((uint16_t)results.value);
      } else {
        /*
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 10, "Debug!");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 40, (String)results.value);
  display.display();
  */
      }
      irrecv.resume();  // resume receiver
    }
    lastbutton_state = button_state;
  } else {
    if (button_state == LOW) {
      delay(5);  //debounce the button
      button_state = digitalRead(trigger_pin);
      if (button_state == LOW) {
        //trigger pressed
        ammo--;
        //Serial.println("Pressed, sending");
        irsend.sendNEC(0x1100, 16);
        delay(50);  // Wait a bit between retransmissions
      }
    } else if (irrecv.decode(&results)) {
      // if (results.value == 0x7100)
      //{
      //Serial.println((uint16_t)results.value, HEX);
      got_hit(results.value);
      // }
      irrecv.resume();  // resume receiver
    }
    lastbutton_state = button_state;
  }
}

void display_hp_ammo() {
  int disp_hp = map(hp, 0, max_hp, 0, 100);
  int disp_ammo = map(ammo, 0, max_ammo, 0, 100);
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Arms: " + String(weapoint));
  display.drawString(102, 28, String(hp));
  display.drawString(102, 49, String(ammo));
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 22, "Life:");
  display.drawProgressBar(0, 34, 100, 8, disp_hp);
  display.drawString(0, 44, "Ammo:");
  display.drawProgressBar(0, 55, 100, 8, disp_ammo);
  display.display();
}

void display_player_dead() {
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 10, "GAME OVER!");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 40, "PLAYER DEAD!!!");
  display.display();
}

void display_game_over() {
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 10, "Time Up!");
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 40, "GAME OWER!");
  display.display();
}

void display_ammo_over() {
  int disp_hp = map(hp, 0, max_hp, 0, 100);
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 10, "Reloading ammo.");
  display.drawString(102, 49, String(hp));
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 25, "PLEASE WAIT...");
  display.drawString(0, 44, "Life:");
  display.drawProgressBar(0, 55, 100, 8, disp_hp);
  display.display();
}


void req_server() {
  if (WiFi.status() == WL_CONNECTED) {
    String url = "http://www.onclick.lv/lasertag/send.php?weapoint=";
    url += String(weapoint);
    url += "&hp=";
    url += String(hp);
    url += "&ammo=";
    url += String(ammo);
    url += "&time=";
    url += String(present_ms - time_ms);
    url += "&mac=";
    url += String(WiFi.macAddress());
    url += "&ip=";
    url += WiFi.localIP().toString();

    HTTPClient http;
    WiFiClient client;
    http.begin(client, url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      weapoint = http.getString(); // Get response content as a string
    }
    http.end();
  }
}