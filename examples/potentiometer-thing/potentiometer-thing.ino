// two-channel potentiometer device https://github.com/modular-things/modular-things-circuits/tree/main/potentiometer
// made network-accessible via OSAP

#include <osap.h>
#include <vt_endpoint.h>
#include <vp_arduinoSerial.h>

// TODO: update... 

// ---------------------------------------------- Declare Pins
#define PIN_POT1 8
#define PIN_POT2 7

// ---------------------------------------------- Instantiate OSAP
// we use a fixed memory model, which can be stretched for performance
// or squished to save flash,
#define OSAP_CONFIG_STACK_SIZE 10
VPacket messageStack[OSAP_CONFIG_STACK_SIZE];
// ---------------------------------------------- OSAP central-nugget
// the first argument here names the device, as it will appear in
// graphs, or via searches
OSAP osap("potentiometer", messageStack, OSAP_CONFIG_STACK_SIZE);

// ---------------------------------------------- 0th VPort: OSAP USB Serial
// this is a usb-serial encapsulation,
OSAP_ArduinoSerLink vp_arduinoSerial(&osap, "usbSerial", &Serial);

// ---------------------------------------------- 1st VPort: Potentiometer Values
boolean prePotQuery(void);
Endpoint tofEndpoint(&osap, "potentiometerQuery", prePotQuery);
// we can declare a callback function that is run ahead of network queries to this endpoint,
// here, we only read the potentiometers when this has happened, and write their readings
// to the endpoint
boolean prePotQuery(void) {
  uint8_t buf[4];
  uint16_t value1 = analogRead(PIN_POT1);
  uint16_t value2 = analogRead(PIN_POT2);
  buf[0] = value1 & 0xFF;
  buf[1] = value1 >> 8 & 0xFF;
  buf[2] = value2 & 0xFF;
  buf[3] = value2 >> 8 & 0xFF;
  tofEndpoint.write(buf, 4);
  return true;
}

void setup() {
  // startup OSAP
  osap.init();
  // startup the serialvport encapsulation,
  vp_arduinoSerial.begin();
  // setup our potentiometers as inputs
  pinMode(PIN_POT1, INPUT);
  pinMode(PIN_POT2, INPUT);
}

void loop() {
  // we call this once / cycle, to operate graph transvport,
  osap.loop();
}
