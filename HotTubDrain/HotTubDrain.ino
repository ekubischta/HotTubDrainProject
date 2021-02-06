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

//Flow sensor pin
#define DISPLAY_MODE_BUTTON_PIN      4

//Pin to the LED that will blink
#define FLOW_BLINK_PIN        7

#define MAX_BLINK_SPEED_MS    1250
#define MIN_BLINK_SPEED_MS    100
#define MAX_FLOW_RATE         20

#define SAMPLE_TIME_MS        1000    //How many MS to sample for when calculating per sec data
#define ML_PER_PULSE          2.25         //Number of milliliters per pulse
#define LITERS_PER_GALLON     3.785411784  //number of liters in a gallon


#define DISPLAY_MODE_COUNT    3   //How many modes are there?
#define DISPLAY_MODE_BASIC    1   //Mode 1 is the basic mode
#define DISPLAY_MODE_GPM      2   //Mode 2 shows just GPM and Total Gallons
#define DISPLAY_MODE_REMAIN   3   //Mode 3 shows Gallons and Time Remaining

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

unsigned int currentDisplayMode         = DISPLAY_MODE_BASIC ;   //The first mode is default


int displayButtonState;                       // the current reading from the input pin
int displayButtonStateLast              = LOW;  // the previous reading from the input pin


unsigned long displayButtonLastDebounceTime  = 0;  // the last time the output pin was toggled
unsigned long displayButtonDebounceDelay     = 50;    // the debounce time; increase if the output flickers



Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);


void setup() {

  START_TIME = millis();
  
  Serial.begin(9600);

  pinMode(FLOW_PIN, INPUT);           //Sets the pin as an input
  pinMode(FLOW_BLINK_PIN, OUTPUT);    //Sets the pin as an input
  attachInterrupt(0, Flow, RISING);  //Configures interrupt 0 (pin 2 on the Arduino Uno) to run the function "Flow" 

  pinMode(DISPLAY_MODE_BUTTON_PIN, INPUT);           //Sets the pin as an input
  

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
    writeDisplay();
  }
  
  blinkFlowPin();

  checkDisplayModeState();

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

  double minutesRemaining = gallonsRemain / flowRate;

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
      
  String hrMinSec = String(hr);
  hrMinSec.concat(":");
  hrMinSec.concat( String(mins) );
  hrMinSec.concat(":");
  hrMinSec.concat( String(sec) );

  return hrMinSec;
}

void checkDisplayModeState()
{
  int reading = digitalRead(DISPLAY_MODE_BUTTON_PIN);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != displayButtonStateLast) {
    // reset the debouncing timer
    displayButtonLastDebounceTime = millis();
  }

  if ((millis() - displayButtonLastDebounceTime) > displayButtonDebounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != displayButtonState) {
      displayButtonState = reading;

      // only advance the mode if displayButtonState is HIGH
      if (displayButtonState == HIGH) {

        currentDisplayMode++;

        if( currentDisplayMode > DISPLAY_MODE_COUNT )
          currentDisplayMode = 1;//Loop back around
        
      }
    }
  }

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  displayButtonStateLast = reading;
}

void writeDisplay()
{
  switch( currentDisplayMode )
  {
    case DISPLAY_MODE_BASIC : 
      writeDisplay_BASIC();
      break;
    case DISPLAY_MODE_GPM : 
      writeDisplay_GPM();
      break;   
    case DISPLAY_MODE_REMAIN : 
      writeDisplay_REMAIN();
      break;     
  }
}

void writeDisplay_BASIC()
{
  
  String perMinString =             "G/M : ";
  String perMinCountString =        "c/S : ";
  String totalString =              "G   : ";
  String totalCountString =         "TC  : ";
  String gallonsRemainingString =   "Gal Remain : ";
  String runTimeString =            "Run Time   : ";
  String remainTimeString =         "Remaining  : ";
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

  display.print(gallonsRemainingString);
  display.print(gallonsRemaining());
  display.println("");

  display.print(runTimeString);
  display.println(runTime);
  
  display.print(remainTimeString);
  display.println(remainTime);
  
  

  display.display();
  
}


void writeDisplay_GPM()
{
  
  String perMinString =             "Gal / Min:";
  String totalString =              "Total Gal:";

  display.clearDisplay();

  display.setTextSize(2);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start position
  display.setTextSize(2);             // Draw 2X-scale text
    
  display.print(perMinString);
  display.println("");
  display.print("   ");
  display.print(flowRate);
  display.println("");
  
  display.print(totalString);
  display.println("");
  display.print("   ");
  display.print(totalGallons);
    
  display.display();
  
}

void writeDisplay_REMAIN()
{
  
  
  String gallonsRemainingString =   "Gal Remain";
  
  String remainTimeString =         "Time Left";
  String remainTime = convertMsToString( msRemaining() );

  display.clearDisplay();

  display.setTextSize(2);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start position
  display.setTextSize(2);             // Draw 2X-scale text
    

  display.print(gallonsRemainingString);
  display.println("");
  display.print("   ");
  display.print(gallonsRemaining());
  display.println("");

  display.print(remainTimeString);
  display.println("  ");
  display.println(remainTime);
  
  

  display.display();
  
}


/**
 * Same as map, but returns a float
 */
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
 return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


unsigned int msBetweenBlinks()
{

  double flowRateInverted = 0;

  if( flowRate < MAX_FLOW_RATE )    
    flowRateInverted = MAX_FLOW_RATE - flowRate;

  int ms = mapfloat( flowRateInverted, 0, MAX_FLOW_RATE, MIN_BLINK_SPEED_MS, MAX_BLINK_SPEED_MS);

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
  
  unsigned long msSinceBlink = millis() - timeOfLastBlink;

  if( msSinceBlink > msBetweenBlinks() )
  {
    if( !isFlowing() )
    {
      //Turn off the led
      if( digitalRead(FLOW_BLINK_PIN) )
        digitalWrite(FLOW_BLINK_PIN, LOW );
        
      return;
    }
    
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
