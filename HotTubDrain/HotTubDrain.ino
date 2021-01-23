/**************************************************************************
 Show the flow rate of a sensor on a screen
 **************************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for SSD1306 display connected using software SPI (default case):
#define OLED_MOSI   11
#define OLED_CLK   13
#define OLED_DC    9
#define OLED_CS    10
#define OLED_RESET 8
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

//Flow sensor pin
#define FLOW_PIN 2

volatile int counter = 0;
volatile int totalCount = 0;
double flowRate;
double totalGallons;

void setup() {
  Serial.begin(9600);

  pinMode(FLOW_PIN, INPUT);           //Sets the pin as an input
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

}

void loop() {

  String perMinString = "Gal/M: ";
  String totalString = "Gallons: ";

  counter = 0;      // Reset the counter so we start counting from 0 again
  interrupts();   //Enables interrupts on the Arduino
  delay (1000);   //Wait 1 second 
  noInterrupts(); //Disable the interrupts on the Arduino
   
  //Start the math
  flowRate = (counter * 2.25);        //Take counted pulses in the last second and multiply by 2.25mL 
  flowRate = flowRate * 60;         //Convert seconds to minutes, giving you mL / Minute
  flowRate = flowRate / 1000;       //Convert mL to Liters, giving you Liters / Minute
  flowRate = flowRate / 3.785411784;       //Convert Liters to gallons


  totalGallons = (totalCount * 2.25);        //Take counted pulses in the last second and multiply by 2.25mL 
  totalGallons = totalGallons / 1000;       //Convert mL to Liters, giving you Liters / Minute
  totalGallons = totalGallons / 3.785411784;       //Convert Liters to gallons
  

  display.clearDisplay();

  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start position
    display.setTextSize(2);             // Draw 2X-scale text
    
  display.print(perMinString);
  display.print(flowRate);
  display.println("");

  display.print(totalString);
  display.print(totalGallons);
  

  display.display();
  
}

void Flow()
{
   counter++; //Every time this function is called, increment "count" by 1
   totalCount++;
}
