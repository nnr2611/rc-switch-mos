
#include "RCSwitch.h"
#include "mgos_gpio.h"
#include "mgos_time.h"
#include "mgos_system.h"
#define RCSWITCH_MAX_CHANGES 67
// interrupt handler and related code must be in RAM on ESP8266,
// according to issue #46.
#define RECEIVE_ATTR ICACHE_RAM_ATTR
#define VAR_ISR_ATTR

/* Format for protocol definitions:
 * {pulselength, Sync bit, "0" bit, "1" bit, invertedSignal}
 *
 * pulselength: pulse length in microseconds, e.g. 350
 * Sync bit: {1, 31} means 1 high pulse and 31 low pulses
 *     (perceived as a 31*pulselength long pulse, total length of sync bit is
 *     32*pulselength microseconds), i.e:
 *      _
 *     | |_______________________________ (don't count the vertical bars)
 * "0" bit: waveform for a data bit of value "0", {1, 3} means 1 high pulse
 *     and 3 low pulses, total length (1+3)*pulselength, i.e:
 *      _
 *     | |___
 * "1" bit: waveform for a data bit of value "1", e.g. {3,1}:
 *      ___
 *     |   |_
 *
 * These are combined to form Tri-State bits when sending or receiving codes.
 */
Protocol_t protocol;
uint16_t bit_pattern[126];

Protocol_t proto[] = {
    {350, {1, 31}, {1, 3}, {3, 1}, 0},    // protocol 1
    {650, {1, 10}, {1, 2}, {2, 1}, 0},    // protocol 2
    {100, {30, 71}, {4, 11}, {9, 6}, 0},  // protocol 3
    {380, {1, 6}, {1, 3}, {3, 1}, 0},     // protocol 4
    {500, {6, 14}, {1, 2}, {2, 1}, 0},    // protocol 5
    {450, {23, 1}, {1, 2}, {2, 1}, 1},    // protocol 6 (HT6P20B)
    {150, {2, 62}, {1, 6}, {6, 1}, 0},    // protocol 7 (HS2303-PT, i. e. used in AUKEY Remote)
    {200, {3, 130}, {7, 16}, {3, 16}, 0}, // protocol 8 Conrad RS-200 RX
    {200, {130, 7}, {16, 7}, {16, 3}, 1}, // protocol 9 Conrad RS-200 TX
    {365, {18, 1}, {3, 1}, {1, 3}, 1},    // protocol 10 (1ByOne Doorbell)
    {270, {36, 1}, {1, 2}, {2, 1}, 1},    // protocol 11 (HT12E)
    {320, {36, 1}, {1, 2}, {2, 1}, 1}     // protocol 12 (SM5212)
};

enum
{
  numProto = sizeof(proto) / sizeof(proto[0])
};

int nRepeat;
int nTransmitterPin;
int nRepeatTransmit;
int nReceiverInterrupt;
int nReceiveTolerance;
volatile unsigned long nReceivedValue = 0;
volatile unsigned int nReceivedBitlength = 0;
volatile unsigned int nReceivedDelay = 0;
volatile unsigned int nReceivedProtocol = 0;
int nReceiveTolerance = 60;
const unsigned int nSeparationLimit = 4300;
unsigned int timings[RCSWITCH_MAX_CHANGES];

void RCSwitch_Init(void)
{
  nTransmitterPin = -1;
  setRepeatTransmit(10);
  setProtocol1(1);
}

/**
 * Sets the protocol to send.
 */
void setProtocol(Protocol_t protocol)
{
  protocol = protocol;
}

/**
 * Sets the protocol to send, from a list of predefined protocols
 */
void setProtocol1(int nProtocol)
{
  if (nProtocol < 1 || nProtocol > numProto)
  {
    nProtocol = 1; // TODO: trigger an error, e.g. "bad protocol" ???
  }
  protocol = proto[nProtocol - 1];
}

/**
 * Sets the protocol to send with pulse length in microseconds.
 */
void setProtocol2(int nProtocol, int nPulseLength)
{
  setProtocol1(nProtocol);
  setPulseLength(nPulseLength);
}

/**
 * Sets pulse length in microseconds
 */
void setPulseLength(int nPulseLength)
{
  protocol.pulseLength = nPulseLength;
}

/**
 * Sets Repeat Transmits
 */
