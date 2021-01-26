/**************************************************************************
 Show the flow rate of a sensor on a screen
 **************************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH          128 // OLED display width, in pixels
#define SCREEN_HEIGHT         64 // OLED display height, in pixels

// Declaration for SSD1306 display connected using software SPI (default case):
#define OLED_MOSI             11
#define OLED_CLK              13
#define OLED_DC               9
#define OLED_CS               10
#define OLED_RESET            8

//Flow sensor pin
#define FLOW_PIN              2

//Pin to the LED that will blink
#define FLOW_BLINK_PIN        7

#define MAX_BLINK_SPEED_MS    1250
#define MAX_FLOW_RATE         20

#define SAMPLE_TIME_MS        1000    //How many MS to sample for when calculating per sec data
#define ML_PER_PULSE          2.25         //Number of milliliters per pulse
#define LITERS_PER_GALLON     3.785411784  //number of liters in a gallon

//These are declared volatile because they are set during an interrupt
volatile int counter          = 0;     //counter for sampling, resets every loop
volatile int totalCount       = 0;  //total count never resets
double flowRate               = 0;  //How many gallons per minute is flowing
double totalGallons           = 0;  //Total gallons that have flowed

unsigned long timeOfLastSample = 0;

double lastTotalCountBlink   = 0;  //This is stored to see if counter changed
unsigned long timeOfLastBlink = 0;  //When did we last change blink state?

unsigned int TARGET_GALLONS            = 300;  //How many gallons are in the hottub

unsigned long START_TIME                = 0;   //set during setup


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);


void setup() {

  START_TIME = millis();
  
  Serial.begin(9600);

  pinMode(FLOW_PIN, INPUT);           //Sets the pin as an input
  pinMode(FLOW_BLINK_PIN, OUTPUT);    //Sets the pin as an input
  attachInterrupt(0, Flow, RISING);  //Configures interrupt 0 (pin 2 on the Arduino Uno) to run the function "Flow" 

  

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }


  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(1000); // Pause for 1 seconds

  // Clear the buffer
  display.clearDisplay();

  timeOfLastSample = millis();

}

void loop() {

  if( hitSampleTime() )
  {
    
    sampleFlowRate();  
  }
  

  blinkFlowPin();
  
  writeDisplay();

}

bool hitSampleTime()
{
  if( ( millis() - timeOfLastSample ) >= SAMPLE_TIME_MS )
  {
    return true;
  }

  return false;
}

void sampleFlowRate()
{
  

  noInterrupts(); //Disable the interrupts on the Arduino so counter does not change
   
  //Start the math
  flowRate = (counter * ML_PER_PULSE);        //Take counted pulses in the last second and multiply by 2.25mL 
  flowRate = flowRate * ( SAMPLE_TIME_MS/1000 ) * 60;         //Convert seconds to minutes, giving you mL / Minute
  flowRate = flowRate / 1000;       //Convert mL to Liters, giving you Liters / Minute
  flowRate = flowRate / 3.785411784;       //Convert Liters to gallons


  totalGallons = (totalCount * ML_PER_PULSE);        //Take counted pulses in the last second and multiply by 2.25mL 
  totalGallons = totalGallons / 1000;       //Convert mL to Liters, giving you Liters / Minute
  totalGallons = totalGallons / LITERS_PER_GALLON;       //Convert Liters to gallons

  interrupts();   //Enables interrupts on the Arduino

  counter = 0;      // Reset the counter so we start counting from 0 again  
  timeOfLastSample = millis();  
 
}

int gallonsRemaining()
{
  return TARGET_GALLONS - totalGallons;
}

unsigned long msRemaining()
{
  int gallonsRemain = gallonsRemaining();

  unsigned long minutesRemaining = gallonsRemain / flowRate;

  return minutesRemaining * 60 * 1000;
}

unsigned long timeSinceStart()
{
  return millis() - START_TIME;
}

String convertMsToString( unsigned long ms )
{
  unsigned long ts = ms/1000;               //convert to seconds 
  unsigned long hr = ts/3600;                                                        //Number of seconds in an hour
  unsigned long mins = (ts-hr*3600)/60;                                              //Remove the number of hours and calculate the minutes.
  unsigned long sec = ts-hr*3600-mins*60;                                            //Remove the number of hours and minutes, leaving only seconds.
  String hrMinSec = (String(hr) + ":" + String(mins) + ":" + String(sec));  //Converts to HH:MM:SS string. This can be returned to the calling function.

  return hrMinSec;
}

void writeDisplay()
{
  
  String perMinString = "G/M: ";
  String perMinCountString = "c/S: ";
  String totalString = "G:   ";
  String totalCountString = "TC:  ";
  String runTimeString = "Run Time:  ";
  String remainTimeString = "Remaining:  ";
  String runTime = convertMsToString( timeSinceStart() );
  String remainTime = convertMsToString( msRemaining() );

  display.clearDisplay();

  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start position
  display.setTextSize(1);             // Draw 2X-scale text
    
  display.print(perMinString);
  display.print(flowRate);
  display.println("");


  display.print(perMinCountString);
  display.print(counter);
  display.println("");
  
  display.print(totalString);
  display.print(totalGallons);
  display.println("");

  //display.print(totalCountString);
  //display.print(totalCount);
  //display.println("");

  display.print(runTimeString);
  display.println(runTime);
  
  display.print(remainTimeString);
  display.println(remainTime);
  
  

  display.display();
  
}

unsigned int msBetweenBlinks()
{
  int flowRateInverted = 0;

  if( flowRate < MAX_FLOW_RATE )    
    flowRateInverted = MAX_FLOW_RATE - flowRate;

  int ms = map( flowRateInverted, 0, MAX_FLOW_RATE, 0, MAX_BLINK_SPEED_MS);

  return ms;
  
}

/**
 * Is totalGallons changing? if so, we are flowing
 * 
 */
bool isFlowing()
{
  if( lastTotalCountBlink != totalCount )
  {
    lastTotalCountBlink = totalCount;
    return true;
  }
  else
  {
    return false;
  }
  
}

void blinkFlowPin()
{
  if( !isFlowing() )
  {
    //Turn off the led
    if( digitalRead(FLOW_BLINK_PIN) )
      digitalWrite(FLOW_BLINK_PIN, LOW );
      
    return false;
  }
  
  unsigned long msSinceBlink = millis() - timeOfLastBlink;

  if( msSinceBlink > msBetweenBlinks() )
  {
    togglePin(FLOW_BLINK_PIN); 
    timeOfLastBlink = millis();
  }    

  
}

void togglePin( unsigned int pin )
{
  digitalWrite(pin, !digitalRead(pin)); 
}

void Flow()
{
   counter++; //Every time this function is called, increment "count" by 1
   totalCount++;
}
