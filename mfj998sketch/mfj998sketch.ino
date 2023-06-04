/* mfj998sketch
** Copyright (C) 2020 by Wayne Wright, W5XD. All rights reserved
**
** for REVISION 2 of the PCB
**
** This sketch is for a daughterboard (designed published in this
** same archive) plugged into the socket for U1 on the MFJ998 antenna tuner. 
** The processor on the daughterboard is an Arduino Pro Mini, which
** has an Atmega328 CPU.
**
** Why replace the manufacturer's PIC microprocessor?
**
** The daughterboard adds a RFM69HCW Wireless Transceiver for 915MHz
** with a maximum range of 500m. This allows telemetry of the
** tuner's state in both directions with the main operating shack.
**
** This sketch (currently) does not duplicate the entire feature set
** in the original firmware. Instead, its main purpose is to enable
** control of the tuner over a 900MHz radio from the shack rather than 
** indirectly through the tuner firmware's supported process of putting a 
** low level RF signal on the line in order to get the tuner to tune.
**
** What the daughterboard does and how to use it.
** On detecting this combination of circumstances:
** 1. power level has sampled in this range: 5W and 25W for
**  MSEC_OF_TUNE_POWER_TO_ENABLE_SEARCH consecutive msec (500) or longer
** 2. frequency detection has been in the range of its
** indexed EEPROM table.
** 3. if an SWR higher than "trigger SWR" is detected, it
**  decides whether to move its L and/or C based on:
**  If it has done an SWR search (defined below) since its
**  last powerup on a previous frequency in the range of
**  +/- 1/32 (FREQ_DELTA_FRACTION_PWR) of the current frequency, then it does nothing.
**  (you have to power it down and back up, or else QSY to
**  get it to change.)
**  4. It looks up the EEPROM stored L/C for the current frequency.
**  and puts the L/C to that table entry.
**
** All of the signals on the original PIC that remain useful are
** routed somewhere on the daughterboard and are available to this
** sketch. NOT all of them are used or supported (currently).
**
** Peripherals and daughterboard wiring
** (A) on the MFJ998 motherboard                             total on Atmega328
**     1. LCD display (7 pins, but 4 are on MCP23s08 extender)  3
**     2. WAKEUP (routed through port extender)                 4
**     3. Serial port (2 pins)                                  6 
**          (The arduino Serial appears on the tuner's back panel DB9.)
**     4. Buzzer                                                7
**     5. FREQ (RF as sniffed and made digital in schmidt trig) 8 
**     6. FWD/REV analog inputs  (2 pins)                       10
**     7. U3/U4 relay latches. (3 pins, including MOSI)         13
**     8. MFWD/MREV (PWM front panel meter outputs)             15
**     9. U2 rig controller START (but no support in sketch)    16
**     Front panel switches (6 shared with LCD)
**  (B) NOT routed from MFJ998 motherboard to daughter MPU
**     x EEPROM i2c SCL on motherboard (EEPROM on mainboard untouched)
**  (C) used pins internal to the daughterboard
**       SPI is 3 pins, but MOSI is shared with above           18
**     1. RFM69 transceiver (2 dedicated + SPI)                 20
**     2. 512KB SPI EEPROM    (1 dedicated + SPI)               21
**     3. CS on MC23S08 port extender                           22
**  (D) pins on MC23S08 port extender                          total on extender
**     1. 4 LCD pins D4 through D7                               4
**     2. PD (power down)                                        5
**     3. WAKEUP (input on extender, and INT output)             6
** 
** Arduino Pro Mini has 22 I/O pins total...all are used.
** The port extender has two unused pins.
**
** 900MHz GATEWAY
** This source archive has a GatewaySketch that implements the
** shack end of the 900MHz link, also requiring the RFM69 board.
**
** Test Harness.
** The daughterboard itself has headers for smaller break-out boards. 
** The Pro Mini board in particular has the constraint that it
** is mounted over SMD ICs. Without physical space in the tuner
** cabinet to allow for sockets, the Pro Mini has to be soldered
** in place. Do NOT solder the Pro Mini in place without following
** the instructions for using the Test Harness sketch on another
** Arduino board (like a Uno or Mega) to check your soldering of 
** the SMD ICs. 
**
** HARDWARE DETAILS
**
** The code in this sketch is a mix of use of standard Arduino libraries
** (e.g. SPI and pinMode, and digitalRead) and sections specific to the
** the Pro Mini hardware ATmega328P. The result is a sketch that,
** on the whole, can only run on the daughterboard as built on the PCB
** in this repo.
**
** For almost all the pins and controls on the MFJ998, the function
** and design intent is obvious. However, there are three that this author, 
** for full disclosure, is not so sure of:
**
** PD
** Its obviously an abbreviation for "power down" and an oscilloscope
** shows that the CPU gets about 15msec of operation after 12VDC 
** power is is removed such that the PD signal appears. What's not 
** obvious is what the CPU is supposed to do with those 15msec. Maybe
** it has to do with the EEPROM that stores the antenna settings?
** That EEPROM has data that must be accessible across power cycling, and it has
** a 5msec write cycle. That is, if you start a write cycle that
** is aborted by a power down, then you don't know at power up 
** what you're going to find. 
**
** K1 and its 25 ohm series resistor.
** The relay obviously adds R121, a 25 ohm series resistor, 15W, 
** between the SWR bridge and the matching network. Because its a 
** 15W part, its reasonable to assume its useful only at tuneup time 
** (i.e. opening K1 while 1500W is applied to the tuner would 
** vaporize R121.) It's definitely not for current limiting at
** relay switching time turning tuneup because its on the wrong
** side of the SWR bridge (it affects the measured SWR.) Guess again...
** My best guess: R121 is an aid to determining whether the
** antenna impedance real part is higher or lower than 50 ohms.
** Measure without; then add it in. If it makes things better,
** guess that the real part is below 50 ohms, otherwise above.
** Knowing that real part w.r.t. 50 ohms is useful because it says 
** whether the matching network capacitance belongs on the generator 
** side (real part below 50 ohms) or load side (real part less.)
** However, regardless of these guesses, this sketch has no code to use
** K1/R121.
**
** START
** This pin is used to communicate with another PIC CPU on the MFJ998
** motherboard that, in turn, interfaces to rig hardware. That second
** PIC is not even installed on the MFJ998RT (remote model). The
** author of this sketch has not seen any documentation for how
** that communication is to take place and this pin is not supported
** in this sketch.
**
** EEPROM
** the i2c EEPROM on the MFJ998 board is not accessed (and not even
** wired to the daughterboard.) This sketch therefore cannot change 
** it. That means the daughterboard and the factory PIC CPU
** can be interchanged at will, restoring the factory functionality
** without even having to do a reset-defaults.
**
** This daughterboard has its own EEPROM with the same capacity
** as the one on the main board. 
**
** The Atmega CPU also has (a much smaller) EEPROM accessed using the
** Arduino standard EEPROM library. This sketch stores its radio 
** setup parameters stored there, along with a few other settings.
** That EEPROM is accessed using the arduino <EEPROM.h>. The
** 512K-bit EEPROM on the daughterboard is accessed using a 
** for-this-purpose-only header at <eepromSpi.h>
*/

// standard Arduino libraries
#include <RFM69.h>      // the wireless transceiver
#include <SPI.h>        // Arduino SPI library 
#include <EEPROM.h>     // Arduinio EEPROM library

#include <LiquidCrystal.h> // MODIFIED to make write4bits virtual

#include <avr/sleep.h>  // sketch only really works on AVR ATmega328P
#include <avr/power.h>
/* The __AVR_ATmega328P__ is used below to mark code that is specific to that processor
** Don't take that to mean it might run on other hardware. It won't. */

#include "PinAssignments.h"

/* code shared with the Gateway that is deployed like a standard
** Arduino library, but has no particular use outside this
** sketch and the gateway sketch. */
#include <portExtender.h>
#include <eepromSpi.h>
#include <RadioConfiguration.h>

#define SERIAL_DEBUG 0 // printout and serial commands to aid debugging

/* All of these conditional compiles make the code much harder to read.
** Sorry about that. However...sometimes I was surprised by hardware
** devices that interact poorly. These compile symbols make it relatively
** easy to exclude software support for key devices and see if the 
** remaining ones change their behavior. */
#define USE_LCD 1
#define USE_EXTENDER 0
#define USE_RFM69 1
#define USE_EEPROM 1
#define USE_RELAYS 1
#define COUNT_FREQ_ON_T1 1 // The T1 input is counted using hardware support 
#define DO_PRINT_FREQ 1 // only relavant on COUNT_FREQ_ON_T1 with USE_LCD
#define PRINT_RADIO_RSSI 0 // one board I built has a radio that reports a questionable RSSI
#define EEPROM_TEST_COMMANDS 0

#define USING_SPI ((USE_RFM69 > 0) || (USE_EEPROM > 0) || (USE_LCD > 0) || (USE_EXTENDER > 0))

#include "Mfj998Relays.h" // depends on the above preprocessor definitions

/* Atmega328 timer/counter #0 is not used explicitly in the sketch because the Arduino timer library uses it
** timer/counter #1 is configured in this sketch to count the external T1 pin, for a frequency counter.
** timer/counter #2 is configured in this sketch to software-generate PWM for the FWD/REF meter outputs.
*/ 

namespace constants { 
    enum class FrontPanelButtons  : uint8_t { // seven buttons on the mfj998 front panel
        DISPATCH_NOBUTTON, DISPATCH_TUNE, DISPATCH_LDOWN, DISPATCH_LUP, DISPATCH_CUP,
        DISPATCH_CDOWN, DISPATCH_ANT, DISPATCH_MODE};

    const int LCD_WIDTH = 16; // number of characters on the LCD display hardware
    const uint16_t MAX_FWD_REF_DURING_SWITCHING = 165;  // about 25W  
    const uint16_t MIN_FWD_TO_ENABLE_SEARCH = MAX_FWD_REF_DURING_SWITCHING / 2;
    const uint16_t MIN_TRIGGER_SWR = 4; // which is 1.125:1 at (SWR-1)*64
    const int MSEC_OF_TUNE_POWER_TO_ENABLE_SEARCH = 500;
    const unsigned long INITIAL_AWAKE_MSEC_ON_POWERUP = 10000; // give user a chance to disable sleep
}
using namespace constants;

class FreqOver4 { 
public:// the daughterboard has a JK flipflop that divides the incoming frequency by 4.
    FreqOver4(): calibration(0x8000), startMicroseconds(0){}

    uint16_t getFreqRaw() const;   // units of MHz * 2048 
    uint16_t getFreqKhz() const;
    bool loop();
    void setup();
    void setCalibration(uint16_t v){ if (v != 0xffff) calibration=v;}
    void wakeup();
    void OnOvf();
    void OnCompA();

protected:
    enum {SCALE_POWER = 11}; // 2**11 = 2048
    struct Measurement {
        Measurement():usec(0), count(0){};
        uint16_t usec;  uint16_t count;
    };
    void acquire(unsigned long nowusec, uint16_t count);
    // the frequency measure is empiracly observed to be stable.
    // no need to keep more than one history point.
    Measurement history;
    uint16_t calibration;
    volatile unsigned long startMicroseconds;
    static const unsigned long MIN_USEC_AT_COMPA;
    static const unsigned long MAX_USEC_TO_DECLARE_ZERO;
    static const uint16_t FIRST_STOP_COUNT;
    static const uint16_t SECOND_STOP_COUNT;
};

class Buzzer {// it buzzes if we wiggle its pin (i.e. steady 5V won't sound)
public:
    Buzzer() :buzzing (false) {}

    bool loop()
    {   // using the loop to buzz introduces audible artifacts
        static unsigned long lastbuzz = micros();
        if (buzzing)
        {
            static const int intervalUSEC = 800; // half cycle. 625Hz
            long thisbuzz = micros();
            int diff = (thisbuzz - lastbuzz);
            if (diff >= intervalUSEC)
            {
                static uint8_t state = 0;
                digitalWrite(BUZZ_PIN, state++ == 0 ? HIGH : LOW);
                if (state > 2)
                    state = 0;
                lastbuzz = thisbuzz;
            }
            return true;
        }
        return false;
    }

    void buzz(uint16_t msec)
    {   // this one sounds good compared to what happens with the
        // loop() implementation. but you can't do anything else concurrently.
        buzzing = false;
        pinMode(BUZZ_PIN, OUTPUT);
        // Sets the pitch of the buzz
        static const int intervalUSEC = 1100; 
        uint32_t usecCount = 0;
        uint32_t finish = msec;
        finish *= 1000;
        while (usecCount < finish)
        {
            digitalWrite(BUZZ_PIN, HIGH);
            delayMicroseconds(intervalUSEC);
            digitalWrite(BUZZ_PIN, LOW);
            // duty cycle controls loudness.. and affects pitch
            delayMicroseconds(4*intervalUSEC);
            usecCount += 5 * intervalUSEC;
        } 
        digitalWrite(BUZZ_PIN, LOW);
        pinMode(BUZZ_PIN, INPUT);
    }
    bool isBuzzing() const { return buzzing;}
    operator bool () const { return buzzing;}
    bool operator = (bool v) { 
        buzzing = v; 
        pinMode(BUZZ_PIN, v ? OUTPUT : INPUT);
        return v;}
protected:
    bool buzzing;
};

