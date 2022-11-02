#include "mgos.h"
#include <stdint.h>


// At least for the ATTiny X4/X5, receiving has to be disabled due to
// missing libm depencies (udivmodhi4)


// Number of maximum high/Low changes per packet.
// We can handle up to (unsigned long) => 32 bit * 2 H/L changes per bit + 2 for sync


void switchOn2(int nGroupNumber, int nSwitchNumber);
void switchOff2(int nGroupNumber, int nSwitchNumber);
void switchOn1(char sFamily, int nGroup, int nDevice);
void switchOff1(char sFamily, int nGroup, int nDevice);
void switchOn3(const char* sGroup, const char* sDevice);
void switchOff3(const char* sGroup, const char* sDevice);
void switchOn(char sGroup, int nDevice);
void switchOff(char sGroup, int nDevice);

void sendTriState(const char* sCodeWord);
void send1(unsigned long code, unsigned int length);
void send(const char* sCodeWord);
    

void enableReceive(int interrupt);
void enableReceive1();
void disableReceive();
int available();
void resetAvailable();

unsigned long getReceivedValue();
unsigned int getReceivedBitlength();
unsigned int getReceivedDelay();
unsigned int getReceivedProtocol();
unsigned int* getReceivedRawdata();

void enableTransmit(int nTransmitterPin);
void disableTransmit();
void setPulseLength(int nPulseLength);
void setRepeatTransmit(int nRepeat);
void setReceiveTolerance(int nPercent);


/**
* Description of a single pule, which consists of a high signal
* whose duration is "high" times the base pulse length, followed
* by a low signal lasting "low" times the base pulse length.
* Thus, the pulse overall lasts (high+low)*pulseLength
*/
typedef struct HighLow {
uint8_t high;
uint8_t low;
} HighLow;

/**
* A "protocol" describes how zero and one bits are encoded into high/low
* pulses.
*/
typedef struct Protocol_t {
/** base pulse length in microseconds, e.g. 350 */
uint16_t pulseLength;
HighLow syncFactor;
HighLow zero;
HighLow one;
uint8_t invertedSignal;
} Protocol_t;

void setProtocol(Protocol_t protocol);
void setProtocol1(int nProtocol);
void setProtocol2(int nProtocol, int nPulseLength);
char* getCodeWordA(const char* sGroup, const char* sDevice, bool bStatus);
char* getCodeWordB(int nAddressCode, int nChannelCode, bool bStatus);  
char* getCodeWordC(char sFamily, int nGroup, int nDevice, bool bStatus);
char* getCodeWordD(char sGroup, int nDevice, bool bStatus);
void transmit_data(HighLow pulses);

void handleInterrupt_cb();
int receiveProtocol(const int p, unsigned int changeCount);
volatile unsigned long nReceivedValue;
volatile unsigned int nReceivedBitlength;
volatile unsigned int nReceivedDelay;
volatile unsigned int nReceivedProtocol;
const unsigned int nSeparationLimit;



void RCSwitch_Init(void);
