/* Run TLC9711 over SPI on PI PICO
*  using inline code 
*  (Based on the Adafruit TLC59711 PWM/LED driver library, authors: Limor Fried & Ladyada).
*  MRD 19/03/2023 ver 01
*/

//#include <SPI.h>
#include <SPI.h>
#include <Arduino.h>

// TLC9711 variables and constants
#define DATAIN 16          // not used by TLC59711 breakout
#define CHIPSELECT 17      // not used by TLC59711 breakout
#define SPICLOCK 18        // ci on TLC59711 breakout
#define DATAOUT 19         // di on TLC59711 breakout
#define SPI_FREQ 14000000  // SPI frequency 
#define LED_BRIGHT 0x7F
#define LED_DARK 0x00
#define PWM_100 65535
#define PWM_0 0

int8_t numdrivers = 1;  // number of chained breakout boards
uint8_t BCr = LED_BRIGHT;
uint8_t BCg = LED_BRIGHT;
uint8_t BCb = LED_BRIGHT;
long *pwmbuffer = (long *)calloc(2, 12 * numdrivers);

// TLC9711 functions
void setupSPI() {
  pinMode(SPICLOCK, OUTPUT);
  pinMode(DATAOUT, OUTPUT);
  SPI.setSCK(SPICLOCK);
  SPI.setTX(DATAOUT);
  delay(5);
  SPI.begin();
  delay(5);
}

void tlcWrite() {
  uint32_t command;

  // Magic word for write
  command = 0x25;

  command <<= 5;
  // OUTTMG = 1, EXTGCK = 0, TMGRST = 1, DSPRPT = 1, BLANK = 0 -> 0x16
  command |= 0x16;

  command <<= 7;
  command |= BCr;

  command <<= 7;
  command |= BCg;

  command <<= 7;
  command |= BCb;

  SPI.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE0));
  for (uint8_t n = 0; n < numdrivers; n++) {
    SPI.transfer(command >> 24);
    SPI.transfer(command >> 16);
    SPI.transfer(command >> 8);
    SPI.transfer(command);

    // 12 channels per TLC59711
    for (int8_t c = 11; c >= 0; c--) {
      // 16 bits per channel, send MSB first
      SPI.transfer(pwmbuffer[n * 12 + c] >> 8);
      SPI.transfer(pwmbuffer[n * 12 + c]);
    }
  }

  delayMicroseconds(200);
  SPI.endTransaction();
}

//void tlcSetPWM (uint16_t chan, uint16_t pwm) {
void tlcSetPWM(int chan, long pwm) {
  if (channelInRange(chan)) {
    pwmbuffer[chan] = pwm;
  }
}

void fadeUp(uint16_t ramp) {
  long pwm = 0;
  while (pwm < 65535) {
    for (int chan = 0; chan < 12 * numdrivers; chan++) {
      tlcSetPWM(chan, pwm);
    }
    tlcWrite();
    delay(10);
    pwm = pwm + ramp;
  }
}

void fadeUpChannel(int channel, uint16_t ramp) {
  long pwm = 0;
  while (pwm < 65535) {
    tlcSetPWM(channel, pwm);
    tlcWrite();
    delay(10);
    pwm = pwm + ramp;
  }
}

void fadeUpExcept(int except, uint16_t ramp) {
  if (channelInRange(except)) {
    long pwm = 0;
    while (pwm < 65535) {
      for (int chan = 0; chan < 12 * numdrivers; chan++) {
        if (chan != except) {
          tlcSetPWM(chan, pwm);
        }
      }
      tlcWrite();
      delay(10);
      pwm = pwm + ramp;
    }
  }
}

void fadeDown(uint16_t ramp) {
  long pwm = 65534;
  while (pwm > 0) {
    for (int chan = 0; chan < 12 * numdrivers; chan++) {
      tlcSetPWM(chan, pwm);
    }
    tlcWrite();
    delay(10);
    pwm = pwm - ramp;
  }
}

void fadeDownExcept(int except, uint16_t ramp) {
  if (channelInRange(except)) {
    long pwm = 65534;
    while (pwm > 0) {
      for (int chan = 0; chan < 12 * numdrivers; chan++) {
        if (chan != except) {
          tlcSetPWM(chan, pwm);
        }
      }
      tlcWrite();
      delay(10);
      pwm = pwm - ramp;
    }
  }
}

bool channelInRange(int chan) {
  return (chan < 12 * numdrivers);
}

// Turn all off/on
void setAll(long pwm, long wait) {
  for (uint16_t chan = 0; chan < 12 * numdrivers; chan++) {
    tlcSetPWM(chan, pwm);
  }
  tlcWrite();
  delay(wait);
}

void setAllExcept(int except, long pwm, long wait) {
  if (channelInRange(except)) {
    for (uint16_t chan = 0; chan < 12 * numdrivers; chan++) {
      if (chan != except) {
        tlcSetPWM(chan, pwm);
      }
    }
    tlcWrite();
    delay(wait);
  }
}

// main
int exceptChannel = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  Serial.println("TLC59711 test");
  setupSPI();
  tlcWrite();
}

void loop() {   
  digitalWrite(LED_BUILTIN, HIGH);
  fadeUp(100);
  setAll(PWM_100, 1000);
  fadeDownExcept(exceptChannel, 100);
  setAllExcept(exceptChannel, PWM_0, 1000);
  delay(3000);
  fadeUpExcept(exceptChannel, 100);
  digitalWrite(LED_BUILTIN, LOW);
  fadeDown(100);
  setAll(PWM_0, 1000);
  Serial.println("cycle");
  if(!channelInRange(++exceptChannel)){
    exceptChannel = 0;
  }
  delay(1000);
}
