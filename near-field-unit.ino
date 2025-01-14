#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <EEPROM.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

#define CE A3
#define CSN A2
#define LED 2
#define button 3
#define radioPower 9

#define UNIT_ID "UI7PCAHD"
char BROADCAST_ID[32] = "";

volatile bool buttonFlag = false;
volatile uint8_t buttonCount = 0;

RF24 radio(CE, CSN); // CE, CSN

byte address[6];

// WDT entry Flag
volatile uint8_t f_wdt=1;

void blink(uint8_t pin){
  digitalWrite(pin, HIGH);
  delay(10);
  digitalWrite(pin, LOW);
}

void blink(uint8_t pin, uint8_t times) {
for(int i=0;i<times;i++){
    digitalWrite(pin, HIGH);
    delay(10);
    digitalWrite(pin, LOW);
    delay(300);
  }
}

bool blinkRepeat = true;
void blink(uint8_t pin, uint8_t times, bool repeat){
  if(repeat){
    blink(pin,times);
  } else {
    if(blinkRepeat){
      blinkRepeat = false;
      blink(pin,times);
    }
  }
  
}

bool inpair = false;
bool s = false;

void turnOnRadio(bool s){
  pinMode(radioPower, OUTPUT);
  s?digitalWrite(radioPower,HIGH):digitalWrite(radioPower,LOW);
  delay(1);
}

uint32_t timme = 0;
void pairMode(){
  if(!inpair){
  inpair = true;
  turnOnRadio(true);
    delay(1);

    if (!radio.begin()) {
    // DEBUG.println(F("radio hardware is not responding!!"));
    while (1) {}  // hold in infinite loop
    }
  //   Serial.println("Radio begin");
  // Serial.println("Pairing Mode");
  timme = millis();
  const byte addr[6] = "UI7PC";
  radio.openReadingPipe(0,addr);
  radio.openWritingPipe(addr);
  radio.stopListening();
  }

  if(!s){
  const char id[] = UNIT_ID; 
  delay(3000);
  s = radio.write(&id, sizeof(id));
  }
  else
  {
    // Serial.println("Waiting for Broadcast ID");
    radio.startListening();
    if (radio.available()) {
    char text[32] = "";
    radio.read(&text, sizeof(text));
    // Serial.print("BROADCAST ID received: ");
    //   Serial.println(text);
      strcpy(BROADCAST_ID,text);
      EEPROM.put(0,text);

      /********************** */
      char newAddr[6];
      for(int i=0;i<5;i++){
        address[i] = text[i];
        newAddr[i] = text[i];
      }  
      // Serial.print("New radio Adrress: ");
      // Serial.println(newAddr);  

      buttonCount = 0;
      inpair = false;
      blink(LED,3);
      f_wdt = 1;
  }
}
}


void button_ISR(){
  buttonFlag = true;
}

ISR(WDT_vect)
{
	if(f_wdt == 0)
	{
    // wdt_disable();
		f_wdt=1; // Reset the Flag
	}
}

//////////////////////////////////////////////////////////////////////////
// Sleep Configuration Function
//   Also wake-up after

void enterSleep(void)
{
	WDTCSR |= _BV(WDIE); // Enable the WatchDog before we initiate sleep

	//set_sleep_mode(SLEEP_MODE_PWR_SAVE);		/* Some power Saving */
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);		/* Even more Power Savings */
	sleep_enable();

	/* Now enter sleep mode. */
  noInterrupts();
	sleep_enable();
	
  // turn off brown-out enable in software
  MCUCR = bit (BODS) | bit (BODSE);
  MCUCR = bit (BODS); 
  interrupts ();             // guarantees next instruction executed
  power_all_disable();
  
  sleep_cpu ();              // sleep within 3 clock cycles of above
	
  
  
  /* The program will continue from here after the WDT timeout*/
	sleep_disable(); /* First thing to do is disable sleep. */

	/* Re-enable the peripherals. */
	power_all_enable();
}

