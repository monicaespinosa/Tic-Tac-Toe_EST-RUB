// Libraries for the program
#include <LedControl.h>
// Pins to control the driver to the out put LEDs
const int DIN = 12; //11 
const int CLK = 13;
const int CS = 19;
// Setting up multiplexing pins
const int rowsIn = 2; // input to the arduino, interruption function
const int muxOut = 6; // output to multiplex the columns of the keyboards

// Rows related variables 
int selRow[3] = {3, 4, 5};
int *dirSelRow = selRow;
int totRowsByte = 2;
int numRow;
int keyRow;

// Columns related variables
int muxCol[3] = {7, 8, 9}; 
int *dirMuxCol = muxCol;
int totColByte = 2;
byte channel;
int colsOutput;
int pulsedCol;

// Debounce related variables
volatile boolean inputRows;
boolean lastInputRows = false;
unsigned long currentTime = 0;
unsigned long lastDebounceTime = millis();
const long debounceDelay = 100;

// Interruption flags
boolean LED = true;
boolean flag = false;
boolean reading = false;
boolean saveLed = false;
boolean standby = true;
boolean reset = false;

// LED output variable
int ledOut[3];
int ledValue;
const int ledGreen[3] = {2, 8, 32};
const int ledRed[3] = {4, 16, 64};
int led = 126;

// Turn related variables
int outCol;
boolean selC = false;
boolean nextTurn = true;
int isLed;
boolean outLed [9] = {0,0,0,
                      0,0,0,
                      0,0,0};
int outValLed [9] = {0,0,0,
                     0,0,0,
                     0,0,0};

// end game related variables
int rowResult[3] = {0,0,0};
int winner = 0; // 1->Green 2->Red
int ledOn = 0;
boolean endGame = false;

unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
const long interval = 200;
const long winInterval = 5000;
unsigned long winPreviousMillis = 0;

int count = 0;
int ledRow = 0;
int randNumber = 0;

LedControl lc = LedControl(DIN, CLK, CS, 0);

void setup() {
  randomSeed(analogRead(0));
  // LED-Driver setup
  lc.shutdown(0,false);
  lc.setIntensity(0,0);
  lc.clearDisplay(0);

  // Setup for multiplexers, setting the channels is the same for
  // both cases - it makes the code shorter to use them in vector
  // form
    // multiplexer rows, reading mode
  pinMode(selRow[0], OUTPUT);
  pinMode(selRow[1], OUTPUT);
  pinMode(selRow[2], OUTPUT);
  pinMode(rowsIn, INPUT);

  pinMode(muxOut, OUTPUT);
  pinMode(muxCol[0], OUTPUT);
  pinMode(muxCol[1], OUTPUT);
  pinMode(muxCol[2], OUTPUT);

  // Declare interruption
  attachInterrupt(digitalPinToInterrupt(rowsIn), saveRow, CHANGE);
  // setting the column multiplexor output to high - it can be a 
  // posibility that it works with blinking  
  digitalWrite(muxOut, HIGH);

  // Time interruption to set reading speed of the arduino, in this case it is also 
  // linked to change the channel on the row multiplexor, whose output is used to 
  // activate the other interruption. 
  cli(); // Stops interruption

  // Set whole Register to 0
  TCCR2A = 0;
  TCCR2B = 0;
  // Initialize counter value to 0
  TCNT2 = 0;
  // clockUc / (prescaler * desiredFreq) -1 -> must be less tha 256 (bit in timer) 
  OCR2A = 49; 
  // turn CTC mode on
  TCCR2A |= (1<<WGM21);

  // Set CS10 bit for 8 prescaler
  TCCR2B |= (1 << CS21); 
  // enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);
  sei(); // allow interruptions*/

  winner = 0;
}

void loop() {
  // Choosing which keyboard to read
  digitalWrite(muxCol[2], selC);
  digitalWrite(selRow[2], selC);

  // due to inestability with storing the value of the columns channel,
  // there is an oscilation only when reading from low to high, in this 
  // case du to the assignation of the significant bit of the channels 
  // it is in the odd channels (also I did it this way to symplify the
  // solution)  
  if((pulsedCol == 1)||(pulsedCol == 3)) outCol = 1;
  else outCol = pulsedCol;
  isLed = (3*(keyRow-1))+outCol; // assigning number to every led in the array 
                                 // based on the received column and row.
  
  if(LED){    
    outLed[isLed] = true;
    
    if (selC) ledValue = ledGreen[outCol];
    else ledValue = ledRed[outCol];
    
    if(nextTurn){
      ledOut[keyRow-1] = ledOut[keyRow-1] + ledValue;
      outValLed[isLed] = ledValue;
    }
    
    LED = !LED;
  }
  
  if(outLed[isLed]){
    nextTurn = false;
    ledOut[keyRow-1] = ledOut[keyRow-1];
  }else nextTurn = true;
  
  for(int i = 0; i < pow(2,totColByte); i++){
    for(int col = 0; col < totColByte; col++){
      channel = bitRead((totColByte+1)-i,col);
      digitalWrite(*dirMuxCol+col, channel);
      MuxRows(totRowsByte, *dirSelRow);
    }
    colsOutput = i;
  }

  if((flag) && ((millis() - lastDebounceTime) > debounceDelay)){
    if (nextTurn){
      selC = !selC;
      ledOn = ledOn +1;
    }
    flag = false;
    LED = true;
    lc.clearDisplay(0);
  }

  
  for(byte i=0;i<3;i++){
    if(i == 0) lc.setRow(0,i,ledOut[i]);
    else lc.setRow(0,3-i,ledOut[i]);
  }
  
  for(int i = 0; i < 3; i++) rowResult[i] = outValLed[(3*i)]+outValLed[(3*i)+1]+outValLed[(3*i)+2];
 
  winningConditions();
  if(endGame){
    randNumber = random(2);   
    if(winner == 1){
      led = 42;
   
    }else if(winner == 2){
      led = 84;
      
    }else if((winner == 0) && (ledOn == 9)){
      led = 126;
    }
    for(int i = 0; i < 3; i++) ledOut[i] = led;
    
    if(millis() - winPreviousMillis >= winInterval){
      winPreviousMillis = millis();
      endGame = false;
      standby = true;
    }
  }
  

  if(standby){
    if(randNumber == 0) fullSlide();
    else if(randNumber == 1) simpleSlide();
    
    for(byte i=0;i<3;i++){
      if(i == 0) lc.setRow(0,i,ledOut[i]);
      else lc.setRow(0,3-i,ledOut[i]);
    }
  }

  if(reset){
    ledOn = 0;
    led = 0;
    for(int i = 0; i < 9; i++){
      outValLed [i] = 0;
      outLed[i] = false;
    }
    for(int i = 0; i < 3; i++){
      rowResult[i] = 0;
      ledOut[i] = 0;
    }
    reset = false;
  }
}