class RadioRFM69 :  public RFM69, 
    public Print//has easy-to-use formatting routines for our telemetry
{
public:
    RadioRFM69() 
        : RFM69(RFM69_SELECT_PIN, RFM69_INTERRUPT_PIN, true)
        , m_initialized(false) ,m_telemetryPos(0), telemetryMsec(0){}

    bool loop()
    {
        if (!initialized())
            return false;

        // RECEIVING
        // In this section, we'll check with the RFM69HCW to see
        // if it has received any packets:
        if (receiveDone()) // Got one!
        {
            static unsigned char COPY[sizeof(DATA)];
            memcpy(COPY, DATA, sizeof(COPY));
            auto CopyLen = DATALEN;
            if (ACKRequested())
            {
                sendACK(); // library one doesn't work well with one build. see below
            }
#if SERIAL_DEBUG > 0
            Serial.print("received from node ");
            Serial.print(SENDERID, DEC);
            Serial.print(", message [");

            // The actual message is contained in the DATA array,
            // and is DATALEN bytes in size, plus a trailing NULL:

            for (byte i = 0; i < CopyLen; i++)
                Serial.print((char)COPY[i]);

            // RSSI is the "Receive Signal Strength Indicator",
            // smaller numbers mean higher power.

            Serial.print(F("], RSSI= "));
            Serial.println(RSSI);
#endif
            if (processCommand((const char *)&COPY[0]))
            {
            }
            return true;
        }
        return false;
    }

    void setup();    
    bool processCommand(const char *);

    bool initialized() const { return m_initialized;}

    void sendTelemetryPacket(uint16_t freq, uint16_t fwd, uint16_t ref,
            uint8_t cIndex, uint16_t lIndex, uint8_t cState);
    void sendStatusPacket(uint16_t triggerSwr, uint16_t stopSearchSwr);

    uint16_t getTelemetryMsec() const { return telemetryMsec; }
    void setTelemetryMsec(uint16_t v) { telemetryMsec = v;}

    unsigned long getLastTelemetryMsec() const { return lastTelemetryMsec; }
    void sendFrame(uint16_t toAddress, const void* buffer, uint8_t size, bool requestACK = false, bool sendACK = false)
    {
        auto now = millis();
        long diff = now - lastSendFrame;
        if (diff < MIN_SEND_FRAME_INTERVAL_MSEC)
            delay(diff);
        lastSendFrame = millis();
        RFM69::sendFrame(toAddress, buffer, size, requestACK, sendACK);
    }
    enum {MIN_SEND_FRAME_INTERVAL_MSEC = 50};
protected:
    unsigned long lastTelemetryMsec;
    size_t write(uint8_t) override;
    void sendACK() {
        /* this is the result of experimenting with a particular
        ** board. The readRSSI() return does not change for SECONDS
        ** after a received packet. The library implementation of sendACK
        ** waits for up to one second for the readRSSI value to fall...
        ** which it often (usually) does not.
        ** So here is a custom sendACK that just blasts away immediately. */
        lastSendFrame = millis();
        RFM69::sendFrame(SENDERID, "", 0, false, true);
    }
    bool m_initialized;
    char telemetryBuffer[RF69_MAX_DATA_LEN];
    int m_telemetryPos;
    uint16_t telemetryMsec;
    unsigned long lastSendFrame;
};

class MeterPWM {
    /* the Atmega328 doesn't have enough pins to
    ** do hardware PWM and everything else in this sketch.
    ** This class accomplishes pulse width modulation using
    ** TIMER2 and its pair of comparator registers to do
    ** (exactly) two PWM signals (which happens to be the number
    ** we need in the MFJ998, one for forward, one for reflected.*/    
public:
    void setup()
    {
        pinMode(MREF_PIN, OUTPUT);
        pinMode(MFWD_PIN, OUTPUT);
        digitalWrite(MREF_PIN, LOW);
        digitalWrite(MFWD_PIN, LOW);
#if defined(__AVR_ATmega328P__)
        TCCR2A = 0;
        /* this is the slowest prescaler setting in the '328.
        ** Going slow might cause the meter pointer to wiggle (vibrate.)
        ** ...but its meter accuracy is less affected by
        ** competing with interrupt processing. */
        TCCR2B = 0x7; // 1024 clock prescaler
        TIMSK2 = (1 << OCIE2B) | (1 << OCIE2A) | (1 << TOIE2);
#endif        
    }

    void setMeters(uint8_t fwd, uint8_t ref)
    {
#if defined(__AVR_ATmega328P__)
        // all we do is set the comparter registers
        // to the "analog" values we want.
        // Zero and max are special cases
        OCR2A = fwd;
        OCR2B = ref;
#endif
        if (fwd == 0)
            digitalWrite(MFWD_PIN, LOW);
        else if (fwd == 0xff)
            digitalWrite(MFWD_PIN, HIGH);
        if (ref == 0)
            digitalWrite(MREF_PIN, LOW);
        else if (ref == 0xff)
            digitalWrite(MREF_PIN, HIGH);
    }

#if defined(__AVR_ATmega328P__)
    void OnOvf()
    {
        if (OCR2A != 0)
            digitalWrite(MFWD_PIN, HIGH);
        else
            digitalWrite(MFWD_PIN, LOW);

        if (OCR2B != 0)
            digitalWrite(MREF_PIN, HIGH);
        else
            digitalWrite(MREF_PIN, LOW);
    }

    void OnCompA()
    {
        if (OCR2A != 0xff)
            digitalWrite(MFWD_PIN, LOW);
    }

    void OnCompB()
    {
         if (OCR2B != 0xff)
            digitalWrite(MREF_PIN, LOW);
    }
#endif

    void sleep()
    {
        digitalWrite(MFWD_PIN, LOW);
        digitalWrite(MREF_PIN, LOW);
    }
};

class ForwardReflectedVoltage
{
public:
    ForwardReflectedVoltage() : 
      lastReadMsec(0), nextHistory(0), startOfTunePower(0), 
      triggerSearchSwr(MIN_TRIGGER_SWR), stopSearchSwr(0)
    {}
    /* to match the MFJ998 firmware, a value of f=2112 (empirically determined, in readPowerX8() units)
    ** is defined to be the voltage that is 100W at a 50 ohm match.
    ** The analogRead uses the board's 5V supply as a standard, which corresponds
    ** to 1023 (which fits in 10 bits). The 2121 value is the sum of 8 sampling
    ** intervals. That means it corresponds to V = 5 V * 2112 / (8 * 1023) = 1.29 Volts 
    ** Power scales as the square of voltage (so at 1500W, we expect sqrt(1500/100) * 1.29V = 5V.
    ** Compensation for nonlinearity in the bridge's detection
    ** diodes. They are schotkey diodes that can be modeled reasonably as having 300mV of
    ** forward voltage drop. That means we should add that much to the analogRead values.
    ** 200mV is 1023 * .2 / 5 = 41 */
    void readPowerX8(uint16_t &f, uint16_t &r) const; // time averaged over HISTORY_LEN
    void acquirePowerNow(uint16_t &f, uint16_t &r) const; // units reduced by 1/HISTORY_LEN
    bool loop(unsigned long);
    void setup();
    bool TimeToSearch(long now);
    bool OkToSwitchRelays();
    void sleep();
    bool OkToSwitchRelays(uint16_t f, uint16_t r);
    void setTriggerSwr(uint16_t swr);
    void setStopSearchSwr(uint16_t swr);
    uint16_t StopSearchSwr() const { return stopSearchSwr;}
    uint16_t TriggerSwr() const { return triggerSearchSwr;}
    // return is (SWR-1) * 64 
    static uint16_t Swr(uint16_t f, uint16_t r); 
    uint16_t Swr(); // only good in loop()
    uint16_t MeasureSwr(uint16_t &f, uint16_t &r);
    enum {HISTORY_PWR = 3, HISTORY_LEN = 1 << HISTORY_PWR, TO_AVERAGE_EXP=5};
    // These four are Voltage, in raw ADC units (as opposed to HISTORY_LEN added)
    uint16_t minFwdToEnableSearch() const { return gMIN_FWD_TO_ENABLE_SEARCH; }
    uint16_t maxFwdRefDuringSwitching() const { return gMAX_FWD_REF_DURING_SWITCHING; }
    void minFwdToEnableSearch(uint16_t);
    void maxFwdRefDuringSwitching(uint16_t);
protected:
    // Model correction for bridge nonlinearity by adding this much to nonzero readings
    enum {DIODE_FORWARD_DROP_MODEL = 41};
    struct History { History():f(0),r(0){}
                uint16_t f; uint16_t r;};
    History history[HISTORY_LEN];
    uint8_t nextHistory;
    long lastReadMsec;
    long startOfTunePower;
    uint16_t triggerSearchSwr;
    uint16_t stopSearchSwr;
    bool activeNow;
    uint16_t gMAX_FWD_REF_DURING_SWITCHING = MAX_FWD_REF_DURING_SWITCHING;
    uint16_t gMIN_FWD_TO_ENABLE_SEARCH = MIN_FWD_TO_ENABLE_SEARCH;
};

class SwrSearch { // class to manage searching through L/C for good SWR
public:
    static bool findMinSwr(Relays::CapRelayState capState, // returns true on found something
            uint8_t minCidx, uint8_t maxCidx, uint8_t incC,
            uint16_t minLidx, uint16_t maxLidx, uint16_t incL);

    bool loop(unsigned long now);
protected:
#if USE_RELAYS > 0
    static bool findMinSwrInternal(Relays::CapRelayState capState, 
        // C is searched from high to low
        uint8_t minCidx, uint8_t maxCidx, uint8_t incC,
        // L is searched from low to high
        uint16_t minLidx, uint16_t maxLidx, uint16_t incL
        );
#endif
};

class LCsavedSettings : public Print
{   // class to manage save/restore of LC settings in daughterboard EEPROM
    /* "EEPROM" in this class refers to the 25LC512 EEPROM IC on the daughterboard, 
    ** not the EEPROM built into the Atmega328. */
public:
    LCsavedSettings() : printRemaining(0), toPrint(0)
    {}
    /* The 25LC512 eeprom has 64K bytes and is byte addressable. 0x0000 through 0xffff.
    ** This class puts an index at 0x0100.
    ** 
    ** The structure of the saved settings is two-level.
    ** There is an array of IndexEntry structures at address 0x100
    ** Memory cells below address 0x100 are currently unused.
    **
    ** The updateIndex() method will use memory cells to the end at 0xffff
    */
    struct IndexEntry {
        uint16_t addr; // updateIndex allocates each table to start on a 128 byte page boundary. 
        uint16_t frequencyStartHalfKhz; // these are NOT indexed in sort order. must instead search linearly
        uint8_t numEntries; // a table may have at most 255
        uint8_t frequencyIncrement; // a table is equal-spaced in frequency. zero is not useful here
        bool isValid() const 
        { return (addr != 0) && (frequencyStartHalfKhz != UNUSED_FREQUENCY) && (frequencyIncrement!=0);}
    };
    /* Each IndexEntry points to an array of TableEntry's. These start at EEPROM address FIRST_TABLE
    ** which is defined to be the end of the (fixed length Index (array of IndexEntry)*/ 
    struct TableEntry {// each entry in the table
        uint8_t cIdx; // through 255
        unsigned lIdx : 9;  // through 512, but only up to 258 used
        unsigned reserved: 6;
        unsigned char onLoad: 1; // top bit is load/gen
        TableEntry() : cIdx(0), lIdx(0), reserved(0), onLoad(0){}
        bool operator != (const TableEntry &other)
        {   return cIdx != other.cIdx || lIdx != other.lIdx || onLoad != other.onLoad;  }
        bool operator == (const TableEntry &other)
        {   return !operator != (other);}
    };
    static_assert(sizeof(TableEntry) == 3, "declare table entry of 3 bytes");

    enum {INDEX_START_ADDR = 0x0100, INDEX_LENGTH = 0x200, // 512 byte index starting at address 256
            NUM_INDEX_ENTRIES = INDEX_LENGTH / sizeof(IndexEntry),
            // at 6 bytes per entry, there is room for 85 entries
            FIRST_TABLE = INDEX_START_ADDR + INDEX_LENGTH, // IndexEntry's start at 512+256 = 0x300
            UNUSED_FREQUENCY = 0xffff,
    };

    /* Capacity
    ** If an index refers to a table with the maximum 255 entries, each of 3 bytes, that consumes
    ** 765 bytes of EEPROM. Doing so runs out of EEPROM at ABOUT the maximum 85 index entries.
    ** Smaller numbers of table entries will run out of index space before eeprom cells*/

    //find the IndexEntry and TableEntry corresponding to freqHalfHz. 
    uint8_t lookup(uint16_t freqHalfHz, IndexEntry &, TableEntry &) const; 
    uint16_t findUnusedBlock(uint8_t numEntries) const;
    uint8_t dumpTableEntry(uint8_t index, uint8_t i, char *buf, uint8_t len);
    bool removeIndexEntry(uint8_t index)const;
    // fill in everything but addr and updateIndex fills that one in, else returns false
    uint8_t updateIndex(IndexEntry &ie)const;
    bool getIndexEntry(uint8_t, IndexEntry &ie)const;
    bool updateTableEntries(uint8_t index, uint8_t firstToUpdate, const TableEntry*, uint8_t numTableEntries)const;
    bool updateTableEntry(uint16_t freqHalfHz, const TableEntry &te);
protected:
    // next three support Print base class
    size_t write(uint8_t c) override;
    char *toPrint;
    uint8_t printRemaining;
};
const SPISettings EepromSPI::settings = SPISettings(10000000, MSBFIRST, SPI_MODE0);
const int EepromSPI::MEM512_SELECT_PIN = ::MEM512_SELECT_PIN;    