void setRepeatTransmit(int nRepeat)
{
  nRepeatTransmit = nRepeat;
}


/**
 * Enable transmissions
 *
 * @param nTransmitterPin    Arduino Pin to which the sender is connected to
 */
void enableTransmit(int nTransmitterPin1)
{
  nTransmitterPin = nTransmitterPin1;
  mgos_gpio_set_mode(nTransmitterPin, MGOS_GPIO_MODE_OUTPUT);
}

/**
 * Disable transmissions
 */
void disableTransmit()
{
  nTransmitterPin = -1;
}

/**
 * Switch a remote switch on (Type D REV)
 *
 * @param sGroup        Code of the switch group (A,B,C,D)
 * @param nDevice       Number of the switch itself (1..3)
 */
void switchOn(char sGroup, int nDevice)
{
  sendTriState(getCodeWordD(sGroup, nDevice, true));
}

/**
 * Switch a remote switch off (Type D REV)
 *
 * @param sGroup        Code of the switch group (A,B,C,D)
 * @param nDevice       Number of the switch itself (1..3)
 */
void switchOff(char sGroup, int nDevice)
{
  sendTriState(getCodeWordD(sGroup, nDevice, false));
}

/**
 * Switch a remote switch on (Type C Intertechno)
 *
 * @param sFamily  Familycode (a..f)
 * @param nGroup   Number of group (1..4)
 * @param nDevice  Number of device (1..4)
 */
void switchOn1(char sFamily, int nGroup, int nDevice)
{
  sendTriState(getCodeWordC(sFamily, nGroup, nDevice, true));
}

/**
 * Switch a remote switch off (Type C Intertechno)
 *
 * @param sFamily  Familycode (a..f)
 * @param nGroup   Number of group (1..4)
 * @param nDevice  Number of device (1..4)
 */
void switchOff1(char sFamily, int nGroup, int nDevice)
{
  sendTriState(getCodeWordC(sFamily, nGroup, nDevice, false));
}

/**
 * Switch a remote switch on (Type B with two rotary/sliding switches)
 *
 * @param nAddressCode  Number of the switch group (1..4)
 * @param nChannelCode  Number of the switch itself (1..4)
 */
void switchOn2(int nAddressCode, int nChannelCode)
{
  sendTriState(getCodeWordB(nAddressCode, nChannelCode, true));
}

/**
 * Switch a remote switch off (Type B with two rotary/sliding switches)
 *
 * @param nAddressCode  Number of the switch group (1..4)
 * @param nChannelCode  Number of the switch itself (1..4)
 */
void switchOff2(int nAddressCode, int nChannelCode)
{
  sendTriState(getCodeWordB(nAddressCode, nChannelCode, false));
}


/**
 * Switch a remote switch on (Type A with 10 pole DIP switches)
 *
 * @param sGroup        Code of the switch group (refers to DIP switches 1..5 where "1" = on and "0" = off, if all DIP switches are on it's "11111")
 * @param sDevice       Code of the switch device (refers to DIP switches 6..10 (A..E) where "1" = on and "0" = off, if all DIP switches are on it's "11111")
 */
void switchOn3(const char *sGroup, const char *sDevice)
{
  sendTriState(getCodeWordA(sGroup, sDevice, true));
}

/**
 * Switch a remote switch off (Type A with 10 pole DIP switches)
 *
 * @param sGroup        Code of the switch group (refers to DIP switches 1..5 where "1" = on and "0" = off, if all DIP switches are on it's "11111")
 * @param sDevice       Code of the switch device (refers to DIP switches 6..10 (A..E) where "1" = on and "0" = off, if all DIP switches are on it's "11111")
 */
void switchOff3(const char *sGroup, const char *sDevice)
{
  sendTriState(getCodeWordA(sGroup, sDevice, false));
}

/**
 * Returns a char[13], representing the code word to be send.
 *
 */
char *getCodeWordA(const char *sGroup, const char *sDevice, bool bStatus)
{
  static char sReturn[13];
  int nReturnPos = 0;

  for (int i = 0; i < 5; i++)
  {
    sReturn[nReturnPos++] = (sGroup[i] == '0') ? 'F' : '0';
  }

  for (int i = 0; i < 5; i++)
  {
    sReturn[nReturnPos++] = (sDevice[i] == '0') ? 'F' : '0';
  }

  sReturn[nReturnPos++] = bStatus ? '0' : 'F';
  sReturn[nReturnPos++] = bStatus ? 'F' : '0';

  sReturn[nReturnPos] = '\0';
  return sReturn;
}