//====================================================Functions=====================================================================
// takes an integer associated with a channel 
void selectChannel(int chnl, int numChnl, int dir){
  numRow = chnl;
  //// Select channel of the multiplexer
  for(int ch = 0; ch < numChnl; ch++){
    digitalWrite(dir+ch, bitRead(chnl,ch));
  }
}

void MuxRows(int numByte, int dir){
  if (reading){
    for(int i = 0; i < pow(2, numByte); i++){
      selectChannel(i, numByte, dir);
      inputRows = digitalRead(rowsIn);
    }
  reading = false;
  }
} 

// this function is to know who won - it is best to manage this as part of a library that 
// runs the minimax algorithm, so for now it works, but it is suceptible to change
void winningConditions(){
  // Row winning condition, if all leds ar the same color the added value will always be the same at the end
  if((rowResult[0] == 84)||(rowResult[1] == 84)||(rowResult[2] == 84)) winner = 2;
  else if((rowResult[0] == 42)||(rowResult[1] == 42)||(rowResult[2] == 42)) winner = 1;
  // Column winning condition, basically the ledValue must be the same for the 3 rows in that specific position, 
  // also the led values correspond uniquely to that specific position in the row.
  else if((outValLed[0] == outValLed[3])&&(outValLed[3] == outValLed[6])){
    if(outValLed[0] ==  2) winner = 1;
    else if (outValLed[0] ==  4) winner = 2;
  }else if((outValLed[1] == outValLed[4])&&(outValLed[4] == outValLed[7])){
    if(outValLed[1] ==  8) winner = 1;
    else if (outValLed[1] ==  16) winner = 2;
  }else if((outValLed[2] == outValLed[5])&&(outValLed[5] == outValLed[8])){
    if(outValLed[2] ==  32) winner = 1;
    else if (outValLed[2] ==  64) winner = 2;
  // Diagonal winning condition, basically row and column condition at the same time.
  }else if(((outValLed[0] ==  2)&&(outValLed[4] ==  8)&&(outValLed[8] ==  32))||
           ((outValLed[2] ==  32)&&(outValLed[4] ==  8)&&(outValLed[6] ==  2))) winner = 1;
  else if(((outValLed[0] ==  4)&&(outValLed[4] ==  16)&&(outValLed[8] ==  64))||
           ((outValLed[2] == 64)&&(outValLed[4] ==  16)&&(outValLed[6] == 4 ))) winner = 2;
  else winner = 0;

  if(((winner == 1) || (winner == 2) || (ledOn == 9)) && (standby == false)){
    endGame = true;
  }
}

//====================================================Interruption=====================================================================
ISR(TIMER2_COMPA_vect){
  reading = true;
}

void saveRow(){
  if(inputRows == HIGH){
    if(inputRows != lastInputRows){
      pulsedCol = colsOutput;
      keyRow = numRow;
    }
  }else{
    flag = true;
    lastDebounceTime = millis();
    lastInputRows = inputRows;
  }

  if(standby){
    winner = 0;
    reset = true;
    standby = false;
  } 
}

//====================================================LED "animation"=====================================================================
void simpleSlide(){
  
  if (millis() - previousMillis >= interval) {
    previousMillis = millis();
    if(winner == 1) ledOut[ledRow] = ledGreen[count];
    else if (winner == 2) ledOut[ledRow] = ledRed[count];
    else if (winner == 0) ledOut[ledRow] = ledRed[count] + ledGreen[count];
    
    if (count == 3){
      count = 0;
      ledOut[ledRow] = 0;
      ledRow++;
    }else count++;
  }
  if(ledRow == 3) ledRow = 0;
}

void fullSlide(){
  
  if (millis() - previousMillis >= interval) {
    previousMillis = millis();
    if(winner == 1) ledOut[ledRow] = led - ledGreen[count];
    else if (winner == 2) ledOut[ledRow] = led - ledRed[count];
    else if (winner == 0) ledOut[ledRow] = led - (ledRed[count] + ledGreen[count]);
    
    if (count == 3){
      count = 0;
      ledOut[ledRow] = led;
      ledRow++;
    }else count++;
  }
  if(ledRow == 3) ledRow = 0;
}
