#include <U8x8lib.h>

#include <stdio.h>
#include <stdlib.h>

// Deep Sleep
#define uS_TO_S_FACTOR 1000000  //Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  5        //Time ESP32 will go to sleep (in seconds)

//LoRaLayer2
#include <Layer1_LoRa.h>
#include <LoRaLayer2.h>
#define LL2_DEBUG

// Sensors
#include <Adafruit_Sensor.h>
#include <DHT.h>
#define DHTPIN 13
#define DHTTYPE DHT11

// LCD
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,20,4);

// the OLED used
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);

int counter = 0;

#define LORA_CS 18
#define LORA_RST 14
#define LORA_IRQ 26
#define LORA_FREQ 915E6
#define LED 25

#define ENABLE_DEBUG 

char LOCAL_ADDRESS[9] = "fffff00d";
char GATEWAY[9] = "fffff00c";

Layer1Class *Layer1;
LL2Class *LL2;
int txPower = 20;

//DHT
DHT dht(DHTPIN, DHTTYPE);


//Function that prints the reason by which ESP32 has been awaken from sleep
void print_wakeup_reason(){
	esp_sleep_wakeup_cause_t wakeup_reason;
	wakeup_reason = esp_sleep_get_wakeup_cause();
	switch(wakeup_reason)
	{
		case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
		case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
		case 3  : Serial.println("Wakeup caused by timer"); break;
		case 4  : Serial.println("Wakeup caused by touchpad"); break;
		case 5  : Serial.println("Wakeup caused by ULP program"); break;
		default : Serial.println("Wakeup was not caused by deep sleep"); break;
	}
}

void setup() {
  print_wakeup_reason();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  //LCD
  lcd.init();                      // initialize the lcd 
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(3,0);
  lcd.print("Hello, world!");


  SPI.begin(5, 19, 27, 18);
  //DHT
  dht.begin();

  Serial.begin(9600);
  while (!Serial);

  #ifdef ENABLE_DEBUG
  // LED Setup
	pinMode(LED,OUTPUT);
  digitalWrite(LED,LOW); 

  Serial.println("* Initializing LoRa...");
  Serial.println("LoRa Sender");
  #endif

  u8x8.begin();
  u8x8.setPowerSave(0);
  //u8x8.setFont(u8x8_font_chroma48medium8_r);
  //u8x8.setFont(u8x8_font_artosserif8_r);
  u8x8.setFont(u8x8_font_5x7_f);
  u8x8.drawString(0, 0, "LoRa Sender...");
  u8x8.drawString(0, 1, GATEWAY);
  u8x8.drawString(0, 2, LOCAL_ADDRESS);
  delay(500);

  Layer1 = new Layer1Class();
  Layer1->setPins(LORA_CS, LORA_RST, LORA_IRQ);
  Layer1->setTxPower(txPower);
  Layer1->setLoRaFrequency(LORA_FREQ);
  if (Layer1->init())
  {
    Serial.println(" --> LoRa initialized");
    LL2 = new LL2Class(Layer1); // initialize Layer2
    LL2->setLocalAddress(LOCAL_ADDRESS); // this should either be randomized or set using the wifi mac address
    LL2->setInterval(10000); // set to zero to disable routing packets
  }
  else
  {
    Serial.println(" --> Failed to initialize LoRa");
  }
}

struct Datagram buildDatagram(uint8_t destination[ADDR_LENGTH], uint8_t type, uint8_t* data, size_t len)
{
    struct Datagram datagram;
    memcpy(datagram.destination, destination, ADDR_LENGTH);
    datagram.type = 'c';
    memcpy(datagram.message, data, len);
    return datagram;
}

void loop() {
  LL2->daemon();
  char data[128] = {0};
  char counter_str[128] = {0};
  int msglen = 0;
  int packetsize = 0;
  sprintf(counter_str, "%i", counter);

  u8x8.drawString(0, 3, "Packets: ");
  u8x8.drawString(9, 3, counter_str);

  #ifdef ENABLE_DEBUG
  digitalWrite(LED, HIGH);
  #endif

  delay(1000);

  #ifdef ENABLE_DEBUG
  digitalWrite(LED, LOW);
  #endif

  float temperature = dht.readTemperature(true);
  float humidity = dht.readHumidity();

  // Build Packet
  msglen = sprintf(data, "%f,%f", temperature, humidity);
  #ifdef ENABLE_DEBUG
  Serial.println(data);
  #endif

  struct Datagram packet = buildDatagram((uint8_t *)GATEWAY, 'c', (uint8_t *)data, msglen);
  packetsize = msglen + HEADER_LENGTH;

  // Send packet
  LL2->writeData(packet, packetsize);
  counter++;

  // After processing, go to sleep
  #ifdef ENABLE_DEBUG
  Serial.println("Going to sleep...");
  #endif

  char routes[128];
  char neighbors[128];
  char config[128];

  LL2->getRoutingTable(routes);
  Serial.println(routes);

  LL2->getNeighborTable(neighbors);
  Serial.println(neighbors);

  delay(1000);

  //esp_deep_sleep_start();
}