/**
 * Encoding for type B switches with two rotary/sliding switches.
 *
 * The code word is a tristate word and with following bit pattern:
 *
 * +-----------------------------+-----------------------------+----------+------------+
 * | 4 bits address              | 4 bits address              | 3 bits   | 1 bit      |
 * | switch group                | switch number               | not used | on / off   |
 * | 1=0FFF 2=F0FF 3=FF0F 4=FFF0 | 1=0FFF 2=F0FF 3=FF0F 4=FFF0 | FFF      | on=F off=0 |
 * +-----------------------------+-----------------------------+----------+------------+
 *
 * @param nAddressCode  Number of the switch group (1..4)
 * @param nChannelCode  Number of the switch itself (1..4)
 * @param bStatus       Whether to switch on (true) or off (false)
 *
 * @return char[13], representing a tristate code word of length 12
 */
char *getCodeWordB(int nAddressCode, int nChannelCode, bool bStatus)
{
  static char sReturn[13];
  int nReturnPos = 0;

  if (nAddressCode < 1 || nAddressCode > 4 || nChannelCode < 1 || nChannelCode > 4)
  {
    return 0;
  }

  for (int i = 1; i <= 4; i++)
  {
    sReturn[nReturnPos++] = (nAddressCode == i) ? '0' : 'F';
  }

  for (int i = 1; i <= 4; i++)
  {
    sReturn[nReturnPos++] = (nChannelCode == i) ? '0' : 'F';
  }

  sReturn[nReturnPos++] = 'F';
  sReturn[nReturnPos++] = 'F';
  sReturn[nReturnPos++] = 'F';

  sReturn[nReturnPos++] = bStatus ? 'F' : '0';

  sReturn[nReturnPos] = '\0';
  return sReturn;
}

/**
 * Like getCodeWord (Type C = Intertechno)
 */
char *getCodeWordC(char sFamily, int nGroup, int nDevice, bool bStatus)
{
  static char sReturn[13];
  int nReturnPos = 0;

  int nFamily = (int)sFamily - 'a';
  if (nFamily < 0 || nFamily > 15 || nGroup < 1 || nGroup > 4 || nDevice < 1 || nDevice > 4)
  {
    return 0;
  }

  // encode the family into four bits
  sReturn[nReturnPos++] = (nFamily & 1) ? 'F' : '0';
  sReturn[nReturnPos++] = (nFamily & 2) ? 'F' : '0';
  sReturn[nReturnPos++] = (nFamily & 4) ? 'F' : '0';
  sReturn[nReturnPos++] = (nFamily & 8) ? 'F' : '0';

  // encode the device and group
  sReturn[nReturnPos++] = ((nDevice - 1) & 1) ? 'F' : '0';
  sReturn[nReturnPos++] = ((nDevice - 1) & 2) ? 'F' : '0';
  sReturn[nReturnPos++] = ((nGroup - 1) & 1) ? 'F' : '0';
  sReturn[nReturnPos++] = ((nGroup - 1) & 2) ? 'F' : '0';

  // encode the status code
  sReturn[nReturnPos++] = '0';
  sReturn[nReturnPos++] = 'F';
  sReturn[nReturnPos++] = 'F';
  sReturn[nReturnPos++] = bStatus ? 'F' : '0';

  sReturn[nReturnPos] = '\0';
  return sReturn;
}

/**
 * Encoding for the REV Switch Type
 *
 * The code word is a tristate word and with following bit pattern:
 *
 * +-----------------------------+-------------------+----------+--------------+
 * | 4 bits address              | 3 bits address    | 3 bits   | 2 bits       |
 * | switch group                | device number     | not used | on / off     |
 * | A=1FFF B=F1FF C=FF1F D=FFF1 | 1=0FF 2=F0F 3=FF0 | 000      | on=10 off=01 |
 * +-----------------------------+-------------------+----------+--------------+
 *
 * Source: http://www.the-intruder.net/funksteckdosen-von-rev-uber-arduino-ansteuern/
 *
 * @param sGroup        Name of the switch group (A..D, resp. a..d)
 * @param nDevice       Number of the switch itself (1..3)
 * @param bStatus       Whether to switch on (true) or off (false)
 *
 * @return char[13], representing a tristate code word of length 12
 */
