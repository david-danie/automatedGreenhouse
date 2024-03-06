#include "Server.h"
#ifndef Plant_h
#define Plant_h

class Plant {

  public:
    Plant();
    void setParameters(uint8_t* parameters);
    uint8_t* getParameters();
    bool readParametersEEPROM();
    void turnOffDevices();
    char* sendParameters();
    char* updateEEPROM(uint8_t condition);
    void addDay();

  private:
    uint8_t _systemStatus[28];
    char buffer[150];
};

void setBuzzer();

#endif