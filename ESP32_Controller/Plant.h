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
    bool addDay();
    bool addDay(unsigned long currentMillis);

  private:
    uint8_t _systemStatus[28];
    char buffer[180];
    bool _updateDayFlag;
    unsigned long _previousMillisAddDay;
};

void setBuzzer();

#endif