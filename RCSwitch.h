/*
  RCSwitch - Arduino libary for remote control outlet switches
  Copyright (c) 2011 Suat Özgür.  All right reserved.

  Contributors:
  - Andre Koehler / info(at)tomate-online(dot)de
  - Gordeev Andrey Vladimirovich / gordeev(at)openpyro(dot)com
  - Skineffect / http://forum.ardumote.com/viewtopic.php?f=2&t=46
  - Dominik Fischer / dom_fischer(at)web(dot)de
  - Frank Oltmanns / <first name>.<last name>(at)gmail(dot)com
  - Max Horn / max(at)quendi(dot)de
  - Robert ter Vehn / <first name>.<last name>(at)gmail(dot)com
  
  Project home: https://github.com/sui77/rc-switch/

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
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