// port extender support either includes the LCD or (for test purposes only) does not include the LCD.
#if USE_LCD > 0
typedef PortExtender<LiquidCrystal> ExtenderSupport;
#elif USE_EXTENDER > 0
class NoLcdPortExtender {
protected:
    virtual void write4bits(uint8_t){};
    void pulseEnable(){} // required by the template, but unused.
};
typedef PortExtender<NoLcdPortExtender> ExtenderSupport;
#endif

#if USE_LCD > 0 || USE_EXTENDER > 0
template <> const SPISettings ExtenderSupport::settings = SPISettings(8000000, MSBFIRST, SPI_MODE0);
template <> const int ExtenderSupport::EXTENDER_CS_PIN = ::EXTENDER_CS_PIN;
#endif

namespace {
#if USE_LCD > 0
    ExtenderSupport extenderLcd(LCD_RS_PIN, LCD_RW_PIN, LCD_E_PIN, 
        /* a bit ugly. we have modified the LCD driver to be subclassed for
        ** writing data bits. This means these final four pins
        ** are necessary only to present to the (unmodified) constructor
        ** that sets up 4bit write mode. That constructor sets pinMode on all of
        ** those pin numbers, so we just repeat the real one,
        ** LCD_E_PIN four times so that code sets pinMode on an actual pin.*/
        LCD_E_PIN, LCD_E_PIN, LCD_E_PIN, LCD_E_PIN);

#elif USE_EXTENDER > 0 
    // software support for the extender w/o LCD. only useful for testing
    ExtenderSupport extenderLcd;
#endif
#if USE_RFM69 > 0
    RadioRFM69 radio;
#endif
#if USE_EEPROM > 0
    EepromSPI eepromSPI;
#endif
#if USE_RELAYS > 0
    Relays relays;
#endif
    FreqOver4 freqOver4;
    SwrSearch swrSearch;
    LCsavedSettings savedLCsettings;
    Buzzer buzzer;
    RadioConfiguration radioConfiguration;
    MeterPWM timer2Pwm;   
    ForwardReflectedVoltage bridge;
    unsigned long LastWakeupTime;
    unsigned long InitialAwakeTime;

    bool sleepEnabled = true;

#if SERIAL_DEBUG > 0
    volatile bool int0Happened;
#endif
    void OnInt0()
    { 
#if SERIAL_DEBUG > 0
        int0Happened = true;
#endif
    }
    void SleepCPU();
    uint16_t hexToUint(const char *p);
    void getTwoInts(const char *cmd, int cmdLen, uint16_t &i1, uint16_t &i2);
    uint8_t readAllButtons();
#if USE_LCD > 0 || USE_EXTENDER > 0
    void OnPowerDown();
    void OnLDownButton(uint8_t);
    void OnTuneButton(uint8_t);
    void OnLUpButton(uint8_t);
    void OnCUpButton(uint8_t);
#endif
    void OnCDownButton(uint8_t);
    void OnAntButton(uint8_t);
    void OnModeButton(uint8_t);
    uint16_t Eeprom328pAddr(uint16_t offset) { return RadioConfiguration::TotalEpromUsed() + offset; }
}

static int printStatusMsec = 1000;
static void printStatus()
{
    Serial.println(F("\r\n\r\nMFJ998 by W5XD (rev5)"));
    Serial.print(F("Radio: Node "));
    Serial.print(radioConfiguration.NodeId(), DEC);
    Serial.print(F(" on network "));
    Serial.print(radioConfiguration.NetworkId(), DEC);
    Serial.print(F(" frequency "));
    Serial.print(radioConfiguration.FrequencyBandId(), DEC);
    Serial.print(". Radio on ");
    Serial.print(radio.getFrequency() / 1000);
    Serial.println(" KHz");
}

// setup() ******************************************************************************
void setup() 
{   // FIRST put the various SPI slave selects in their correct state ASAP
    digitalWrite(RFM69_SELECT_PIN, HIGH);
    pinMode(RFM69_SELECT_PIN, OUTPUT);
    digitalWrite(EXTENDER_CS_PIN, HIGH);
    pinMode(EXTENDER_CS_PIN, OUTPUT);
    pinMode(RELAYS_STROBE_PIN, OUTPUT);
    pinMode(MFJ998_CLOCK_AND_MODE_PIN, INPUT_PULLUP);
    // The time delay between setting those selects above and starting
    // SPI on, for example, the extender, is IMPORTANT.

#if USE_EEPROM > 0
    eepromSPI.init();
#endif

#if USING_SPI
    SPI.begin();
#endif

#if USE_LCD > 0 || USE_EXTENDER > 0
    extenderLcd.setup();
#endif

#if USE_RELAYS > 0
    relays.setup();
#endif

#if USE_LCD > 0
    extenderLcd.begin(LCD_WIDTH,2);
    extenderLcd.print(F("MFJ998 by W5XD"));
#endif
    Serial.begin(38400);

#if SERIAL_DEBUG > 0
    const char *key = radioConfiguration.EncryptionKey();
    if (radioConfiguration.encrypted())
    {
        Serial.print(" key ");
        for (unsigned j = 0; j < radioConfiguration.ENCRYPT_KEY_LENGTH; j++)
        {
    	    unsigned c = key[j];
    	    if (isprint(c))
    		    Serial.print((char)c);
    	    else
    	    {
    		    Serial.print("\\");
    		    Serial.print(int(c));
    	    }
        }
    }
    Serial.println();
#endif

    buzzer = false; // turn it off

    freqOver4.setup();

#if USE_RFM69 > 0
    radio.setup();
    Serial.print(F(" Radio on "));
    Serial.print(radio.getFrequency() / 1000);
    Serial.println(F(" KHz"));
#endif

    timer2Pwm.setup();
    bridge.setup();

#if USE_LCD > 0 || USE_EXTENDER > 0
    int intNum = digitalPinToInterrupt(WAKEUP_PIN);
    attachInterrupt(intNum, OnInt0, FALLING);
    /* if the powerdown signal happened as we were starting up,
    ** clear it now. I never observed this, but it would
    ** be very difficult to debug as the box would just shut itself down on power up. */
    extenderLcd.getCapturedInterruptMask();
#endif

    uint16_t freqCalibration;
    EEPROM.get(Eeprom328pAddr(FREQ_CALI_OFFSET), freqCalibration);
    freqOver4.setCalibration(freqCalibration);

    InitialAwakeTime = millis() + INITIAL_AWAKE_MSEC_ON_POWERUP;
}

static bool processCommandString(const char *pHost, unsigned short count)
{
    static const char SENDMESSAGETONODE[] = "SendMessageToNode";
    if (strncmp(pHost, SENDMESSAGETONODE, sizeof(SENDMESSAGETONODE) - 1) == 0)
    {	// send a message to a node now.
#if USE_RFM69 > 0
        pHost += sizeof(SENDMESSAGETONODE) - 1;
        count -= sizeof(SENDMESSAGETONODE);
        if (*pHost++ == ' ')
        {
            unsigned nodeId = 0;
            for (;; pHost++)
            {
                if (*pHost >= '0' && *pHost <= '9')
                {
                    nodeId *= 10;
                    nodeId += *pHost - '0';
                    count -= 1;
                }
                else
                    break;
            }
            if ((nodeId >= 0) && *pHost++ == ' ')
            {
                radio.send(nodeId, pHost, --count);
                Serial.print(SENDMESSAGETONODE);
                Serial.print(" ");
                Serial.println(nodeId, DEC);
            }
        }
#endif
        return true;
    }
    else if (strncmp(pHost, "SET ", 4) == 0)
    {
        char *lPtr;
        if (bridge.OkToSwitchRelays())
        {
            lPtr = strstr(pHost, " L=");
            if (lPtr)
            {
                uint16_t lidx = atoi(lPtr+3);
#if USE_RELAYS > 0
                relays.setLindex(lidx);
                relays.transfer();
#endif
#if SERIAL_DEBUG > 0
                Serial.print("setting L="); Serial.println(lidx);
#endif
            }
            lPtr = strstr(pHost, " P=");
            if (lPtr)
            {
                uint8_t pidx = atoi(lPtr+3);
#if USE_RELAYS > 0
                relays.setCapRelayState((Relays::CapRelayState)pidx);
                relays.transfer();
#endif
#if SERIAL_DEBUG > 0
                Serial.print("setting P="); Serial.println((uint16_t)pidx);
#endif
            }
            lPtr = strstr(pHost, " C=");
            if (lPtr)
            {
                uint8_t cidx = atoi(lPtr+3);
#if USE_RELAYS > 0
                relays.setCindex(cidx);
                relays.transfer();
#endif
#if SERIAL_DEBUG > 0
                Serial.print("setting C="); Serial.println((uint16_t)cidx);
#endif
            }
            lPtr = strstr(pHost, " K1=");
            if (lPtr)
            {
                uint8_t k1 = atoi(lPtr+4);
#if USE_RELAYS > 0
                if (k1 == 0)
                    relays.setK1Off();
                else
                    relays.setK1On();
                relays.transfer();
#endif
#if SERIAL_DEBUG > 0
                Serial.print("setting K1="); Serial.println((uint16_t)k1);
#endif
            }
        }
#if USE_RFM69 > 0
        lPtr = strstr(pHost, " T=");
        if (lPtr)
        {
            uint16_t interval = atoi(lPtr+3);
            radio.setTelemetryMsec(interval);
#if SERIAL_DEBUG > 0
            Serial.print("setting T="); Serial.println((uint16_t)interval);
#endif    
        } 
#endif
        lPtr = strstr(pHost, "TSWR=");
        if (lPtr)
        {
            uint16_t triggerSwr = atoi(lPtr+5);
            bridge.setTriggerSwr(triggerSwr);
#if SERIAL_DEBUG > 0
            Serial.print(F("TSWR=0x")); Serial.println(triggerSwr, HEX);
#endif
        }
        lPtr = strstr(pHost, "SSWR=");
        if (lPtr)
        {
            uint16_t stopSwr = atoi(lPtr+5);
            bridge.setStopSearchSwr(stopSwr);
#if SERIAL_DEBUG > 0
            Serial.print(F("SSWR=0x")); Serial.println(stopSwr, HEX);
#endif        
        }
        lPtr = strstr(pHost, " S=");
        if (lPtr)
            sleepEnabled = atoi(lPtr+3) != 0;

        lPtr = strstr(pHost, "MXSW=");
        if (lPtr)
        {
            uint16_t maxSwitch = atoi(lPtr + 5);
            bridge.maxFwdRefDuringSwitching(maxSwitch);
#if SERIAL_DEBUG > 0
            Serial.print(F("MXSW=")); Serial.println(maxSwitch);
#endif
        }
        lPtr = strstr(pHost, "MNSR=");
        if (lPtr)
        {
            uint16_t minSearch = atoi(lPtr + 5);
            bridge.minFwdToEnableSearch(minSearch);
#if SERIAL_DEBUG > 0
            Serial.print(F("MNSR=")); Serial.println(minSearch);
#endif
        }
        return true;
    } else if (strncmp(pHost, "GET", 3) == 0)
    {
#if USE_RFM69
        radio.sendStatusPacket(bridge.TriggerSwr(), bridge.StopSearchSwr());
#endif
        return true;
    } else if (strncmp(pHost, "SEARCH ", 7) == 0)
    {
        uint8_t minC = 0;
        uint8_t maxC = 0xff;
        uint8_t incC = 1;
        uint16_t minL = 0;
        uint16_t maxL = Relays::MAX_LINDEX;
        uint16_t incL = 1;
        Relays::CapRelayState cstate = Relays::CapRelayState::STATE_C_GENERATOR;
        const char *p = strstr(pHost, "C1=");
        if (p)
            minC = atoi(p+3);
        p = strstr(pHost, "C2=");
        if (p)
            maxC = atoi(p+3);
        p = strstr(pHost, "C3=");
        if (p)
            incC = atoi(p+3);
        p = strstr(pHost, "L1=");
        if (p)
            minL = atoi(p+3);
        p = strstr(pHost, "L2=");
        if (p)
            maxL = atoi(p+3);
        p = strstr(pHost, "L3=");
        if (p)
            incL = atoi(p+3);
        p = strstr(pHost,"P=");
        if (p)
            cstate = atoi(p+2) == 1 ? Relays::CapRelayState::STATE_C_LOAD : cstate;
        if (incC == 0)
            incC = 1;
        if (incL == 0)
            incL = 1;
        SwrSearch::findMinSwr(cstate, minC, maxC, incC, minL, maxL, incL);
        return true;
    } 
#if USE_EEPROM > 0
    else if (strncmp(pHost, "EEPROM INDEX", 12) == 0)
    {   // creates an entry in frequency-to-LC EEPROM index
        LCsavedSettings::IndexEntry ieUpdate={};
        ieUpdate.addr = -1; // escape isValid trouble
        ieUpdate.frequencyStartHalfKhz = LCsavedSettings::UNUSED_FREQUENCY;
        if (strstr(pHost, " ERASE"))
        {
            eepromSPI.setWriteEnable();
            eepromSPI.chipErase();
        }
        const char *p = strstr(pHost, "f=");
        if (p)
            ieUpdate.frequencyStartHalfKhz = atoi(p+2);

        p = strstr(pHost, "REMOVE");
        bool erase = (p != 0);
        if (erase)
        {
            LCsavedSettings::TableEntry te;
            auto idx = savedLCsettings.lookup(ieUpdate.frequencyStartHalfKhz, ieUpdate, te);
            if (idx != 0xff)
                savedLCsettings.removeIndexEntry(idx);
            return true;
        }
        p = strstr(pHost, "n=");
        if (p)
            ieUpdate.numEntries = atoi(p+2);
        p = strstr(pHost, "i=");
        if (p)
            ieUpdate.frequencyIncrement = atoi(p+2);
        if (ieUpdate.isValid())
        {
            auto ret = savedLCsettings.updateIndex(ieUpdate);
#if 0//SERIAL_DEBUG > 0
            Serial.print(F("EEPROM INDEX n=")); Serial.print((int)ieUpdate.numEntries);
            Serial.print(F(" f=")); Serial.print(ieUpdate.frequencyStartHalfKhz);
            Serial.print(F(" i=")); Serial.print((int)ieUpdate.frequencyIncrement);
            Serial.println(ret != -1 ? " OK" : " FAIL");
#endif
        }
        return true;
    } else if (strncmp(pHost, "EEPROM TABLE", 12) == 0)
    {   // overwrites a table entry for a given frequency. The entry must exist.
        LCsavedSettings::IndexEntry ieUpdate;
        LCsavedSettings::TableEntry te={};
        const char *p = strstr(pHost, "f="); // frequency in raw units
        if (p)
        {
            uint16_t fHalfHz = atoi(p+2);
            if (fHalfHz)
            {
                p = strstr(pHost, "C="); // capacitor index
                if (p)
                {
                    te.cIdx = atoi(p+2);
                    p = strstr(pHost, "L=");    // inductor index
                    if (p)
                    {
                        te.lIdx = atoi(p+2);
                        p = strstr(pHost, "P=");    // load/generator bit
                        if (p)
                        {
                            te.onLoad = (atoi(p+2) == 1);
                            bool res = savedLCsettings.updateTableEntry(fHalfHz, te);
#if 0//SERIAL_DEBUG > 0
                            Serial.print(F("table update for f=")); Serial.print(fHalfHz);
                            Serial.print(F(" C=")); Serial.print((int)te.cIdx);
                            Serial.print(F(" L=")); Serial.print((int)te.lIdx);
                            Serial.print(F(" P=")); Serial.print(te.onLoad ? "1 " : "2 ");
                            Serial.println(res ? "OK" : "FAILED");
#endif
                        }
                    }
                }
            }
        }
        return true;
    }
    else if (strncmp(pHost, "EEPROM DUMP ", 12) == 0)
    {
        if (strstr(pHost, "INDEX"))
        {
            for (uint8_t idx=0; ;idx++)
            {
                LCsavedSettings::IndexEntry ie;
                if (!savedLCsettings.getIndexEntry(idx, ie))
                    break;
                if (ie.isValid())
                {
                    Serial.print(F("index=")); Serial.print((int)idx);
                    Serial.print(F(" addr=0x")); Serial.print(ie.addr, HEX);
                    Serial.print(F(" f=")); Serial.print(ie.frequencyStartHalfKhz);
                    Serial.print(F(" n=")); Serial.print(ie.numEntries);
                    Serial.print(F(" I=")); Serial.println((int)ie.frequencyIncrement);
                }
            }
        }
        uint16_t freqHalfHz = 0;
        const char *p = strstr(pHost, "f=");
        if (p)
            freqHalfHz = atoi(p+2);
        for (uint8_t idx=0; ;idx++)
        {
            LCsavedSettings::IndexEntry ie;
            if (!savedLCsettings.getIndexEntry(idx, ie))
                break;
            if (ie.isValid() && 
                ie.frequencyStartHalfKhz <= freqHalfHz &&
                ie.frequencyStartHalfKhz + (uint16_t)ie.numEntries * (uint16_t)ie.frequencyIncrement > freqHalfHz)
            {
                Serial.print("Index="); Serial.print((int)idx);
                Serial.print(" fStart="); Serial.print(ie.frequencyStartHalfKhz);
                Serial.print(" fInc="); Serial.println((int)ie.frequencyIncrement);
                uint16_t freq = ie.frequencyStartHalfKhz;
                for (uint8_t j = 0; j < ie.numEntries; j++, freq += ie.frequencyIncrement)
                {
                    Serial.print(F(" f=")); Serial.print(freq);
                    Serial.print(F(" "));
                    char buf[16];
                    savedLCsettings.dumpTableEntry(idx, j, buf, sizeof(buf));
                    Serial.println(buf);                    
                }
            }
        }        
        Serial.println(F("EEPROM DUMP done"));
        return true;
    }
#endif

    return false;
}

