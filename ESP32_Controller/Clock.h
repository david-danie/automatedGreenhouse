#ifndef Clock_h
#define Clock_h

class Clock {

  public:
    Clock();
    void startClock();
    bool setTime(uint8_t* currentTime);
    bool getTime(uint8_t* currentTime);
    
  private:
    uint8_t dato;
    
};

uint8_t bcd2bin(uint8_t bcd);
uint8_t bin2bcd(uint8_t bin);

#endif