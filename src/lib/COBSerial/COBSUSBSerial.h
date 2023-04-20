// example cobs-encoded usb-serial link 

#include <Arduino.h>

class COBSUSBSerial {
  public: 
    COBSUSBSerial(Serial_* _usbcdc);
    void begin(void);
    void loop(void);
    // check & read,
    boolean clearToRead(void);
    size_t getPacket(uint8_t* dest);
    // clear ahead?
    boolean clearToSend(void);
    // open at all?
    boolean isOpen(void);
    // transmit a packet of this length 
    void send(uint8_t* packet, size_t len);
  private: 
    Serial_* usbcdc = nullptr;
    // buffer, write pointer, length, 
    uint8_t rxBuffer[255];
    uint8_t rxBufferWp = 0;
    uint8_t rxBufferLen = 0;
    // ibid, 
    uint8_t txBuffer[255];
    uint8_t txBufferRp = 0;
    uint8_t txBufferLen = 0;
};