char *getCodeWordD(char sGroup, int nDevice, bool bStatus)
{
  static char sReturn[13];
  int nReturnPos = 0;

  // sGroup must be one of the letters in "abcdABCD"
  int nGroup = (sGroup >= 'a') ? (int)sGroup - 'a' : (int)sGroup - 'A';
  if (nGroup < 0 || nGroup > 3 || nDevice < 1 || nDevice > 3)
  {
    return 0;
  }

  for (int i = 0; i < 4; i++)
  {
    sReturn[nReturnPos++] = (nGroup == i) ? '1' : 'F';
  }

  for (int i = 1; i <= 3; i++)
  {
    sReturn[nReturnPos++] = (nDevice == i) ? '1' : 'F';
  }

  sReturn[nReturnPos++] = '0';
  sReturn[nReturnPos++] = '0';
  sReturn[nReturnPos++] = '0';

  sReturn[nReturnPos++] = bStatus ? '1' : '0';
  sReturn[nReturnPos++] = bStatus ? '0' : '1';

  sReturn[nReturnPos] = '\0';
  return sReturn;
}

/**
 * @param sCodeWord   a tristate code word consisting of the letter 0, 1, F
 */
void sendTriState(const char *sCodeWord)
{
  // turn the tristate code word into the corresponding bit pattern, then send it
  unsigned long code = 0;
  unsigned int length = 0;
  for (const char *p = sCodeWord; *p; p++)
  {
    code <<= 2L;
    switch (*p)
    {
    case '0':
      // bit pattern 00
      break;
    case 'F':
      // bit pattern 01
      code |= 1L;
      break;
    case '1':
      // bit pattern 11
      code |= 3L;
      break;
    }
    length += 2;
  }
  send1(code, length);

}

/**
 * @param sCodeWord   a binary code word consisting of the letter 0, 1
 */
void send(const char *sCodeWord)
{
  // turn the tristate code word into the corresponding bit pattern, then send it
  unsigned long code = 0;
  unsigned int length = 0;
  for (const char *p = sCodeWord; *p; p++)
  {
    code <<= 1L;
    if (*p != '0')
      code |= 1L;
    length++;
  }
  send1(code, length);
}

/**
 * Transmit the first 'length' bits of the integer 'code'. The
 * bits are sent from MSB to LSB, i.e., first the bit at position length-1,
 * then the bit at position length-2, and so on, till finally the bit at position 0.
 */
void send1(unsigned long code, unsigned int length)
{
  
  if (nTransmitterPin == -1)
    return;

  
  for (int nRepeat = 0; nRepeat < nRepeatTransmit; nRepeat++) {
    for (int i = length-1; i >= 0; i--) {
      if (code & (1L << i))
        transmit_data(protocol.one);
      else
        transmit_data(protocol.zero);
    }
    transmit_data(protocol.syncFactor);
  }
  // Disable transmit after sending (i.e., for inverted protocols)
  mgos_gpio_write(nTransmitterPin, 0);
}
/*
 * Transmit a single high-low pulse.
 */
void transmit_data(HighLow pulses)
{
 
  uint8_t firstLogicLevel = (protocol.invertedSignal) ? 0 : 1;
  uint8_t secondLogicLevel = (protocol.invertedSignal) ? 1 : 0;

  mgos_gpio_write(nTransmitterPin, firstLogicLevel);
  mgos_usleep(protocol.pulseLength * pulses.high);
  
  mgos_gpio_write(nTransmitterPin, secondLogicLevel);
  mgos_usleep(protocol.pulseLength * pulses.low);
  
}

/**
 * Set Receiving Tolerance
 */

void setReceiveTolerance(int nPercent)
{
  nReceiveTolerance = nPercent;
}

/**
 * Enable receiving data
 */
void enableReceive(int interrupt)
{
  nReceiverInterrupt = interrupt;
  mgos_gpio_set_mode(nReceiverInterrupt, MGOS_GPIO_MODE_INPUT);

  enableReceive1();
}
void enableReceive1()
{
  if (nReceiverInterrupt != -1)
  {
    nReceivedValue = 0;
    nReceivedBitlength = 0;
  }
  mgos_gpio_set_int_handler_isr(nReceiverInterrupt, MGOS_GPIO_INT_EDGE_ANY, handleInterrupt_cb, NULL);
  mgos_gpio_enable_int(nReceiverInterrupt);
}