// loop() *******************************************************************************
void loop() 
{
    static const int SHORTEST_STAYWOKEN_MSEC = 100;
    auto now = millis();
    if (printStatusMsec > 0 && now >= printStatusMsec)
    {
        printStatusMsec = 0;
        printStatus();
    }
    bool canSleep = sleepEnabled && (now > InitialAwakeTime);

    // at wakeup time, these are in priority order
#if USE_LCD > 0 || USE_EXTENDER > 0
    auto intFlags = extenderLcd.getCurrentInterruptMask(); // does NOT clear extender interrupt
    if (intFlags & (1 << ExtenderSupport::PD))
        OnPowerDown(); //powerdown interrupt handler does not return
#endif
    // radio wants an ACK in 30msec, although the no-operation loop() measured under 1000 usec
#if USE_RFM69 > 0
    canSleep &= !radio.loop();
#if PRINT_RADIO_RSSI > 0
    {   // radio in an actual hardware build decays RSSI slowly. it takes
        // seconds to go down after a message received.
        // the library sendACK won't send the ack because of that.
        // these prints show the decay
        static long lastprint;
        if (now - lastprint > 1000)
        {
            lastprint = now;
            Serial.print(F("Radio RSSI: ")); Serial.println(radio.readRSSI());
        }
    }
#endif
#endif

    auto buttonMask = readAllButtons(); // clears extender interrupt as a side effect

    static const long BUTTON_DEBOUNCE_MSEC = 100;
    static long lastButtonOnMsec;
    
    // debounce switch read
    static uint8_t dispatched = 0; // only dispatch a button press once
    if (buttonMask == 0)
    {   // don't see any buttons now
        if (now - lastButtonOnMsec > BUTTON_DEBOUNCE_MSEC)
            dispatched = 0; // and only after there has been no button for 20msec
    }
    else
    {
        static const long REPEAT_MSEC = 110;
        int numButtonsPressed = 0;
        // repeating for button held down only works for a single button. 
        auto mask = buttonMask | dispatched;
        for (int i = 0; i < 8; i++)
            if ((mask & (1 << i)) != 0)
                numButtonsPressed += 1;
        if ((buttonMask > dispatched) || // single button repeats
            (numButtonsPressed == 1 && (now - lastButtonOnMsec > REPEAT_MSEC)))
        {
            // for multi-button presses
            if (buttonMask >= 1u << (uint8_t)FrontPanelButtons::DISPATCH_MODE)
                OnModeButton(buttonMask);
            else if (buttonMask >= 1u << (uint8_t)FrontPanelButtons::DISPATCH_ANT)
                OnAntButton(buttonMask);
            else if (buttonMask >= 1u << (uint8_t)FrontPanelButtons::DISPATCH_CDOWN)
                OnCDownButton(buttonMask);
#if USE_LCD > 0 || USE_EXTENDER > 0
            else if (buttonMask >= 1u << (uint8_t)FrontPanelButtons::DISPATCH_CUP)
                OnCUpButton(buttonMask);
            else if (buttonMask >= 1u << (uint8_t)FrontPanelButtons::DISPATCH_LUP)
                OnLUpButton(buttonMask);
            else if (buttonMask >= 1u << (uint8_t)FrontPanelButtons::DISPATCH_LDOWN)
                OnLDownButton(buttonMask);
            else if (buttonMask >= 1u << (uint8_t)FrontPanelButtons::DISPATCH_TUNE)
                OnTuneButton(buttonMask);
#endif
            dispatched = buttonMask;
            lastButtonOnMsec = now;
        }
    }

    canSleep &= (buttonMask == 0);

    canSleep &= !buzzer.loop();

    canSleep &= !freqOver4.loop();

    canSleep &= !bridge.loop(now);

    canSleep &= !swrSearch.loop(now);

#if USE_RFM69 > 0
    {
        uint16_t interval = radio.getTelemetryMsec();
        if ((interval!=0) && (now - radio.getLastTelemetryMsec() > interval))
        {
            uint16_t f,r;
            bridge.readPowerX8(f,r);
            radio.sendTelemetryPacket(freqOver4.getFreqRaw(),f,r, 
#if USE_RELAYS > 0
                    relays.getCindex(), relays.getLindex(), 
                    static_cast<uint8_t>(relays.getCapRelayState()));
#else   
                    0,0,0);
#endif
        }
    }
#endif

#if (DO_PRINT_FREQ > 0) && (USE_LCD > 0)
    {   // put the freqOver4 and bridge results on the LCD
        static long lastPrint;
        static const int PRINT_INTERVAL_MSEC = 500;
        if (now - lastPrint > PRINT_INTERVAL_MSEC)
        {
            lastPrint = now;
            extenderLcd.home();
            int w = extenderLcd.print("F:");
            auto freq = freqOver4.getFreqKhz();
            w+= extenderLcd.print(freq);
            w+= extenderLcd.print("K p:");
            uint16_t f,r;
            bridge.readPowerX8(f,r);
            w+= extenderLcd.print(f);
            w+= extenderLcd.print(" ");
            w+= extenderLcd.print(r);
            while (w < LCD_WIDTH)
                w+=extenderLcd.print(" ");
        }
    }
