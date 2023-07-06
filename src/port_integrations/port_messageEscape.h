// message escape-er, 

#ifndef PORT_MESSAGE_ESCAPE_H_
#define PORT_MESSAGE_ESCAPE_H_

#include "../structure/ports.h"

#define PESCAPE_ROUTESET 44
#define PESCAPE_MSG 77

class OSAP_Port_MessageEscape : public VPort {
  public: 
    // -------------------------------- Constructor
    OSAP_Port_MessageEscape(void);
    // simple API, right ? and should be singleton, so:
    static void escape(String msg);
  private: 
    Route escapePath;
};

#endif 