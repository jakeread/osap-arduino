// RGB LED and Button device https://github.com/modular-things/modular-things-circuits/tree/main/rgbb 
// made network-accessible via OSAP

#include <osap.h>
#include <vt_endpoint.h>
#include <vp_arduinoSerial.h>

// ---------------------------------------------- Declare Pins
#define PIN_R 14
#define PIN_G 15
#define PIN_B 16
#define PIN_BUT 17

// ---------------------------------------------- Instantiate OSAP
// we use a fixed memory model, which can be stretched for performance 
// or squished to save flash, 
#define OSAP_STACK_SIZE 10
VPacket messageStack[OSAP_STACK_SIZE];
// ---------------------------------------------- OSAP central-nugget 
// the first argument here names the device, as it will appear in 
// graphs, or via searches 
OSAP osap("rgbb", messageStack, OSAP_STACK_SIZE);

// ---------------------------------------------- 0th Vertex: OSAP USB Serial
// this is a usb-serial encapsulation, 
VPort_ArduinoSerial vp_arduinoSerial(&osap, "usbSerial", &Serial);

// ---------------------------------------------- 1st Vertex: RGB Inputs Endpoint 
// a callback handler for when someone sends data to our 1st vertex, 
EP_ONDATA_RESPONSES onRGBData(uint8_t* data, uint16_t len){
  // we did the float -> int conversion in js 
  analogWrite(PIN_R, data[0]);
  analogWrite(PIN_G, data[1]);
  analogWrite(PIN_B, data[2]);
  return EP_ONDATA_ACCEPT;
}

Endpoint rgbEndpoint(&osap, "rgbValues", onRGBData);

// ---------------------------------------------- 2nd Vertex: Button Endpoint 
// an endpoint that we can write button states to, to pipe to others in 
// the graph... 
Endpoint buttonEndpoint(&osap, "buttonState");

void setup() {
  // startup OSAP
  osap.init();
  // startup the serialport encapsulation,  
  vp_arduinoSerial.begin();
  // setup our RGB led with PWM 
  analogWriteResolution(8);
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  analogWrite(PIN_R, 255);
  analogWrite(PIN_G, 255);
  analogWrite(PIN_B, 255);
  // pull-down switch, high when pressed
  pinMode(PIN_BUT, INPUT);
}

// button-reading (and debouncing) variables, 
uint32_t debounceDelay = 10;
uint32_t lastButtonCheck = 0;
boolean lastButtonState = false;

void loop() {
  // we call this once / cycle, to operate graph transport, 
  osap.loop();
  // debounce and set button states, 
  if(lastButtonCheck + debounceDelay < millis()){
    lastButtonCheck = millis();
    boolean newState = digitalRead(PIN_BUT);
    if(newState != lastButtonState){
      lastButtonState = newState;
      // and publish new data to the endpoint 
      buttonEndpoint.write(lastButtonState);
    }
  }
}