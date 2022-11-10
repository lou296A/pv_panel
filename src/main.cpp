


#include<Arduino.h>
#include <Wire.h>
#include <INA219_WE.h>
#include <SPI.h>
#include <SD.h>

INA219_WE ina219; // this is the instantiation of the library for the current sensor

// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;
// relay pin 
#define  relaypin 4 
#define  relayvcc 5
 


const int chipSelect = 10; //hardwired chip select for the SD card
unsigned int loop_trigger;
unsigned int int_count = 0; // a variables to count the interrupts. Used for program debugging.
float Ts = 0.001; //1 kHz control frequency.
// measurement 
float boost_Iin, boost_Vin; 
float pwm_out;
float V_in;
float vout,iL,dutyref,current_mA; // Measurement Variables
unsigned int sensorValue0,sensorValue1,sensorValue2,sensorValue3;  // ADC sample values declaration
float current_limit;
// mppt variable 
float D ; 
float Ppv; 
float Vpv; 
float  Dprev,Pprev,Vprev; 
bool final_charge; 
float final_charge_Dsat= 0.9;

// state of the smps
  boolean bat_status_prev; 
  boolean bat_status; 
  boolean relay_state;  

boolean input_switch;
int state_num=0,next_state;
String dataString;

void setup() {
  //Some General Setup Stuff 
  Wire.begin(); // We need this for the i2c comms for the current sensor
  Wire.setClock(700000); // set the comms speed for i2c
  ina219.init(); // this initiates the current sensor
  Serial.begin(9600); // USB Communication
  //Check for the SD Card
  Serial.println("\nInitializing SD card...");
 /* if (!SD.begin(chipSelect)) {
    Serial.println("* is a card inserted?");
    while (true) {} //It will stick here FOREVER if no SD is in on boot
  } else {
    Serial.println("Wiring is correct and a card is present.");
  }

  if (SD.exists("SD_Test.csv")) { // Wipe the datalog when starting
    SD.remove("SD_Test.csv");
  }*/
  noInterrupts(); //disable all interrupts
  analogReference(EXTERNAL); // We are using an external analogue reference for the ADC
  //SMPS Pins
  pinMode(13, OUTPUT); // Using the LED on Pin D13 to indicate status
  pinMode(2, INPUT_PULLUP); // Pin 2 is the input from the CL/OL switch
  pinMode(6, OUTPUT); // This is the PWM Pin
   bat_status_prev = digitalRead(2); 
   bat_status = bat_status_prev ; 
  //LEDs on pin 7 and 8
  pinMode(7, OUTPUT); //error led
  pinMode(8, OUTPUT); //some other digital out
  //Analogue input, the battery voltage (also port B voltage)
  pinMode(A0, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  // relay 
  pinMode(relaypin, OUTPUT);
  pinMode(relayvcc, OUTPUT); 
  digitalWrite(relayvcc, HIGH); 
  digitalWrite(relaypin, LOW ); 
  relay_state = LOW; 
  pwm_out = 0.02;
  //analogWrite(6, (int)( pwm_out* 255));

  // TimerA0 initialization for 1kHz control-loop interrupt.
  TCA0.SINGLE.PER = 999; //
  TCA0.SINGLE.CMP1 = 999; //
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV16_gc | TCA_SINGLE_ENABLE_bm; //16 prescaler, 1M.
  TCA0.SINGLE.INTCTRL = TCA_SINGLE_CMP1_bm;

  // TimerB0 initialization for PWM output
  TCB0.CTRLA = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm; //62.5kHz

  interrupts();  //enable interrupts.
  analogWrite(6, 120); //just a default state to start with


}
float saturation( float sat_input, float uplim, float lowlim) { // Saturation function
  if (sat_input > uplim) sat_input = uplim;
  else if (sat_input < lowlim ) sat_input = lowlim;
  else;
  return sat_input;
}
void sampling(){
  boost_Iin  = -(ina219.getCurrent_mA())/1000; // sample the inductor current (via the sensor chip)
  boost_Vin = analogRead(A0)*5.05/170; 
  vout =   analogRead(A3)*4.79/429;
  // (4.096 / 1023.0); 
    

}
float time; 
void sd_card(){
  time = millis(); 
  dataString = String(pwm_out) + "," + String(boost_Vin) + "," + String(boost_Iin)+ "," + String(vout)+","+String(digitalRead(2)) +"," + String(state_num)+","+String(relay_state); 
  Serial.println(dataString);
  File dataFile = SD.open("MG.csv", FILE_WRITE); 
    if (dataFile){ 
      dataFile.println(dataString); 
       Serial.println(" open"); 
    } else {
      Serial.println("File not open"); 
    }
    dataFile.close(); 
  }
float  mppt(float Ipv,float Vpv){
float deltaD = 0.02;
float Ppv = Vpv*Ipv;
float dv = Vpv - Vprev; 
if((Vpv > 5.4 )&&(final_charge == LOW)){
  Serial.println("decreasing vpv: ");
  D = Dprev +0.02; 
}
else if((Vpv < 4)&&(final_charge == LOW)){
   D = Dprev -0.02; ("increase vpv: ");
}
else if(((Ppv - Pprev) != 0) && (Vpv < 5.4 )&& (Vpv >4  )){
 if(((Ppv - Pprev) != 0) ){
  if((Ppv - Pprev) > 0){
       if((Vpv - Vprev) > 0){
      D = Dprev - deltaD;
      Serial.println("1"); 
    }
    else{
      Serial.println("2"); 
      Serial.println("Dprev: " + String(Dprev));
       D = Dprev + deltaD;}
  }
    else{
        if((Vpv - Vprev) > 0){
          Serial.println("3"); 
          D = Dprev + deltaD;}
    else{
        D = Dprev - deltaD;
        Serial.println("4"); 
        }
    }

    }    
      
else{
  D = Dprev;
  Serial.println("5");  
}

if( final_charge == HIGH){
if(D > final_charge_Dsat){
    D = final_charge_Dsat;
     Serial.println("othersat");  
    
      
}
else if(D <0){  
      D =0.1;
      
}}

else{
if(D > 0.9){
    D = 0.9;
    Serial.println("sat0.09");  
}
else if(D <0){  
      D =0;
      
}}}

//update internal values
Dprev = D;
Vprev = Vpv;
Pprev = Ppv;

return D;  
}
void loop() {
  if (loop_trigger == 1){ // FAST LOOP (1kHZ)
    state_num = next_state;
    dataString = String(pwm_out) + "," + String(boost_Vin) + "," + String(boost_Iin)+ "," + String(vout)+","+String(digitalRead(2)) +"," + String(state_num)+","+ String(relay_state);
   // Serial.println(dataString);
   
    pwm_out = saturation(pwm_out, 0.99, 0.01); //duty_cycle saturation
    analogWrite(6, (int)( pwm_out* 255)); // write it out (remember if it the buck inverting for the Buck here)
    sampling();
    //Serial.println("state_num_fastloop" + String(state_num)); 
    int_count++; //count how many interrupts since this was last reset to zero
    loop_trigger = 0; //reset the trigger and move on with life
  
  }
  if (int_count == 1000) { // SLOW LOOP (1Hz)
  //pwm_out = pwm_out + 0.01; 
      Serial.println(dataString);
     

    Serial.println("state " + String(state_num)); 
  switch(state_num){
    case 0:{ // IDLE
  
      if(vout > 4.5){ 
        bat_status_prev =  digitalRead(2); 
        next_state = 1; 
        Serial.println("state_0goingto_1 " + String(next_state)); 
      }
      else{ 
        pwm_out = pwm_out+0.01;
        Serial.println("state_0stay_0 " + String(state_num)); 
      }
    
      break; 
    }
    case 1:{// wait for the batterie to be connected 
    
      bat_status = digitalRead(2);
      if(0){ 
        Serial.println("state_1going_0 " + String(state_num)); 
        next_state = 0; 
        }
      else if(bat_status_prev != bat_status){
        digitalWrite(relaypin,HIGH);
        relay_state = HIGH; 
        bat_status_prev =  digitalRead(2); 
         Serial.println("state_1going_2 ");
        next_state = 2; 
         
         
        }
        break; 
        }
    
      case 2:{// cruise
      bat_status = digitalRead(2);
      
       if((vout > 5.1)&&(vout < 5.4)){
        next_state = 3; 
        }
      else  if(vout > 5.4){
        pwm_out = 0; 
        digitalWrite(relaypin,LOW); 
        relay_state = LOW; 
        next_state =  0; }
      else if(vout<4.5){
          pwm_out = 0; 
          digitalWrite(relaypin,LOW); 
          relay_state = LOW; 
          next_state =  0; 
            
         }
      else if(bat_status_prev != bat_status){
        digitalWrite(relaypin,LOW);
         next_state = 0; 
          relay_state = LOW; 
         }
      else{
           pwm_out =  mppt( boost_Iin,boost_Vin);
           }
      break; 
      }
    case 3:{
       final_charge = 1; 
       if((vout > 5.1)&&(vout < 5.4)){
      pwm_out = pwm_out-0.02;
      final_charge_Dsat  = pwm_out; 
      
     
      }
      else if(vout > 5.4){
        pwm_out = 0; 
        digitalWrite(relaypin,LOW); 
        relay_state = LOW; 
        next_state =  0; 
    }
      else{
      next_state = 2; 
      }
      // reverse mttp
      break; 
    }
}
   
 int_count =0; 


 
}
}

// Timer A CMP1 interrupt. Every 1000us the program enters this interrupt. This is the fast 1kHz loop
ISR(TCA0_CMP1_vect) {
  loop_trigger = 1; //trigger the loop when we are back in normal flow
  TCA0.SINGLE.INTFLAGS |= TCA_SINGLE_CMP1_bm; //clear interrupt flag
}