#pragma once
#include "PinAssignments.h"
class Relays { // relays are output only. 22 bits of them
public:
    enum class CapRelayState : uint8_t {STATE_BYPASS, STATE_C_LOAD, STATE_C_GENERATOR};
    // bit positions in the shift register...
    enum class BitNumber {K1_25ohm_INPUT=0, 
                        K15_16_C_GENERATOR, K13_14_C_LOAD, // smaller capacitors switch to either LOAD or GENERATOR
                        K2_ANTENNASELECT_1_2,                           // not on MFJ998RT model
                        C31pF_K17_18, L760nH_K3, L1520nH_K4, 
                        L3040nH_K5, L190nH_K6, L380nH_K7, 
                        K31_AMP_ENABLE,                             // not on MFJ998RT model
                        L12160nH_K12, 
                        L95nH_K10_11, //  95nH coils. 
                        L6080nH_K9, 
                        L95nH_K8, // the other 95nH coil
                        C1950pF_K30, C1000pF_K29, //These largest capacitors are hardwired GENERATOR side!
                        C15pF_K27_28, C62pF_K25_26, 
                        C124pF_K23_24, C248pF_K21_22, C496pF_K19_20};

    // masks
    static const uint32_t 
        C0,        C1,        C2,        C4,        C8,
        C16,        C32,        C64,        C_MASK;

    static const uint32_t 
        L0,     // lowest two are close to same value
        L01,    // ditto
        L1,        L2,        L4,        L8,        L16,        L32,        L64,
        L_MASK;

    Relays() : relaystate(0), lIndex(0), cIndex(0), capstate(CapRelayState::STATE_BYPASS) {}

    void transfer()
    {
        pinMode(MFJ998_CLOCK_AND_MODE_PIN, OUTPUT);
#if USING_SPI
        // tell arduino system we're using SPI...which we sort-of are.
        SPI.beginTransaction(EepromSPI::settings);
#if defined(__AVR_ATmega328P__)
        /* the relay shift register uses the same pins
        ** as the hardware SPI. The hardware won't let us
        ** control those pins if SPI is turned on. So we save/restore */
        auto spcrSaved = SPCR;
        SPCR = 0;
        auto portBddr = DDRB;
        auto portB = PORTB;
        DDRB |= (1 << PB3);
#endif
#else
        pinMode(MFJ998_SDA_PIN, OUTPUT);
#endif
        shiftOut(MFJ998_SDA_PIN, MFJ998_CLOCK_AND_MODE_PIN, MSBFIRST, relaystate>>16);
        shiftOut(MFJ998_SDA_PIN, MFJ998_CLOCK_AND_MODE_PIN, MSBFIRST, relaystate>>8);
        shiftOut(MFJ998_SDA_PIN, MFJ998_CLOCK_AND_MODE_PIN, MSBFIRST, relaystate);
        digitalWrite(RELAYS_STROBE_PIN, HIGH);
#if USING_SPI
#if defined(__AVR_ATmega328P__)
        // restore hardware state
        PORTB = portB;
        DDRB = portBddr;
        SPCR = spcrSaved;
#endif
        SPI.endTransaction();
#endif
        pinMode(MFJ998_CLOCK_AND_MODE_PIN, INPUT_PULLUP);
        digitalWrite(RELAYS_STROBE_PIN, LOW); 
    }

    void transferAndWait()
    {
        transfer();
        delay(RELAY_SWITCH_MSEC);
    }
    void setup()
    { set(0);}  // deactivate all relays

    void set(uint32_t s)
    {
        relaystate = s;
        transfer();
    }

    /* these 3 calls must be followed by transfer() to actually change a relay. 
    ** More than one may be called between transfer() calls.*/
    bool setLindex(uint16_t idx);
    bool setCindex(uint8_t idx);
    void setCapRelayState(CapRelayState v);
    uint8_t getMaxCindex() const // hardwired capacitors on LOAD side check
    { return (capstate == CapRelayState::STATE_C_GENERATOR) ? 255 : MAX_LOAD_CINDEX;}

    void setK1On()
    { relaystate |= 1ul << (int)BitNumber::K1_25ohm_INPUT; }
    void setK1Off()
    { relaystate &= ~ (1ul << (int)BitNumber::K1_25ohm_INPUT); }

    uint32_t get() const { return relaystate;}
    CapRelayState getCapRelayState() const { return capstate;}
    uint16_t getLindex() const { return lIndex;}
    uint8_t getCindex() const { return cIndex;}

    enum {NUM_LVALUES=258, MAX_LINDEX=NUM_LVALUES-1, MAX_LOAD_CINDEX=63, RELAY_SWITCH_MSEC = 60};
    static const uint16_t LVALUES[NUM_LVALUES] PROGMEM; // 16bit optimization! L relays are all below position 16
protected:
    // we keep a copy of the last-set state
    uint32_t relaystate;
    uint16_t lIndex;
    uint8_t cIndex;
    CapRelayState capstate;
};

