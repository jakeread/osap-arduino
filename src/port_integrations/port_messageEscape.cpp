// message escape-er 

#include "port_messageEscape.h"
#include "../serializers.h"

// constructor just calls the parent... 
OSAP_Port_MessageEscape::OSAP_Port_MessageEscape(void) : VPort(OSAP_Runtime::getInstance()){};

OSAP_Port_MessageEscape::escape(String msg){
  // would write-into then bounce, maybe stashing... 
}