#endif

    // serial port commands
    while (Serial.available() > 0)
    {
        canSleep = false; // incoming characters cancel sleep
        static const unsigned CMDBUFLEN = 64;
        static char cmdbuf[CMDBUFLEN+1];
        static uint16_t eepromMemAddress;
        static uint8_t cmdbuflen;
        auto c = Serial.read();
        if (c == '\r' || c == '\n')
        {
            if (cmdbuflen < 1)
            {
                Serial.println(F("Commands: S,F"));
                continue;
            }

            if (!processCommandString(cmdbuf, cmdbuflen))
            {
                switch (toupper(cmdbuf[0]))
                {
                // commands present whether we debug or not
                case 'S': // radio commands all start with S
                    if (radioConfiguration.ApplyCommand(cmdbuf))
                    {
                        Serial.print(cmdbuf);
                        Serial.println(F(" command accepted for radio"));
                    }
                    break;

                case 'F':
                    {
                        if ((cmdbuflen > 1) && cmdbuf[1] == 'R')
                        {   // reset calibration to unity
                            freqOver4.setCalibration(1 << 15);
                            Serial.println(F("Now reading uncalibrated frequency"));
                            break;
                        }
                        uint16_t actual, displayed;
                        getTwoInts(cmdbuf, cmdbuflen, actual, displayed);
                        if ((actual > 0) && (displayed > 0))
                        {
                            uint32_t result = actual;
                            result <<= 15;
                            result /= displayed;
                            uint16_t calibration = result;
                            freqOver4.setCalibration(calibration);
                            EEPROM.put(Eeprom328pAddr(FREQ_CALI_OFFSET), calibration);

                            Serial.print(F("Calibration = "));
                            Serial.println(calibration, HEX);
                        }
                        else
                            Serial.println(F("F [R] | [<actual> <displayed>]"));
                    }
                    break;

#if SERIAL_DEBUG > 0 
// these commands are here more for reference to how to do stuff
// They were added in order to test certain features and mostly are no longer useful.
                case 'P':
                    {
                        uint16_t f,r;
                        bridge.readPowerX8(f,r);
                        uint16_t swr = bridge.Swr();
                        Serial.print(F("FWD: ")); Serial.print(f);
                        Serial.print(F(" REF: ")); Serial.print(r);
                        Serial.print(F(" TSWR: 0x")); Serial.print(bridge.TriggerSwr(), HEX);
                        Serial.print(F(" SSWR: 0x")); Serial.print(bridge.StopSearchSwr(), HEX);
                        Serial.print(F(" SWR: 0x")); Serial.println(swr,HEX);
                    }
                    break;

#if USE_EEPROM > 0 && EEPROM_TEST_COMMANDS > 0
                // eeprom tests
                case 'A':
                    {
                        uint8_t status = eepromSPI.status();
                        Serial.print(F("chip status = 0x")); Serial.println((unsigned int)status, HEX);
                        if (cmdbuflen == 1)
                        {
                            Serial.print(F("address: 0x"));
                            Serial.println(eepromMemAddress, HEX);
                        }
                        else {
                            auto adr = hexToUint(&cmdbuf[1]);
                            Serial.print(F("Setting addr: 0x"));
                            Serial.println(adr, HEX);
                            eepromMemAddress = adr;
                        }
                    }
                    break;   

                case 'W':
                    if (cmdbuflen > 1)
                    {
                        uint16_t v = hexToUint(&cmdbuf[1]);
                        Serial.print(F("Writing 0x"));
                        Serial.print(v, HEX);
                        Serial.print(F(" to 0x"));
                        Serial.println(eepromMemAddress,HEX);
                        if (!eepromSPI.waitReadyForWrite())
                            Serial.println(F("EEPROM NOT READY!"));
                        else
                            eepromSPI.write(eepromMemAddress, reinterpret_cast<const uint8_t*>(&v), 2);        
                    }
                    break;

                case 'G':
                    {
                        Serial.print(F("Reading EEPROM address 0x"));
                        Serial.println(eepromMemAddress, HEX);
                        uint16_t r(0);

                        eepromSPI.read(eepromMemAddress, reinterpret_cast<uint8_t*>(&r), 2);
                        Serial.print(" got: 0x");
                        Serial.println(r,HEX);

                    }
                    break;

                case 'L':
                    {
                        uint16_t f = 0;
                        const char *p = strstr(cmdbuf, "f=");
                            if (p)
                                f = atoi(p+2);
                        LCsavedSettings::TableEntry te;
                        LCsavedSettings::IndexEntry ie;
                        auto idx = savedLCsettings.lookup(f, ie, te);
                    }
                    break;

#if 0 // the eeprom write test eats a lot of Arduino memory
                case 'C':
                    eepromSPI.testWritePageBoundaries();
                    break;
#endif
#endif

#if (USE_RELAYS > 0)
                case 'R':
                    {
                        static bool allOff = true;
                        static uint8_t relay;
                        allOff = !allOff;
                        Serial.print(F("Relay setting "));
                        Serial.println(allOff ? "all off OFF" : "ON");
                        if (allOff)
                            relays.set(0);
                        else
                        {   // turn on one relay at a time
                            uint32_t r = 1L << (uint32_t)relay;
                            Serial.print(F("Relay OUT"));
                            Serial.print(relay+1);
                            Serial.print(", 0x");
                            Serial.println(r, HEX);
                            relay += 1;
                            if (relay >= 22)
                                relay = 0;
                            relays.set(r);
                        }
                    }
                    break;
#endif

                case 'B':
                    if (cmdbuflen > 1)
                    {
                        uint16_t v = atoi(&cmdbuf[1]);
                        if (v > 0)
                            buzzer.buzz(v);
                    }
                    else
                    {
                        buzzer = !buzzer;
                        Serial.print(F("BUZZ_PIN is now "));
                        Serial.println(buzzer.isBuzzing());
                    }
                    break;


#if USE_LCD > 0 || USE_EXTENDER > 0
                case 'X':
                    {
                        uint8_t registers[0xa];
                        extenderLcd.getRegisters(registers);
                        Serial.println(F("Registers:"));
                        for (int i = 0; i < sizeof(registers); i++)
                        {Serial.print("  0x"); Serial.println(registers[i], HEX);}
                    }
                    break;
#endif
                case 'M':
                    {   // pwm power meter output test
                        if ((cmdbuflen == 2) && cmdbuf[1] == 'X')
                        {
                            for (int j = 0; j < 5; j++)
                            {
                                for (int i = 0; i < 255; i++)
                                {
                                    timer2Pwm.setMeters(i, 255 - i);
                                    delay(10);
                                }
                                for (int i = 255; i >= 0; i--)
                                {
                                    timer2Pwm.setMeters(i, 255 - i);
                                    delay(10);
                                } 
                            }
                            timer2Pwm.setMeters(0,0);
                            break;
                        }
                        uint16_t fwd;
                        uint16_t ref;
                        getTwoInts(cmdbuf, cmdbuflen, fwd, ref);
                        Serial.print(F("Setting analog meters to fwd="));
                        Serial.print((int) fwd);
                        Serial.print(F(" ref="));
                        Serial.println((int) ref);
                        timer2Pwm.setMeters(fwd, ref);
                    }
                    break;
#endif
                case 'Q':
                    printStatus();
                    break;

                default:
                    break;
                }
            }
            cmdbuflen = 0;
        }
        else
        {
            if (cmdbuflen < CMDBUFLEN)
            {
                cmdbuf[cmdbuflen++] = c;
                cmdbuf[cmdbuflen] = 0;
            }
        }
    }

    if (canSleep)
    {
        if (now - LastWakeupTime > SHORTEST_STAYWOKEN_MSEC)
        {
            Serial.println(F("Sleeping"));
            Serial.flush();
            bridge.sleep();
            SleepCPU();
            // the millis() clock didn't run while we asleep,
            // so don't really need this.
            freqOver4.wakeup();
            LastWakeupTime = millis();
            Serial.println(F("woke up"));
        }
    }
    else
        LastWakeupTime = millis();
}

namespace {
    void SleepCPU()
    {
        /* the purpose of sleeping the CPU is to get rid of
        ** any RF noise it might generate. If we're transmitting,
        ** we can't hear it. But the logic above tries to get us to SleepCPU()
        ** when we think the antenna is being used for RX. */
#if USE_LCD > 0 || USE_EXTENDER > 0
        extenderLcd.setLCDpinsAsInputs();
#endif

        timer2Pwm.sleep();

#if SERIAL_DEBUG > 0
        bool WakeupLow = false;
        int0Happened = false;
#endif

#if defined(__AVR_ATmega328P__)
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        cli();
        // http://www.nongnu.org/avr-libc/user-manual/group__avr__sleep.html
        if (PIND & (1 << PD2)) //digitalRead(WAKEUP_PIN) == HIGH
        {
            EIFR |= 1 << INTF0; // clear any pending INT0
            EIMSK |= 1 << INT0;  // enable INT0
            power_all_disable(); // turn off everything
            sleep_enable();
            // next two instructions, sei() and sleep_cpu() MUST be consecutive to guarantee
            // that a pending interrupts wakes us right back up
            sei(); sleep_cpu(); // the one instruction that SEI guarantees
            sleep_disable();
            power_all_enable();
        }
#if SERIAL_DEBUG > 0
        else WakeupLow = true;
#endif            
        EIMSK &= ~(1 << INT0); // Disable INT0 while awake
        sei();
#endif

#if SERIAL_DEBUG > 1
        if (WakeupLow)
            Serial.println("Did not sleep cuz wakeup");
        if (int0Happened)
        {
            Serial.print("bh: "); Serial.print(int0Happened ? "yes" : "no");
#if USE_LCD > 0 || USE_EXTENDER > 0
            Serial.print(", pins: 0x"); Serial.print((int)extenderLcd.getPins(),HEX);
            Serial.print(" mask: 0x"); Serial.println((int)extenderLcd.getCapturedInterruptMask(),HEX);
#else
            Serial.println();
#endif
        }
#endif
    }
}

namespace {
    uint8_t readAllButtons()
    {
        // this daughterboard has the 7 button inputs scattered around a bit.
        // Gather them all up into buttonMask.
        uint8_t buttonMask = 0;

#if USE_LCD > 0 || USE_EXTENDER > 0
        // The extender has four of the buttons
        // read the switches
        auto inputState2 = extenderLcd.getPins();
        if (0 == (inputState2 & ( 1 << ExtenderSupport::BUTTON_LDOWN)))
            buttonMask |= 1u << (uint8_t)FrontPanelButtons::DISPATCH_LDOWN;
        if (0 == (inputState2 & (  1 << ExtenderSupport::BUTTON_TUNE)))
            buttonMask |= 1u << (uint8_t)FrontPanelButtons::DISPATCH_TUNE;
        if (0 == (inputState2 & (  1 << ExtenderSupport::BUTTON_LUP)))
            buttonMask |= 1u << (uint8_t)FrontPanelButtons::DISPATCH_LUP;
        if (0 == (inputState2 & (  1 << ExtenderSupport::BUTTON_CUP)))
            buttonMask |= 1u << (uint8_t)FrontPanelButtons::DISPATCH_CUP;
#endif

    // There are two pins shared between the LCD display (as outputs)
    // and front panel buttons (as inputs).
    // Both are on PORTD. This code snapshots the state of PORTD (data direction and pullups)
    // reconfigures as input with pullup
    // reads the pins and restores so the LCD display continues to work.
#if defined(__AVR_ATmega328P__)
        {
            cli();
            uint8_t portDDR = DDRD;
            uint8_t portD = PORTD;
            PORTD = portD | ((1 << PD7) | (1 << PD4)); // PD7 and PD4 activate pullups
            DDRD = portDDR & ~((1 << PD7) | (1 << PD4)); // PD7 and PD4 now inputs
            __asm__("nop");// give the button time to pull its port pin down
            __asm__("nop");// ditto
            uint8_t portDinput = PIND;
            // restore port D for LCD use
            PORTD = portD;
            DDRD = portDDR;
            sei();
            // two buttons on PORTD
            if (0 == (portDinput & (1 << PD7)))
                buttonMask |= 1u << (uint8_t)FrontPanelButtons::DISPATCH_CDOWN;
            if (0 == (portDinput & (1 << PD4)))
                buttonMask |= 1u << (uint8_t)FrontPanelButtons::DISPATCH_ANT;
        }
#endif
        // one button on different port
        if (digitalRead(MODE_BUTTON_PIN) == LOW)
            buttonMask |= 1 << (uint8_t)FrontPanelButtons::DISPATCH_MODE;    
        return buttonMask;
    }    
    
    void displayCandPonLCD()
    {
#if USE_RELAYS > 0 && USE_LCD > 0
        extenderLcd.setCursor(0,1);
        int w = extenderLcd.print(F("Cidx: "));
        w += extenderLcd.print(relays.getCindex());
        w += extenderLcd.print(" P:");
        w += extenderLcd.print((int)relays.getCapRelayState());
        while (w < LCD_WIDTH)
            w += extenderLcd.print(" ");               
#endif
    }
    
    void displayLonLCD()
    {
#if USE_RELAYS > 0 && USE_LCD > 0
        extenderLcd.setCursor(0,1);
        int w = extenderLcd.print("Lidx: ");
        w += extenderLcd.print(relays.getLindex());
        while (w < LCD_WIDTH)
            w += extenderLcd.print(" ");               
#endif
    }


#if USE_LCD > 0 || USE_EXTENDER > 0
    void OnPowerDown()
    {   // according to my 'scope measurements, you get
        // about 15msec of 5VDC from this interrupt to death
#if SERIAL_DEBUG > 0
        Serial.println(F("OnPowerDown"));
#endif
        while(true);    // do not return
    }

    void OnLDownButton(uint8_t mask)
    {
#if SERIAL_DEBUG > 0
        Serial.println(F("OnLDownButton"));
#endif
#if USE_RELAYS > 0
        if (bridge.OkToSwitchRelays())
        {
            auto cur = relays.getLindex();
            if (cur > 0)
            {
                relays.setLindex(cur-1);
                relays.transfer();
            }
        }
#endif
        displayLonLCD();
    }

    void OnTuneButton(uint8_t mask)
    {
#if SERIAL_DEBUG > 0
        Serial.println(F("OnTuneButton"));
#endif
    }
    void OnLUpButton(uint8_t mask)
    {
#if SERIAL_DEBUG > 0
        Serial.println("OnLUpButton");
#endif
#if USE_RELAYS > 0
        if (bridge.OkToSwitchRelays())
        {
            auto cur = relays.getLindex();
            if (cur < relays.MAX_LINDEX)
            {
                if (relays.setLindex(cur+1))
                    relays.transfer();
            }
        }
#endif
        displayLonLCD();
    }


    void OnCUpButton(uint8_t mask)
    {
#if SERIAL_DEBUG > 0
        Serial.println(F("OnCUpButton"));
#endif
#if USE_RELAYS > 0
        if (bridge.OkToSwitchRelays())
        {
            auto cur = relays.getCindex();
            if (cur < 255)
            {
                if (relays.setCindex(cur+1))
                    relays.transfer();
            }
        }
#endif
        displayCandPonLCD();
    }
#endif
    void OnCDownButton(uint8_t mask)
    {
#if USE_RELAYS > 0
        if (bridge.OkToSwitchRelays())
        {
            if ((mask & (1u << (uint8_t)FrontPanelButtons::DISPATCH_CUP)) != 0)
            {   // if CUP is also on, then toggle LOAD/GENERATOR
                auto cur = relays.getCapRelayState();
                switch (cur)
                {
                case Relays::CapRelayState::STATE_C_LOAD:
                    cur = Relays::CapRelayState::STATE_C_GENERATOR;
                    break;
                default:
                    cur = Relays::CapRelayState::STATE_C_LOAD;
                    break;
                }
                relays.setCapRelayState(cur);
                relays.transfer();
            }
            else
            {
                auto cur = relays.getCindex();
                if (cur > 0)
                {
                    relays.setCindex(cur-1);
                    relays.transfer();
                }
            }
        }
#endif
        displayCandPonLCD();
#if SERIAL_DEBUG > 0
        Serial.println(F("OnCDownButton"));
#endif
    }
    void OnAntButton(uint8_t mask)
    {
 #if SERIAL_DEBUG > 0
        Serial.println(F("OnAntButton"));
#endif
   }
    void OnModeButton(uint8_t mask)
    {
#if SERIAL_DEBUG > 0
        Serial.println(F("OnModeButton"));
#endif
    }

