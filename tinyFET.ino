#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <Adafruit_NeoPixel.h>
//some stolen constants defines for usage - http://www.technoblogy.com/show?KX0
#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC (before power-off)
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC
//lets set some pins
#define pressPin 1
#define MOSFETpin 3
#define PixelPin 4
#define bright 25

int pressCount,colourCount,voltage;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, PixelPin, NEO_GRB + NEO_KHZ800);


void setup() {
  //init the neopixel
  pixels.begin();
  pixels.setBrightness(25);
  pixels.show();
  pinMode(pressPin, INPUT_PULLUP);
  pinMode(MOSFETpin,OUTPUT);
  digitalWrite(MOSFETpin,LOW);//make sure we are off
  colourCount=0;
  sleep();
}

void loop() {
  //digitalWrite(MOSFETpin,digitalRead(pressPin));
  if(digitalRead(pressPin)==LOW){
      pressCount++;
  }
  else{
    pressCount=0;
  }
  //show the battery status colour
  pixels.setPixelColor(0,getColour());
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
  //only read the voltage once every 10000 attempts
  //if(colourCount>10000){
    iTemp = readVcc()+readVcc()+readVcc()+readVcc()+readVcc();
    iTemp = iTemp/5;//get the average
    if(iTemp > 4200)
      voltage = 255;
    else if(voltage < 3300)
      voltage = 0;
    voltage = map(iTemp,3300,4200,0,255);
    //colourCount=0;
  //}
  //else{
  //  colourCount++;
  //}
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
    digitalWrite(MOSFETpin,LOW);
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
    for(int i=0;i<10;i++){
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

ISR(PCINT0_vect) {
    // This is called when the interrupt occurs, but I don't need to do anything in it
                               
    
}