/**
 * Disable receiving data
 */
void disableReceive()
{
  nReceiverInterrupt = -1;
}

int available()
{
  return nReceivedValue != 0;
}

void resetAvailable()
{
  nReceivedValue = 0;
}

unsigned long getReceivedValue()
{
  return nReceivedValue;
}

unsigned int getReceivedBitlength()
{
  return nReceivedBitlength;
}

unsigned int getReceivedDelay()
{
  return nReceivedDelay;
}

unsigned int getReceivedProtocol()
{
  return nReceivedProtocol;
}

/*unsigned int *getReceivedRawdata()
{
  return timings;
}*/

/* helper function for the receiveProtocol method */
static inline unsigned int diff(long A, long B)
{
  return abs(A - B);
}

/**
 *
 */
int receiveProtocol(const int p, unsigned int changeCount2)
{
  
  unsigned int ip;
  const Protocol_t pro = proto[p - 1];
  unsigned long code = 0;
  // Assuming the longer pulse length is the pulse captured in timings[0]
  const unsigned int syncLengthInPulses = ((pro.syncFactor.low) > (pro.syncFactor.high)) ? (pro.syncFactor.low) : (pro.syncFactor.high);
  const unsigned long delay = timings[0] / syncLengthInPulses;
  // const unsigned long delay = timings[0];
  const unsigned long delayTolerance = delay * nReceiveTolerance / 100;
 
  /* For protocols that start low, the sync period looks like
   *               _________
   * _____________|         |XXXXXXXXXXXX|
   *
   * |--1st dur--|-2nd dur-|-Start data-|
   *
   * The 3rd saved duration starts the data.
   *
   * For protocols that start high, the sync period looks like
   *
   *  ______________
   * |              |____________|XXXXXXXXXXXXX|
   *
   * |-filtered out-|--1st dur--|--Start data--|
   *
   * The 2nd saved duration starts the data
   */

  unsigned int firstDataTiming = (pro.invertedSignal) ? 2 : 1;
  
  for (ip = firstDataTiming; ip < changeCount2 - 1; ip = ip + 2)
  {

    code <<= 1;
    
    if (diff(timings[ip], delay * pro.zero.high) < delayTolerance && diff(timings[ip + 1], delay * pro.zero.low) < delayTolerance)
    {
      // zero
    }
    else if (diff(timings[ip], delay * pro.one.high) < delayTolerance && diff(timings[ip + 1], delay * pro.one.low) < delayTolerance)
    {
      // one
      code |= 1;
      
    }
    else
    {
      //  Failed
      return 0;
    }
  }
  if (changeCount2 > 7)
  { // ignore very short transmissions: no device sends them, so this must be noise
    nReceivedValue = code;
    nReceivedBitlength = (changeCount2 - 1) / 2;
    nReceivedDelay = delay;
    nReceivedProtocol = p;
    return 1;
  }
  return 0;
}

void handleInterrupt_cb(int pin, void *arg)
{
  static unsigned int changeCount = 0;
  static unsigned long lastTime = 0;
  static unsigned int repeatCount = 0;

  const long time = mgos_uptime_micros();
  const unsigned int duration = time - lastTime;

  if (duration > nSeparationLimit) {
    // A long stretch without signal level change occurred. This could
    // be the gap between two transmission.
    if ((repeatCount==0) || (diff(duration,timings[0]) < 200)) {
      // This long signal is close in length to the long signal which
      // started the previously recorded timings; this suggests that
      // it may indeed by a a gap between two transmissions (we assume
      // here that a sender will send the signal multiple times,
      // with roughly the same gap between them).
      repeatCount++;
      if (repeatCount == 2) {
        for(unsigned int i = 1; i <= numProto; i++) {
          if (receiveProtocol(i, changeCount)) {
            // receive succeeded for protocol i
            break;
          }
        }
        repeatCount = 0;
      }
    }
    changeCount = 0;
  }
 
  // detect overflow
  if (changeCount >= RCSWITCH_MAX_CHANGES) {
    changeCount = 0;
    repeatCount = 0;
  }

  timings[changeCount++] = duration;
  lastTime = time; 
  (void)arg;
  (void)pin;
}