    uint16_t hexToUint(const char *p)
    {
        uint16_t adr = 0;
        for (; *p; p += 1)
        {
            adr <<= 4;
            char q = toupper(*p);
            if (isdigit(*p))
                adr |= (unsigned char)(*p - '0');
            else if (q >= 'A' && q <= 'F')
                adr |= (unsigned char) (10 + q - 'A');
        }
        return adr;
    }

    void getTwoInts(const char *cmdbuf, int cmdbuflen, uint16_t &i1, uint16_t &i2)
    {
        i1 = 0;
        i2 = 0;
        int i = 0;
        bool haveDigit = false;
        while (++i < cmdbuflen)
        {
            if (isspace(cmdbuf[i]))
            {
                if (haveDigit)
                    break;
                else
                    continue;
            }
            else if (isdigit(cmdbuf[i]))
            {
                haveDigit = true;
                i1 *= 10;
                i1 += cmdbuf[i] - '0';
            }
            else
                break;
        }
        haveDigit = false;
        while (++i < cmdbuflen)
        {
            if (isspace(cmdbuf[i]))
            {
                if (haveDigit)
                    break;
                else
                    continue;
            }
            else if (isdigit(cmdbuf[i]))
            {
                haveDigit = true;
                i2 *= 10;
                i2 += cmdbuf[i] - '0';
            }
            else
                break;
        }    
    }

// class SwrSearch *****************************************************************
#if USE_RELAYS > 0
    /* SWR search routine. Place the capacitors on either
    ** GENERATOR or LOAD and search across both C and L for
    ** the lowest SWR.*/
    bool SwrSearch::findMinSwrInternal(Relays::CapRelayState capState, 
        // C is searched from high to low
        uint8_t minCidx, uint8_t maxCidx, uint8_t incC,
        // L is searched from low to high
        uint16_t minLidx, uint16_t maxLidx, uint16_t incL
        )
    {
#if USE_RFM69 > 0
        uint16_t interval = radio.getTelemetryMsec();
#endif
        uint16_t bestSwr = 0xffff;
        uint16_t lowestR = 0xffff;
        uint8_t cIdxAtMin = 0;
        uint16_t lIdxAtMin = 0;
        relays.setCapRelayState(capState);
        uint16_t f, r;
        bridge.acquirePowerNow(f,r);
        for (uint16_t lIdx = minLidx; lIdx <= maxLidx; lIdx += incL)
        {
            int16_t cIdx;
            for (cIdx = maxCidx; 
                cIdx >= static_cast<int16_t>(minCidx); 
                cIdx -= incC)
            {
                if (!relays.setCindex(cIdx)) 
                    return false;
                if (!relays.setLindex(lIdx))
                    return false;
                if (!bridge.OkToSwitchRelays(f,r))
                    return false;
                if (f < bridge.minFwdToEnableSearch())
                    return false;
                relays.transferAndWait();
                uint16_t swr = bridge.MeasureSwr(f,r);
#if USE_RFM69 > 0
                if (interval > 0)
                {   // keep telemetry going during search
                    auto now = millis();
                    if (now - radio.getLastTelemetryMsec()>= interval)
                        radio.sendTelemetryPacket(freqOver4.getFreqRaw(), f, r,
                            cIdx, lIdx, static_cast<uint8_t>(capState));
                }
#endif

#if SERIAL_DEBUG > 0
                Serial.print(F("SEARCHING c="));
                Serial.print((uint16_t)cIdx);
                Serial.print(F(" l="));
                Serial.print(lIdx);
                Serial.print(F(" swr="));
                Serial.print(swr);
                Serial.print(F(" f="));
                Serial.print(f);
                Serial.print(F(" r="));
                Serial.println(r);
                if (Serial.available())
                {
                    Serial.println("abort");
                    return false;
                }
#endif          
                auto rawR = r;
                // restore units for OkToSwitchRelays
                f += 1 << (ForwardReflectedVoltage::TO_AVERAGE_EXP-1);
                f >>= ForwardReflectedVoltage::TO_AVERAGE_EXP; 
                r += 1 << (ForwardReflectedVoltage::TO_AVERAGE_EXP-1);
                r >>= ForwardReflectedVoltage::TO_AVERAGE_EXP;

                bool better = (swr < bestSwr) || (swr == bestSwr && r < lowestR);
                if (better) 
                {
                    bestSwr = swr;
                    cIdxAtMin = cIdx;
                    lIdxAtMin = lIdx;
                    lowestR = rawR;
                }
                if (swr <= bridge.StopSearchSwr())
                    break;
            }
        }
        if ((bestSwr != 0xffff) && bridge.OkToSwitchRelays(f,r))
        {
            relays.setCindex(cIdxAtMin);
            relays.setLindex(lIdxAtMin);
            relays.transferAndWait();
            return true;
        }
        return false;
    }
#endif

bool SwrSearch::findMinSwr(Relays::CapRelayState capState, // returns true on found something
                uint8_t minCidx, uint8_t maxCidx, uint8_t incC,
                uint16_t minLidx, uint16_t maxLidx, uint16_t incL)
    {
#if SERIAL_DEBUG > 0
        Serial.print(F("findMinSwr c="));
        Serial.print(capState == Relays::CapRelayState::STATE_C_LOAD ? "Load " : "Gen ");
        Serial.print(F(" C1=")); Serial.print((uint16_t)minCidx);
        Serial.print(F(" C2=")); Serial.print((uint16_t)maxCidx);
        Serial.print(F(" L1=")); Serial.print(minLidx);
        Serial.print(F(" L2=")); Serial.println(maxLidx);
#endif
#if USE_RELAYS > 0
        bool ret= findMinSwrInternal(capState, minCidx, maxCidx, incC, minLidx, maxLidx, incL);
#if SERIAL_DEBUG > 0
        Serial.print("Search returning ");
        Serial.print(ret? "true c=" : "false c=");
        Serial.print((uint16_t)relays.getCindex());
        Serial.print(" l=");
        Serial.println(relays.getLindex());
#endif
        return ret;
#else
        return false;
#endif
    }
}

bool SwrSearch::loop(unsigned long now)
{
#if SERIAL_DEBUG_SWR_SEARCH > 0
    static unsigned long lastDebugMsg;
    static const int EXIT_MSG_INTERVAL_MSEC = 333;
#endif

    if (bridge.TimeToSearch(now))
    {   // we're here because there is power applied to the tuner
        // and the SWR is above the trigger-to-search.
#if USE_RELAYS > 0
        static const uint16_t FREQ_DELTA_FRACTION_PWR = 5;
        static LCsavedSettings::TableEntry lastTableEntrySearched;
        static uint16_t freqRawLastSearched;
        auto freqRaw = freqOver4.getFreqRaw();
        if (freqRaw == 0)
        {
#if SERIAL_DEBUG_SWR_SEARCH > 0
            if (now - lastDebugMsg >= EXIT_MSG_INTERVAL_MSEC)
            {
                lastDebugMsg = now;
                Serial.println("Exit cuz freqRaw == 0");
            }
#endif
            return false; 
        }

        LCsavedSettings::IndexEntry ie; LCsavedSettings::TableEntry te;
        auto indexIdx = savedLCsettings.lookup(freqRaw, ie, te);

        // we have done a tuneup at this frequency, or close,
        uint16_t lastTunedRange = freqRawLastSearched >> FREQ_DELTA_FRACTION_PWR; // 1/32
        if (((freqRaw >= freqRawLastSearched - lastTunedRange) &&
            (freqRaw <= freqRawLastSearched + lastTunedRange)) &&
                te == lastTableEntrySearched)
        {
#if SERIAL_DEBUG_SWR_SEARCH > 0
            if (now - lastDebugMsg >= EXIT_MSG_INTERVAL_MSEC)
            {
                lastDebugMsg = now;
                Serial.println("Exit cuz tuned up at this freq");
            }
#endif
            return false;
        }
        // Power down and back up, or QSY to get out of this loop

        // pull LC from table lookup
        if (indexIdx != 0xff)
        {   // require a table entry to do anything automatically
            // are we already at the lookup table settings?
            Relays::CapRelayState CtoSet = te.onLoad ? 
                Relays::CapRelayState::STATE_C_LOAD :
                Relays::CapRelayState::STATE_C_GENERATOR;
            auto cIdx = relays.getCindex();
            auto lIdx = relays.getLindex();
            auto capState = relays.getCapRelayState();
            bool isAtTable = 
                (CtoSet == capState) &&
                (te.cIdx == cIdx) &&
                (te.lIdx == lIdx);
            static unsigned long setToTableEntryAt;
            static const int WAIT_AT_TABLE_ENTRY_MSEC = 100;
            if (!isAtTable)
            {   // not at table entry and been a while, then set to table
                relays.setCapRelayState(CtoSet);
                relays.setCindex(te.cIdx);
                relays.setLindex(te.lIdx);
                relays.transferAndWait();
                setToTableEntryAt = millis();
#if SERIAL_DEBUG > 0
                Serial.print(F("SwrSearch::loop set to table entry C="));
                Serial.print((int)te.cIdx);
                Serial.print(F(" L=")); Serial.println(te.lIdx);
#endif
                return true; // but don't update freqRawLastSearched
            }
            
            if (now - setToTableEntryAt < WAIT_AT_TABLE_ENTRY_MSEC)
                return false;

#if USE_RFM69 > 0
            {
                uint16_t f,r;
                bridge.readPowerX8(f,r);
                radio.sendTelemetryPacket(freqRaw, f, r,
                    cIdx, lIdx, static_cast<uint8_t>(capState));
            }
#endif

#if SERIAL_DEBUG > 0
            Serial.print(F("SwrSearch::loop freqRaw=")); Serial.print(freqRaw);
            Serial.print(F(" lastSearched=")); Serial.print(freqRawLastSearched);
            Serial.println(F(" unhappy with table entry. searching"));
#endif

            uint16_t measuredSwr = 0xffff;
            uint16_t f,r;
            for (int j = 0; j < 2; j++)
            {
                // search from where we are
                static const int MAX_SMALL_SEARCHES = 3;
                static const int SEARCH_RANGE = 4;
                for (int i = 1; i <= MAX_SMALL_SEARCHES; i++)
                {
                    // center search on where we are
                    auto cIdx = relays.getCindex();
                    auto lIdx = relays.getLindex();
#if SERIAL_DEBUG > 0
                    Serial.print(F("SwrSearch::loop search from C=")); Serial.print((int)cIdx);
                    Serial.print(F(" L=")); Serial.println(lIdx);
#endif
                    int16_t minCidx = (int16_t)cIdx - SEARCH_RANGE;
                    if (minCidx < 0)
                        minCidx = 0;
                    int16_t maxCidx = (int16_t)cIdx + SEARCH_RANGE;
                    auto maxCindex = relays.getMaxCindex();
                    if (maxCidx > maxCindex)
                        maxCidx = maxCindex;
                    int16_t minLidx = lIdx - SEARCH_RANGE;
                    if (minLidx < 0)
                        minLidx = 0;
                    uint16_t maxLidx = lIdx + SEARCH_RANGE;
                    if (maxLidx > Relays::MAX_LINDEX)
                        maxLidx = Relays::MAX_LINDEX;
                    if (!findMinSwr(capState, minCidx, maxCidx, 1,
                        minLidx, maxLidx, 1))
                        return false; // search failed

#if SERIAL_DEBUG > 0
                    Serial.print(F("SwrSearch::loop setting lastSearched=")); Serial.println((int)freqRaw);
#endif                
                    freqRawLastSearched = freqRaw; // lock out further searching at this frequency
                    lastTableEntrySearched = te;

                    measuredSwr = bridge.MeasureSwr(f,r);
                    if (f == 0)
                        return false; // TX power went off

                    if (measuredSwr <= bridge.StopSearchSwr())               
                        return true;

                }

                if (j != 0) // only try broad search first time through
                    continue;

                // small searches didn't get a good result. try a broad search
                uint16_t maxLindex = relays.MAX_LINDEX;
                // limit use of the inductors to avoid overheating them
                static const uint16_t x20m = -1 + 14ul * 2048ul;
                static const uint16_t x40m = -1 + 7ul * 2048ul;
                static const uint16_t x80m = -1 + (35ul * 2048ul) / 10;
                if (freqRaw >= x20m)
                    maxLindex = maxLindex >> 2; 
                else if (freqRaw >= x40m)
                    maxLindex = maxLindex >> 1;
                else if (freqRaw >= x80m)
                    maxLindex = (maxLindex * 3) / 2;

                if (!findMinSwr(capState, 0, relays.getMaxCindex(), 15, 0, maxLindex, maxLindex >> 4))
                    return false;

                auto nowSwr = bridge.MeasureSwr(f,r);
                if (nowSwr >= measuredSwr)
                    return false;   // got nowhere
                measuredSwr = nowSwr;
            }
        }
#endif
    }
    else
    {
#if SERIAL_DEBUG_SWR_SEARCH > 0
            if (now - lastDebugMsg >= EXIT_MSG_INTERVAL_MSEC)
            {
                lastDebugMsg = now;
                Serial.println("Exit cuz bridge not ready");
            }
#endif
    }
    return false;
}