void setup() {
  
  ADCSRA = 0;
  power_adc_disable();

  // Serial.begin(9600);
  delay(100);

  pinMode(LED, OUTPUT);
  pinMode(button, INPUT_PULLUP);
  pinMode(radioPower, OUTPUT);

  digitalWrite(LED, LOW);
  turnOnRadio(true);

  // Serial.println("Unit Started");
  
  attachInterrupt(digitalPinToInterrupt(button),button_ISR,FALLING);
  
  EEPROM.get(0, BROADCAST_ID);
  // Serial.print("BROADCAST ID SAVED: ");
  // Serial.println(BROADCAST_ID);
  
  for(int i=0;i<5;i++){
    address[i] = BROADCAST_ID[i];
    // Serial.print(char(address[i]));
  }
  // Serial.println();
  delay(10);
 }

uint32_t currentTime = 0;

void loop() {
  /*** Setup the WDT ***/
	cli();
	/* Clear the reset flag. */
	MCUSR = 0;
  WDTCSR = bit(WDCE) | bit(WDE);

	/* set new watchdog timeout prescaler value */
	// WDTCSR = 1<<WDP1 | 1<<WDP2;             /* 1.0 seconds */
  //WDTCSR = 1<<WDP0 | 1<<WDP1 | 1<<WDP2; /* 2.0 seconds */
  WDTCSR = 1<<WDP3;                     /* 4.0 seconds */
  sei();


  if(buttonFlag){
    detachInterrupt(button);
    // Serial.println("Awoken by Button");
    delay(300);
    uint32_t lastTime = millis();
    while(!digitalRead(button)){
      uint32_t t = millis() - lastTime;
      if((t >= 2900) && (t<3000)) blink(LED);
      if(t >= 6000) blink(LED,3);

      if((millis()-lastTime)>8000){
        break;
      }
    }
    buttonCount = (millis() - lastTime)/1000;
    currentTime = lastTime;
    buttonFlag = false;
    timme = millis();
    delay(500);
    attachInterrupt(digitalPinToInterrupt(button),button_ISR,FALLING);
  
  }

  if(buttonCount >= 3 && buttonCount <5){
    
    pairMode();

    if(timeout(60000, timme)){
      inpair = false;
      buttonCount = 0;
      // Serial.println("Going to sleep");
      turnOnRadio(false);
      _delay(10);
      enterSleep();
    }

  } 
  
  if(buttonCount >=7){
    int l = EEPROM.length();
    // Serial.println("Clearing storage");
    for(int i=0;i<l;i++){
      EEPROM.write(i,0);
    }
    // blink(LED,5);
    inpair = false;
    buttonCount = 0;
  }

    if(f_wdt == 1) {
    
    // Serial.println("Awaken by timer");
    if(address[0]!=0) {
    turnOnRadio(true);
    _delay(100);
    if (!radio.begin()) {
    // Serial.println(F("radio hardware is not responding!!"));
    while (1) {}  // hold in infinite loop
  }

  radio.setPALevel(RF24_PA_HIGH);
  
  radio.openWritingPipe(address);
 
      char text[32]="";
      strcat(text, BROADCAST_ID);
      strcat(text, UNIT_ID);
      // Serial.println(text);
      
      
      radio.stopListening();
      delay(2);
      bool s = radio.write(&text, sizeof(text));
      if(s) {
        int c = checkStrength();
        // Serial.print("No of Packet Sent: ");
        // Serial.println(c);
        
      }
    } else{
      // Serial.println("No Device Paired");
    }
  f_wdt = 0;

  // Serial.println("Going to sleep");
  turnOnRadio(false);
  _delay(1);
  enterSleep();
    
  }
}

int checkStrength(){
  // radio.openWritingPipe(address);
  // radio.stopListening();

    radio.setRetries(0,0);
  int counter = 0;
  char buffer[32]= "***###";

    for(int i=0; i<100; i++)
{
   int status = radio.write(buffer,32); // send 32 bytes of data. It does not matter what it is
   if(status)
       counter++;

   delay(1); // try again in 1 millisecond
}
radio.setRetries(5,15);

return counter;
}

void _delay(uint32_t ms){
  uint32_t t = millis();
  while(millis()-t < ms);
}

bool timeout(uint32_t setTime, uint32_t currTime){
 bool s = false; 
  ((millis()-currTime) < setTime)?s=false:s=true;
  return s;
}

