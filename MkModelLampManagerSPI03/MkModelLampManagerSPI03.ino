
/* Run TLC9711 over SPI on PI PICO
*  using inline code 
*  (Based on the Adafruit TLC59711 PWM/LED driver library, authors: Limor Fried & Ladyada).
*  MRD 19/03/2023 ver 01
*/

//#include <SPI.h>
#include <SPI.h>
#include <Arduino.h>

// General constants
#define HIGHLIGHT_TIMEOUT 60000

// TLC9711 variables and constants
#define DATAIN 16          // not used by TLC59711 breakout
#define CHIPSELECT 17      // not used by TLC59711 breakout
#define SPICLOCK 18        // ci on TLC59711 breakout
#define DATAOUT 19         // di on TLC59711 breakout
#define SPI_FREQ 14000000  // SPI frequency 
#define LED_BRIGHT 0x7F
#define RAMP_UP 255
#define RAMP_DOWN -255
#define MIN_PWM 0
#define MAX_PWM 65535

const int8_t numdrivers = 1;  // number of chained breakout boards
const int8_t numChannels = 12 * numdrivers;
uint8_t BCr = LED_BRIGHT;
uint8_t BCg = LED_BRIGHT;
uint8_t BCb = LED_BRIGHT;
// Array of struct to hold current pwm and ramp direction for each channel
struct channelState {
  long pwm;
  int ramp;
} channelStates[numChannels];

// TLC9711 functions
// Initialise SPI for PI PICO
void setupSPI() {
  pinMode(SPICLOCK, OUTPUT);
  pinMode(DATAOUT, OUTPUT);
  SPI.setSCK(SPICLOCK);
  SPI.setTX(DATAOUT);
  delay(5);
  SPI.begin();
  delay(5);
}

// Write channel pwm info to TLC59711
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
      SPI.transfer(channelStates[n * 12 + c].pwm >> 8);
      SPI.transfer(channelStates[n * 12 + c].pwm);
    }
  }

  delayMicroseconds(200);
  SPI.endTransaction();
}

// Set all channels to same pwm and ramp setting. 
void setAllChannels(long pwm, int ramp) {
  for(int channel = 0; channel < numChannels; channel++){
    channelStates[channel].pwm = pwm;
    channelStates[channel].ramp = ramp;  
  }
}

// Fade lamps according to setting for each channel in the ramp buffer.
void fadeLamps() {
  for(int iter = 0; iter < 258; iter++) {
    tlcWrite();
    for(int channel = 0; channel < numChannels; channel++){
        channelStates[channel].pwm = calculateNewPwm(channelStates[channel].pwm,channelStates[channel].ramp); 
    }
    delay(10);
  }
}

// Identify a single channel to be faded up, mark others to be faded down. 
void highlightLamp(uint32_t highlitChannel){
  for(int channel = 0; channel < numChannels; channel++){
    channelStates[channel].ramp = RAMP_DOWN;        
  }
  channelStates[highlitChannel].ramp = RAMP_UP;  
}

// Calculate new pwm value based on current pwm and ramp values.
long calculateNewPwm(long pwm, int ramp){
  long newPwm = pwm + ramp;
  if(newPwm < MIN_PWM) {
    newPwm = MIN_PWM;
  }
  else if(newPwm > MAX_PWM){
    newPwm = MAX_PWM;    
  }
  return newPwm;
}

// Test that a channel number is within range of available channels.
bool channelInRange(int chan) {
  return (chan > -1 && chan < numChannels);
}

// Print channle states
void printChannelStates() {
  for(int channel = 0; channel < numChannels; channel++){
    Serial.print(channel);
    Serial.print("   ");
    Serial.print(channelStates[channel].pwm);
    Serial.print("   ");
    Serial.println(channelStates[channel].ramp); 
  }
}

int highlight = 0;

void setup() {
  Serial.begin(9600);
  while(!Serial){}
  delay(100);
}

void loop() {
  uint32_t channelIn = 99;
  while(Serial.available()) {       
    channelIn = Serial.read() - 'A';
    if(channelInRange(channelIn)) {
      //Serial.printf("Sending to C2\n");
      rp2040.fifo.push_nb(channelIn);
    } 
  }
  delay(10);
}

void setup1() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  setupSPI();
  delay(100);
  setAllChannels(MIN_PWM, RAMP_UP);
  fadeLamps();
}

void loop1() {
  static long lastHighlightTS = 0;

  if(rp2040.fifo.available() > 0){
    uint32_t highlight = rp2040.fifo.pop();
    highlightLamp(highlight);
    fadeLamps();
    lastHighlightTS = millis();
  }
  else if(lastHighlightTS > 0 && millis() > (lastHighlightTS + HIGHLIGHT_TIMEOUT)) {
      setAllChannels(MIN_PWM, RAMP_UP);
      fadeLamps();
      lastHighlightTS = 0;
  }
  delay(20);
}
