#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <Adafruit_NeoPixel.h>
#include <TinyWireS.h>
//some stolen constants defines for usage - http://www.technoblogy.com/show?KX0
#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC (before power-off)
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC
//lets set some pins
#define pressPin 1
#define MOSFETpin 3
#define PixelPin 4
#define bright 25
#define I2C_SLAVE_ADDRESS 0xE3//this is hex for 227, which is the ascii code for pi :P

int pressCount,colourCount,voltage;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, PixelPin, NEO_GRB + NEO_KHZ800);


void setup() {
  //init the i2c
  TinyWireS.begin(I2C_SLAVE_ADDRESS); 
  TinyWireS.onReceive(receiveEvent); 
  TinyWireS.onRequest(requestEvent);
  //init the neopixel
  pixels.begin();
  pixels.setBrightness(25);
  pixels.show();
  pinMode(pressPin, INPUT_PULLUP);
  pinMode(MOSFETpin,OUTPUT);
  digitalWrite(MOSFETpin,HIGH);//make sure we are off
  colourCount=0;
  sleep();
}

void loop() {
  //digitalWrite(MOSFETpin,digitalRead(pressPin));
  pixels.setPixelColor(0,getColour());
  if(digitalRead(pressPin)==LOW){
      pressCount++;
      pixels.setBrightness(map(pressCount,0,3000,bright,0));//bright/2);// - ((bright/3000)*pressCount));
  }
  else{
    pressCount=0;
    pixels.setBrightness(bright);
  }
  //show the battery status colour
  pixels.show();
  if (pressCount > 3000){
    pixels.setPixelColor(0,0,0,0);
    pixels.show();
    delay(2000);
    sleep();
  }
}

uint32_t getColour(){
  int iTemp;
  //get the average of 5 reads to increas stability
  iTemp = readVcc()+readVcc()+readVcc()+readVcc()+readVcc();
  iTemp = iTemp/5;//get the average
  if(iTemp > 4200)//we are over 4.2v - bad things must have happened, just revert to full
    voltage = 255;
  else if(voltage < 3300)//revert anything below 3.3v to empty
    voltage = 0;
  else//anything else is fair game, lets report it
    voltage = map(iTemp,3300,4200,0,255);
  
  return pixels.Color(255-voltage,voltage,0);
}

long readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif 

  //delay(3); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1125300L / result; // Back-calculate AVcc in mV
  return result;
}

//sleep code 'borrowed' from 
//https://bigdanzblog.wordpress.com/2014/08/10/attiny85-wake-from-sleep-on-pin-state-change-code-example/
void sleep() {
    //make sure the FET is off
    digitalWrite(MOSFETpin,HIGH);
    //and set the neopixel off so we save juice
    pixels.setPixelColor(0,0,0,0);
    pixels.show();
    GIMSK |= _BV(PCIE);                     // Enable Pin Change Interrupts
    PCMSK |= _BV(PCINT1);                   // Use PB1 as interrupt pin
    ADCSRA &= ~_BV(ADEN);                   // ADC off
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);    // replaces above statement

    sleep_enable();                         // Sets the Sleep Enable bit in the MCUCR Register (SE BIT)
    sei();                                  // Enable interrupts
    sleep_cpu();                            // sleep

    cli();                                  // Disable interrupts
    PCMSK &= ~_BV(PCINT3);                  // Turn off PB3 as interrupt pin
    sleep_disable();                        // Clear SE bit
    ADCSRA |= _BV(ADEN);                    // ADC on

    //sei();                                  // Enable interrupts

    //lets wait for debouncing
    for(int i=0;i<5;i++){
      //if(i%2==0){
      for(int j=0;j<bright;j++){
        pixels.setPixelColor(0,getColour());
        pixels.setBrightness(j);
        pixels.show();
        delay(10);
      }
      for(int j=bright;j>0;j--){
        pixels.setPixelColor(0,getColour());
        pixels.setBrightness(j);
        pixels.show();
        delay(10);
      }
    }
    //reset the brightness
    pixels.setBrightness(bright);
    //see if the button is still down
    if(digitalRead(pressPin)!=LOW){
      //the button is not still down, lets sleep again
      sleep();
    } 
    else{
      //button was down. lets wait a moment so the button gets released
      pixels.setPixelColor(0,getColour());
      pixels.show();
      delay(2000); 
      //make sure the FET back on
      digitalWrite(MOSFETpin,LOW);    
    }
} // sleep

//using code from here https://thewanderingengineer.com/2014/02/17/attiny-i2c-slave/
// Gets called when the ATtiny receives an i2c request
void requestEvent(){
  //return the voltage to whatever asked for it
  int iTemp;
  //get the average of 5 reads to increas stability
  iTemp = readVcc()+readVcc()+readVcc()+readVcc()+readVcc();
  iTemp = iTemp/5;//get the average
  iTemp = iTemp/100;//if out milivolts reports 4000, convert it to 40, so it can become 4.0v later
  TinyWireS.send(iTemp);
}

void receiveEvent(){
  //return the voltage to whatever asked for it
  int iTemp;
  //get the average of 5 reads to increas stability
  iTemp = readVcc()+readVcc()+readVcc()+readVcc()+readVcc();
  iTemp = iTemp/5;//get the average
  iTemp = iTemp/100;//if out milivolts reports 4000, convert it to 40, so it can become 4.0v later
  TinyWireS.send(iTemp);
}

ISR(PCINT0_vect) {
    // This is called when the interrupt occurs, but I don't need to do anything in it
                               
    
}