// FreqOver4 *************************************************************************
/* timer/counter 1 is 16 bits, so overflows at 64K
** Its input is MFJ998 FREQ, which we want to count up to 30MHz (the tuner's design limit)
** FlipFlop reduces FREQ by 4, so we see only up to 7.5MHz(1/4 of actual.)
**
** Use of TIMER1/COUNTER1
**      We set it to interrupt when it reaches these two count values:
**      0xffff (2**16-1) With 30MHz signal, T1 sees 7.5MHz. 0xffff/7.5 = 8378 uSec
**      24000 (see reasons for this choice, below)
**
**      The two-tier count interval is to give a little more consistency
**      in acquisition time. 
**      30MHz  ->   8.378 msec to 16 bit overflow, lower bands take longer
**      1.8Mhz -> would overflow at 146msec, but...
**      At lower frequencies, we really don't want to wait 146 msec
**      for an answer.
**      MIN_USEC_AT_COMPA is set to demand an 8msec minimum calculation length
**          rationale: that's how fast the 30Mhz signal comes in
**      FIRST_STOP_COUNT is set for 12MHz to come in just above 8msec
**          FIRST_STOP_COUNT = 8msec*(12Mhz/4) = 24000;
**      frequencies above 12Mhz will count to overflow, those below
**      will count to 24000, which brings in the 1.8Mhz counter at 53msec.
**
**      Summary:
**      30Mhz counts to 8.378 msec
**      14Mhz will count to 18.7 msec 
**      10Mhz will count to  9.6 msec
**      1.8Mhz will count to 53 msec
*/
//
const unsigned long FreqOver4::MIN_USEC_AT_COMPA = 8000;
const unsigned long FreqOver4::MAX_USEC_TO_DECLARE_ZERO = 55000;
const uint16_t FreqOver4::FIRST_STOP_COUNT = 24000;
const uint16_t FreqOver4::SECOND_STOP_COUNT = 0xffff;

bool FreqOver4::loop()
{
    cli();
    if (micros() - startMicroseconds > MAX_USEC_TO_DECLARE_ZERO)
        history.count = 0;
    sei();
    return false;
}

void FreqOver4::setup()
{
#if COUNT_FREQ_ON_T1 > 0
    /* counting the FREQ input is supported in the Arduino hardware
    ** on the T1 pin. Manufacturer says it is sampled on the system clock,
    ** which is 16MHz, which is too slow for the 30MHz upper limit that
    ** maybe we want to use for the tuner. The daughterboard has a JK flipflop
    ** to divide by 4, so we see up to 7.5MHz on the T1 input. */
#if defined(__AVR_ATmega328P__)
    // Timer counter 1 control register setup
    TCCR1A = 0; // timer counter 1 control register. counter operation "normal"
    TCCR1B = 0x7; // clock select T1 pin
    TCCR1C = 0;
    TCNT1 = 0;
    OCR1A = FIRST_STOP_COUNT; 
    OCR1B = SECOND_STOP_COUNT;
    TIMSK1 = (1 << OCIE1B) | (1 << OCIE1A); // CompB and CompA enable
#endif
    pinMode(FREQ_DIV4_PIN, INPUT_PULLUP);
#endif
}

void FreqOver4::wakeup()
{
    cli();
    history.count = 0;
    TCNT1 = 0;
    startMicroseconds = micros();
    sei();
}

uint16_t FreqOver4::getFreqKhz() const
{return static_cast<uint16_t>(((getFreqRaw() * 1000ul) + (1ul >> (SCALE_POWER-1))) >> SCALE_POWER);}

void FreqOver4::acquire(unsigned long nowusec, uint16_t c)
{   // call with interrupts locked out
    unsigned long usec = nowusec - startMicroseconds;
    startMicroseconds = nowusec;
    TCNT1 = 0;
    if (usec > MAX_USEC_TO_DECLARE_ZERO) // sanity check
    {
        history.count = 0;
        return;
    }
    history.usec = usec;
    history.count = c;
}

void FreqOver4::OnOvf()
{   // interrupt service routine
    acquire(micros(), SECOND_STOP_COUNT);
}

void FreqOver4::OnCompA()
{   // interrupt service routine
    auto tcnt1 = TCNT1;
    auto now = micros();
    if (now - startMicroseconds > MIN_USEC_AT_COMPA)
        acquire(now,tcnt1);
}

uint16_t FreqOver4::getFreqRaw() const // units of MHz * 2048
{
    cli();
    uint32_t usecTotal = history.usec;
    uint32_t fTotal = history.count; // units of counts/4 cuz of FF
    sei();
    if (usecTotal == 0)
        return 0;
    //fTotal is in units of MHz/4 (divide by usec gives Mhz)
    fTotal <<= 13; 
    uint32_t fCalibrate = fTotal / usecTotal; // MHz * 8192 / FF-div-4. overflows at 32MHz at the FF input
    fCalibrate *= calibration;
    fCalibrate >>= 15;
    return (uint16_t)fCalibrate;
}

// RadioRFM69 ******************************************************************
void RadioRFM69::setup()
{
    // Initialize the RFM69HCW:
    m_initialized = initialize(radioConfiguration.FrequencyBandId(),
        radioConfiguration.NodeId(), radioConfiguration.NetworkId());
    // we have to tell the RFM69 not only the "band" it operates on,
    // but we MAY also set the frequency to set its synthesizer within that band.
    uint32_t freq;
    if (radioConfiguration.FrequencyKHz(freq))
        setFrequency(1000*freq);
    // Arduino library puts the synth at a reasonable frequency by default.

#if SERIAL_DEBUG > 0
    Serial.print("RadioRFM69::initialize");
    Serial.print(" returned ");
    Serial.println(m_initialized ? "true" : "false");
#endif

    if (!m_initialized)
        return;

    setHighPower(); // Always use this for RFM69HCW
}

void RadioRFM69::sendTelemetryPacket(uint16_t freq, uint16_t fwd, uint16_t ref,
            uint8_t cIndex, uint16_t lIndex, uint8_t cState)
{
    m_telemetryPos = 0;
#if SERIAL_DEBUG > 0
    Serial.print(F("Telemetry ")); 
#endif
    print("X:"); print(freq);
    print(" F:"); print(fwd);
    print(" R:"); print(ref);
    print(" C:"); print((uint16_t) cIndex);
    print(" L:"); print(lIndex);
    print(" P:"); print((uint16_t)cState);
#if SERIAL_DEBUG > 0
    Serial.println(); 
#endif
    lastTelemetryMsec = millis();
    static const uint16_t GATEWAY_ID = 1;
    // use sendFrame cuz use of canSend() in send is problematic. 
    // it monitors readRSSI() which can take seconds to decay even
    // though each packet is only a few dozen msec.
    sendFrame(GATEWAY_ID, telemetryBuffer, m_telemetryPos, false, false);
}

void RadioRFM69::sendStatusPacket(uint16_t triggerSwr, uint16_t stopSearchSwr)
{
    m_telemetryPos = 0;
#if SERIAL_DEBUG > 0
    Serial.print(F("Status ")); 
#endif
    print("T:"); print(triggerSwr);
    print(" S:"); print(stopSearchSwr);
    print(" MW:"); print(bridge.maxFwdRefDuringSwitching());
    print(" MS:"); print(bridge.minFwdToEnableSearch());
#if SERIAL_DEBUG > 0
    Serial.println(); 
#endif
    static const uint16_t GATEWAY_ID = 1;
    // use sendFrame cuz use of canSend() in send is problematic. 
    // it monitors readRSSI() which can take seconds to decay even
    // though each packet is only a few dozen msec.
    auto now = millis();
    while (!canSend())
    {
        static const int MAX_WAIT = 900;
        if (millis() - now > MAX_WAIT)
            break;
    }
    sendFrame(GATEWAY_ID, telemetryBuffer, m_telemetryPos, false, false);
}

bool RadioRFM69::processCommand(const char *c)
{  return processCommandString(c, strlen(c));}

size_t RadioRFM69::write(uint8_t c)
{   // virtual called by Print base for formating output buffer
    if (m_telemetryPos < sizeof(telemetryBuffer))
    {
        telemetryBuffer[m_telemetryPos++] = c;
#if SERIAL_DEBUG > 0
        Serial.write(c);
#endif
        return 1;
    }
    return 0;
}

// ForwardReflectedVoltage *****************************************************
bool ForwardReflectedVoltage::loop(unsigned long now)
{   // it takes about 100uSec to read. But we sample only every ACQUIRE_BRIDGE_MSEC
    static const long ACQUIRE_BRIDGE_MSEC = 3;
    if (lastReadMsec == 0 || (now - lastReadMsec > ACQUIRE_BRIDGE_MSEC))
    {
        uint16_t f, r;
        acquirePowerNow(f,r);
        history[nextHistory].f = f;
        history[nextHistory++].r = r;
        if (nextHistory >= HISTORY_LEN)
            nextHistory = 0;
        readPowerX8(f,r);
#if SERIAL_DEBUG
        auto prevStartOfTunePower = startOfTunePower;
#endif
        bool belowMin = f < bridge.minFwdToEnableSearch() << HISTORY_PWR;
        if (f >= bridge.maxFwdRefDuringSwitching() << HISTORY_PWR )
            startOfTunePower = 0;
        else if (belowMin)
            startOfTunePower = 0;
        else if (startOfTunePower == 0)
            startOfTunePower = now;
#if SERIAL_DEBUG
        if (prevStartOfTunePower != startOfTunePower)
        {
            Serial.print(F("Power "));
            Serial.print(startOfTunePower == 0 ? "disabled":"enabled");
            Serial.println(F(" search"));
        }
#endif
        activeNow = !belowMin;
        lastReadMsec = now;
    }
    // never prevent sleep cuz were always reading some noise
    return activeNow;
}

void ForwardReflectedVoltage::acquirePowerNow(uint16_t &f, uint16_t &r) const
{
    f = analogRead(FWD_ANALOG_PIN); // range 0 to 1023
    if (f > 0)
        f += DIODE_FORWARD_DROP_MODEL;
    r = analogRead(REF_ANALOG_PIN);
    if (r > 0)
        r += DIODE_FORWARD_DROP_MODEL;
}

uint16_t ForwardReflectedVoltage::Swr(uint16_t f, uint16_t r)
{
    //  SWR = (f + r)/(f - r)
    // fixed point result is calculated as (SWR-1) * 64 
    // Therefore:
    //  1:1 returns 0
    //  max SWR, 0xffff is (about) 1025 : 1
    //  smallest discernable non-unity:
    //  1.016 : 1 returns 0x0001
    //  2 : 1 returns 64
    //  3 : 1 returns 128
    if (r >= f)
        return 0xff;
    if (f == 0)
        return 0xff;
    if (r == 0)
        return 0;
    uint32_t numerator = f + r;
    uint16_t denominator = f - r;
    numerator <<= 6; // fixed point scale by 64 (2**6)
    numerator /= denominator;
    numerator -= 1 << 6; // subtract one to fully use our uint16_t
    if (numerator >= 0x80000ul) // biggest that will fit in return type
        return 0xffff;
    return static_cast<uint16_t>(numerator);
}

uint16_t ForwardReflectedVoltage::Swr()
{
    uint16_t f, r;
    readPowerX8(f,r);
    return Swr(f,r);
}
uint16_t ForwardReflectedVoltage::MeasureSwr(uint16_t &f, uint16_t &r)
{
    f = r = 0;
    // the A/D acquisition is 10 bits. The sum must fit in 16.
    // cannot extend more than 6
    static const int TO_AVERAGE_COUNT = 1 << TO_AVERAGE_EXP;
    for (int i = 0; i < TO_AVERAGE_COUNT; i+=1)
    {
        uint16_t f1, r1;
        bridge.acquirePowerNow(f1,r1);
        f += f1; r += r1;
        delayMicroseconds(450);
    }
    return Swr(f,r);
}

void ForwardReflectedVoltage::readPowerX8(uint16_t &f, uint16_t &r) const
{   // return the sum. scaled by HISTORY_LEN compared to acquirePowerNow
    f = r = 0;
    for (uint8_t i = 0; i < HISTORY_LEN; i++)
    {
        f += history[i].f;
        r += history[i].r;
    }
}

bool ForwardReflectedVoltage::OkToSwitchRelays(uint16_t f, uint16_t r)
{  
    bool ret = (f <= gMAX_FWD_REF_DURING_SWITCHING) && (r <= gMAX_FWD_REF_DURING_SWITCHING);
#if SERIAL_DEBUG > 0
    if (!ret)
    {
        Serial.print(F("Not OkToSwitchRelays f=")); Serial.print(f); 
        Serial.print(" r="); Serial.println(r);
    }
#endif
    return ret;
}

bool ForwardReflectedVoltage::OkToSwitchRelays()
{
    uint16_t f, r;
    acquirePowerNow(f,r);
    return OkToSwitchRelays(f,r);
}

bool ForwardReflectedVoltage::TimeToSearch(long now)
{
    if (startOfTunePower==0 || (now - startOfTunePower < MSEC_OF_TUNE_POWER_TO_ENABLE_SEARCH))
        return false;
    return Swr() > triggerSearchSwr;
}

