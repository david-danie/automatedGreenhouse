#ifndef Clock_h
#define Clock_h

class Clock {

  public:
    Clock();
    void startClock();
    bool setCurrentTime(uint8_t* currentTime);
    bool getCurrentTime(uint8_t* currentTime);
    
};

uint8_t bcd2bin(uint8_t bcd);
uint8_t bin2bcd(uint8_t bin);

#endif