void ForwardReflectedVoltage::sleep()
{
// start looking for tune power again
    lastReadMsec = 0;
    startOfTunePower = 0; 
}

void ForwardReflectedVoltage::setup()
{
    uint16_t temp;
    EEPROM.get(Eeprom328pAddr(TRIGGER_SWR), temp);
    if (temp != 0xffffu)
        triggerSearchSwr = temp;
    EEPROM.get(Eeprom328pAddr(STOP_SEARCH_SWR), temp);
    if (temp < triggerSearchSwr)
        stopSearchSwr = temp;
    EEPROM.get(Eeprom328pAddr(MAX_SWITCHING_V_ADC), temp);
    if (temp != 0xffffu)
        gMAX_FWD_REF_DURING_SWITCHING = temp;
    EEPROM.get(Eeprom328pAddr(MIN_SEARCH_V_ADC), temp);
    if (temp != 0xffffu)
        gMIN_FWD_TO_ENABLE_SEARCH = temp;
}

void ForwardReflectedVoltage::setTriggerSwr(uint16_t triggerSwr)
{
    if (triggerSwr >= MIN_TRIGGER_SWR) // don't allow just any setting
    {
        EEPROM.put(Eeprom328pAddr(TRIGGER_SWR), triggerSwr);
        triggerSearchSwr = triggerSwr;
    }
}

void ForwardReflectedVoltage::setStopSearchSwr(uint16_t ss)
{
    if (ss < triggerSearchSwr)
    {
        EEPROM.put(Eeprom328pAddr(STOP_SEARCH_SWR), ss);
        stopSearchSwr = ss;
    }
}

void ForwardReflectedVoltage::minFwdToEnableSearch(uint16_t v)
{
    gMIN_FWD_TO_ENABLE_SEARCH = v;
    EEPROM.put(Eeprom328pAddr(MIN_SEARCH_V_ADC), v);
}

void ForwardReflectedVoltage::maxFwdRefDuringSwitching(uint16_t v)
{
    gMAX_FWD_REF_DURING_SWITCHING = v;
    EEPROM.put(Eeprom328pAddr(MAX_SWITCHING_V_ADC), v);
}

// LCsavedSettings**************************************************************
uint8_t LCsavedSettings::lookup(uint16_t freqHalfHz, IndexEntry &ie, TableEntry &te)const
{
    // lookup sets TableEntry &te to the highest table entry that is less than or equal
    // in frequency. we do NOT interpolate.
#if USE_EEPROM > 0
    uint16_t indexAddr = INDEX_START_ADDR;
    for (uint8_t i = 0; i < NUM_INDEX_ENTRIES; i++, indexAddr += sizeof(IndexEntry))
    {
        eepromSPI.read(indexAddr, (uint8_t*)&ie, sizeof(ie));
        if (!ie.isValid())
            continue;
        if (ie.frequencyStartHalfKhz > freqHalfHz)
            continue; 
        uint16_t maxFreqForThisIndexEntry = ie.frequencyStartHalfKhz;
        maxFreqForThisIndexEntry += (uint16_t)ie.numEntries * (uint16_t)ie.frequencyIncrement;
        if (maxFreqForThisIndexEntry >= freqHalfHz)
        {   // found first index entry that covers freqHalfHz
            uint16_t which = (freqHalfHz - ie.frequencyStartHalfKhz) / ie.frequencyIncrement;
            uint16_t addr = ie.addr + which * sizeof(TableEntry);  
            eepromSPI.read(addr, (uint8_t*)&te, sizeof(te));
#if 0//SERIAL_DEBUG > 0
            Serial.print(F("lookup found i=")); Serial.print((int)i);
            Serial.print(F(" w=")); Serial.print(which);
            Serial.print(F(" a=0x")); Serial.print(addr, HEX);
            Serial.print(F(" f=")); Serial.print(freqHalfHz);
            Serial.print(F(" C=")); Serial.print((int)te.cIdx);
            Serial.print(F(" L=")); Serial.println(te.lIdx);
#endif            
            return i;
        }
    }
#endif
    return 0xff;
}

uint16_t LCsavedSettings::findUnusedBlock(uint8_t numEntries) const
{
#if USE_EEPROM > 0
    /* this allocation routine can and will FRAGMENT the EEPROM
    ** memory. Fixing that will require an outside application to 
    ** read its contents out, clear the EEPROM, then write them back.
    */
    const uint16_t size = (uint16_t)numEntries * sizeof(TableEntry);
    uint16_t candidate = FIRST_TABLE;
    uint16_t end = candidate + size - 1;
    for (;;)
    {   /* each pass through, set candidate to page boundary beyond an allocation,
        ** using the maximum such address over all index entries. */
        bool ok = true;
        bool updated = false;
        uint16_t indexAddr = INDEX_START_ADDR;
        for (uint8_t i = 0; i < NUM_INDEX_ENTRIES; i++, indexAddr += sizeof(IndexEntry))
        {
            IndexEntry ie;
            eepromSPI.read(indexAddr, (uint8_t*)&ie, sizeof(ie));
            if (ie.frequencyStartHalfKhz == UNUSED_FREQUENCY)
                continue; 
            if (ie.addr == 0)
                continue;
            uint16_t lastAddr = (uint16_t)ie.numEntries * (uint16_t)sizeof(TableEntry);
            lastAddr -= 1;
            lastAddr += ie.addr;
            if (!((lastAddr < candidate) || (ie.addr > end)))
            {   // this table overlaps with candidate. 
                ok = false;
                uint16_t next = lastAddr + 1;
                // bump to next EEPROM page boundary
                next += 0x7f;
                next &= ~0x7f;
                if (next > candidate)
                {
                    candidate = next;
                    end = candidate + size - 1;
                    if (end < candidate)
                        return 0; //overflowed
                    updated = true;
                }
            }
        }
        if (ok) // made it through without an overlap
        {
#if SERIAL_DEBUG > 0
            Serial.print(F("findUnusedBlock for "));
            Serial.print(size);
            Serial.print(F(" found:0x"));
            Serial.println(candidate, HEX);
#endif
            return candidate;
        }
        if (!updated)   // no overlap and no update. done
        {
#if SERIAL_DEBUG > 0
            Serial.print(F("findUnusedBlock for "));
            Serial.print(size);
            Serial.println(F(" failed"));
#endif
            return 0;
        }
    }
#endif
    return 0;
}

uint8_t LCsavedSettings::dumpTableEntry(uint8_t index, uint8_t i, char *buf, uint8_t len)
{
#if USE_EEPROM > 0
    if (index >= NUM_INDEX_ENTRIES)
    {
        print("inv");
        return 0;
    }
    uint8_t init = len;
    printRemaining = len;
    toPrint = buf;
    IndexEntry ie;
    uint16_t indexAddr = INDEX_START_ADDR + sizeof(IndexEntry) * (uint16_t)index;
    eepromSPI.read(indexAddr, (uint8_t*)&ie, sizeof(ie));
    // the print() method is in our Print base class
    if (ie.addr == 0)
        print("addr=0");
    else if (ie.frequencyStartHalfKhz == UNUSED_FREQUENCY)
        print("unused");
    else if (i >= ie.numEntries)
        print("max");
    else {
        print((int)index); print(",");
        print((int)i); print(",");
        TableEntry te;
        eepromSPI.read(ie.addr + i * sizeof(te), (uint8_t*)&te, sizeof(te));
        print((int)te.cIdx); print(",");
        print((int)te.lIdx); print(",");
        print((int)te.onLoad);
    }
    return init - printRemaining;
#else
    return 0;
#endif
}

size_t LCsavedSettings::write(uint8_t c)
{
    if (printRemaining > 1)
    {
        *toPrint++ = c;
        *toPrint = 0;
        printRemaining -= 1;
        return 1;
    }
    return 0;
}

bool LCsavedSettings::getIndexEntry(uint8_t idx, IndexEntry &ie)const
{
#if USE_EEPROM > 0
    if (idx >= NUM_INDEX_ENTRIES)
        return false; // invalid
    uint16_t indexAddr = INDEX_START_ADDR + sizeof(IndexEntry) * (uint16_t)idx;
    eepromSPI.read(indexAddr, (uint8_t*)&ie, sizeof(ie));
    return true;
#else
    return false;
#endif;
}

uint8_t LCsavedSettings::updateIndex(IndexEntry &ieUpdate)const
{
/* Find enough unused EEPROM to hold the given ieUpdate and write
** it into EEPROM, updating its addr in the process */
#if USE_EEPROM > 0
    if (ieUpdate.numEntries == 0)
        return 0xff;
    if (ieUpdate.frequencyIncrement == 0)
        return 0xff;
    uint16_t tableAddr = findUnusedBlock(ieUpdate.numEntries);
    if (tableAddr != 0)
    {
        ieUpdate.addr = tableAddr;
        uint16_t indexAddr = INDEX_START_ADDR;
        for (uint8_t i = 0; i < NUM_INDEX_ENTRIES; i++, indexAddr += sizeof(IndexEntry))
        {
            IndexEntry ie;
            eepromSPI.read(indexAddr, (uint8_t*)&ie, sizeof(ie));
            if ((ie.frequencyStartHalfKhz == UNUSED_FREQUENCY) ||
                (ie.addr == 0))
            {
                if (eepromSPI.waitReadyForWrite())
                    eepromSPI.writeAcrossPageBoundary(indexAddr, (const uint8_t*)&ieUpdate, sizeof(ieUpdate)); 
                else
                    return -1;
                return i;
            }
        }
    }
#endif
    return -1;
}

bool LCsavedSettings::removeIndexEntry(uint8_t index)const
{
#if USE_EEPROM > 0
    if (index >= NUM_INDEX_ENTRIES)
        return false; // invalid
    static const uint8_t COUNT = 4;
    static const uint8_t CLEARED_ENTRY[COUNT] = { 0, 0, 0xff, 0xff};
    uint16_t indexAddr = INDEX_START_ADDR + sizeof(IndexEntry) * (uint16_t)index;
    if (eepromSPI.waitReadyForWrite())
    {
#if SERIAL_DEBUG > 0
        Serial.print(F("removeIndexEntry idx="));
        Serial.print((int)index);
        Serial.print(F(" a=0x")); Serial.println(indexAddr, HEX);
#endif
        return eepromSPI.writeAcrossPageBoundary(indexAddr, CLEARED_ENTRY, COUNT);
    }
#endif
    return false;
}

bool LCsavedSettings::updateTableEntries(uint8_t index, uint8_t firstToUpdate, const TableEntry*te, uint8_t numTableEntries)const
{
#if USE_EEPROM > 0
    if (index >= NUM_INDEX_ENTRIES)
        return false;
    uint16_t indexAddr = INDEX_START_ADDR + sizeof(IndexEntry) * (uint16_t)index;
    IndexEntry ie;
    eepromSPI.read(indexAddr, (uint8_t*)&ie, sizeof(ie));
    if (ie.addr == 0)
        return false;
    if (ie.frequencyStartHalfKhz == UNUSED_FREQUENCY)
        return false;
    uint16_t endTableIdx = firstToUpdate;
    endTableIdx += numTableEntries;
    if (endTableIdx > ie.numEntries)
        return false;
    uint16_t tableAddr = ie.addr;
    tableAddr += firstToUpdate * sizeof(TableEntry);
#if SERIAL_DEBUG > 0
    Serial.print(F("write EEPROM i="));
    Serial.print((int)index);
    Serial.print(F(" a=0x")); Serial.print(tableAddr, HEX);
    Serial.print(F(" first=")); Serial.print((int)firstToUpdate);
    Serial.print(F(" count=")); Serial.print((int)numTableEntries);
    Serial.print(F(" C=")); Serial.print((int)te->cIdx);
    Serial.print(F(" L=")); Serial.print((int)te->lIdx);
    Serial.print(F(" P=")); Serial.println(te->onLoad ? "1" : "2");
#endif
    if (eepromSPI.writeAcrossPageBoundary(tableAddr,reinterpret_cast<const uint8_t*>(te),numTableEntries * sizeof(TableEntry)))
        return true;
#endif
    return false;
}
bool LCsavedSettings::updateTableEntry(uint16_t freqHalfHz, const TableEntry &te)
{
    TableEntry temp;
    IndexEntry ie;
    auto idx = lookup(freqHalfHz, ie, temp);
    if (idx >= NUM_INDEX_ENTRIES)
        return false;
    uint8_t which = (freqHalfHz - ie.frequencyStartHalfKhz) / ie.frequencyIncrement;
    return updateTableEntries(idx, which, &te, 1);
}
// interrupt handlers **********************************************************
#if defined(__AVR_ATmega328P__)
// Timer/counter #2 in the Atmega328 hardware
ISR (TIMER2_OVF_vect)
{  timer2Pwm.OnOvf();}
ISR (TIMER2_COMPA_vect)
{ timer2Pwm.OnCompA();}
ISR (TIMER2_COMPB_vect)
{  timer2Pwm.OnCompB();}
#endif

#if COUNT_FREQ_ON_T1 > 0 && defined(__AVR_ATmega328P__)
ISR (TIMER1_COMPB_vect)
{  freqOver4.OnOvf();}
ISR (TIMER1_COMPA_vect)
{  freqOver4.OnCompA();}
#endif
