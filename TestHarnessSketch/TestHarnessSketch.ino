/* Copyright (C) 2020 by Wayne Wright, W5XD. All rights reserved
**
** Sketch to accomplish a hardware diagnostic on the MFJ998 daughterboard.
** It should run on pretty much any arduino with SPI support and enough pins.
** The author built and ran it on a Mega
**
** The purpose of this sketch is to exercise the as-soldered surface mount ICs
** pins on the daughterboard BEFORE the Arduino Pro Mini is installed and blocks
** access to the ICs below.
*/ 
#include <SPI.h>
#include <RFM69.h>      // the wireless transceiver
#include <eepromSpi.h>
#include <portExtender.h>

// Terminology definitions:
// Test Machine is the machine running this sketch. It is NOT part of the
// final product but is only used to exercise the partially assembled daughterboard.
// Daughterboard is the PCB that will be the final product but WITHOUT its ardunio
// installed, nor plugged into the MFJ998 motherboard.
// 
// On Test Machine , connect the following pins to
// the daughterboard. (along with ground and 5V)
namespace {
/* There are 18 jumpers to install from the Test Machine
** onto the partially assembled daughterboard. 5V and ground and 
** the 16 signals below. 7 of the signals go to the header for the PIC part,
** and 9 go to the header for the Pro Mini. 
** With the exception of MISO, MOSI, SCK, and the RFM69_INT_PIN, all pins on
** the device doing the testing (i.e. that is running this test) are just
** used as data pins and need no special hardware support on the Test
** Machine. That means you can rebuild the sketch with different Test Machine
** pins if that suits your fancy. The daughterboard pins, however, 
** are built into the daughterboard PCB.
**
** 7 jumpers to the PIC header on the board-under-test */
           /* daughterboard PIC18 */    /* Test Machine */
 const int LCD4 =                       3;
 const int LCD5 =                       4;
 const int LCD6 =                       5;
 const int LCD7 =                       6; 
 const int PD_PIN =                     7;
 const int WAKEUP_PIN =                 8;
 const int FREQ_PIN =                   9;
 // also +5V (PIC pin 20) and GND (PIC pin 19). Unlabeled on daughterboard

// 9 jumpers to the arduino header on the board-under-test
 /* name in code below */       /* daughterboard Arduino */     /* pin on Test Machine */
 const int RFM69_INT_PIN =      /* IntR*/                            2; // exception: must be external interrupt. INT0 in this case
 const int RFM69_SELECT_PIN =   /* Rsel */                          14; // A0 on UNO
 const int FREQ_DIV4_PIN =      /* F/4 */                           15; // A1
 const int EXTENDER_CS_PIN =    /* A3 (unlabeled)*/                 16; // A2
 const int iSPI_PIN =           /* iSPI */                          17; // A3
 const int MEM512_SELECT_PIN =  /* EEP */                           A5;  
//plus MISO, MOSI, SCK as defined in SPI.H
// SCK is unlabeled on daughterboard. its next to MISO

}
static RFM69 rfm69(RFM69_SELECT_PIN, RFM69_INT_PIN); 

static EepromSPI eepromSPI;
const SPISettings EepromSPI::settings = SPISettings(10000000, MSBFIRST, SPI_MODE0);
const int EepromSPI::MEM512_SELECT_PIN = ::MEM512_SELECT_PIN;

class PortExtenderTest { // PortExtender template requires a base class with write4bits
public:
    virtual void write4bits(uint8_t)=0;
protected:
    void pulseEnable(){} // and a pulseEnable which is not used in this test harness.
};

typedef PortExtender<PortExtenderTest> ExtenderWithLiquidCrystal;
ExtenderWithLiquidCrystal extender;
template <> const SPISettings PortExtender<PortExtenderTest>::settings = SPISettings(5000000, MSBFIRST, SPI_MODE0);
template <> const int PortExtender<PortExtenderTest>::EXTENDER_CS_PIN = ::EXTENDER_CS_PIN;

static void printExtenderRegisters()
{
    uint8_t extRegisters[10];
    memset(extRegisters, 0, sizeof(extRegisters));
    extender.getRegisters(extRegisters);
    Serial.println(F("MSCP23S08 registers"));
    for (int i = 0; i < 10; i++)
    {
        Serial.print(F("   0x"));
        Serial.print(i,HEX);
        Serial.print(F(" =0x"));
        Serial.print((int)extRegisters[i], HEX);
        Serial.println();
    }
}

void setup() {
    Serial.begin(9600);
    Serial.println(F("MFJ998 daughter test harness (rev02)"));
    Serial.println(F("Type A to test All. E to EEPROM, X to test extender, F for frequency divider"));
    SPI.begin();

    pinMode(FREQ_PIN, OUTPUT);
    pinMode(LCD7, INPUT);
    pinMode(LCD6, INPUT);
    pinMode(LCD5, INPUT);
    pinMode(LCD4, INPUT);
    pinMode(WAKEUP_PIN, OUTPUT);
    pinMode(PD_PIN, OUTPUT);

    eepromSPI.init();

    pinMode(FREQ_DIV4_PIN, INPUT);
    pinMode(iSPI_PIN, INPUT);

    digitalWrite(FREQ_PIN, LOW);
    digitalWrite(WAKEUP_PIN, HIGH);
    digitalWrite(PD_PIN, LOW);
    digitalWrite(EXTENDER_CS_PIN, HIGH);
    digitalWrite(iSPI_PIN, HIGH);

    extender.setup();
}

bool testEEprom()
{
    bool pass = true;
    // check that EEPROM can be written and read
    // well, OK, the EEPROM is not underneath the Arduino, so it
    // could be resoldered after the Arduino is in place. but its easy to check so 
    // we do it here.

    Serial.println(F("EEPROM write to address 0 and 0xfff0"));
    static const uint16_t addr0 = 0;
    static const uint16_t addr1 = 0xfff0;
    static const uint32_t v1= 0x1f002;
    static const uint32_t v2 = 1;
    static const uint32_t v3 = 0x1E;
    static const uint32_t v4 = 0x7f0021;
    uint32_t v ;
    uint32_t r1(0), r2(0);
    v = v1;
    if (!eepromSPI.waitReadyForWrite())
    {
        pass = false;
        Serial.println(F("waitReadyForWrite failed"));
    }
    eepromSPI.write(addr1, reinterpret_cast<const uint8_t*>(&v), sizeof(v));
    v = v2;
    if (!eepromSPI.waitReadyForWrite())
    {
        pass = false;
        Serial.println(F("waitReadyForWrite failed"));
    }
    eepromSPI.write(addr0, reinterpret_cast<const uint8_t*>(&v), sizeof(v)); 

    Serial.println(F("EEPROM read from address 0 and 0xfff0"));
    eepromSPI.read(addr0, reinterpret_cast<uint8_t*>(&r1), sizeof(r1));
    eepromSPI.read(addr1, reinterpret_cast<uint8_t*>(&r2), sizeof(r2));

    if (r1 != v2 || r2 != v1)
    {
        Serial.print(F("r1 = 0x")); Serial.print(r1, HEX);
        Serial.print(F(" r2 = 0x")); Serial.print(r2, HEX);
        Serial.println(F(" EEPROM TEST FAILED ****************"));
        pass = false;
    }

    Serial.println(F("EEPROM write other values to address 0 and 0xfff0"));
    v = v3;
    if (!eepromSPI.waitReadyForWrite())
    {
        pass = false;
        Serial.println(F("waitReadyForWrite failed"));
    }
    eepromSPI.write(addr1, reinterpret_cast<const uint8_t*>(&v), sizeof(v));
    v = v4;
    if (!eepromSPI.waitReadyForWrite())
    {
        pass = false;
        Serial.println(F("waitReadyForWrite failed"));
    }
    eepromSPI.write(addr0, reinterpret_cast<const uint8_t*>(&v), sizeof(v)); 

    Serial.println(F("EEPROM read from addresses"));
    r1 = ~0;
    r2 = ~0;
    eepromSPI.read(addr0, reinterpret_cast<uint8_t*>(&r1), sizeof(r1));
    eepromSPI.read(addr1, reinterpret_cast<uint8_t*>(&r2), sizeof(r2));
    if (r1 != v4 || r2 != v3)
    {
        Serial.print(F("r1 = 0x")); Serial.print(r1, HEX);
        Serial.print(F(" r2 = 0x")); Serial.print(r2, HEX);
        Serial.println(F(" EEPROM TEST FAILED ****************"));
        pass = false;
    }
    return pass;
}

bool testExtender()
{
    bool pass = true;
   // for MSCP23S08 extender...
    //  a. check that we can write each of LCD4 through LCD7 as both low and high
    printExtenderRegisters();
    uint8_t b = 1;
    // LCD4-7 are inputs now
    for (int i = 0; i < 4; i++)
    {
        Serial.print(F("LCD bits set to 0x"));
        Serial.print((int)b, HEX);
        extender.write4bits(b);
        if (digitalRead(LCD4) != ((b & 1) != 0 ? HIGH : LOW))
        {
            pass = false;
            Serial.print(F("Failed LCD4 read on i = "));
            Serial.print(i);
            Serial.println(F(" **********************"));
            break;
        }
        else
            Serial.print(F(". LCD4 OK"));
        if (digitalRead(LCD5) != ((b & 2) != 0 ? HIGH : LOW))
        {
            pass = false;
            Serial.print(F("Failed LCD5 read on i = "));
            Serial.print(i);
            Serial.println(F(" **********************"));
            break;
        }
        else
            Serial.print(F(" LCD5 OK"));
        if (digitalRead(LCD6) != ((b & 4) != 0 ? HIGH : LOW))
        {
            pass = false;
            Serial.print(F("Failed LCD6 read on i = "));
            Serial.print(i);
            Serial.println(F(" **********************"));
            break;
        }
        else
            Serial.print(F(" LCD6 OK"));
        if (digitalRead(LCD7) != ((b & 8) != 0 ? HIGH : LOW))
        {
            pass = false;
            Serial.print(F("Failed LCD7 read on i = "));
            Serial.print(i);
            Serial.println(F(" **********************"));
            break;
        }
        else
            Serial.println(F(" LCD7 OK"));
        b <<= 1;
    }

    // now switch extender to read LCD4-7, with its internal pullups
    extender.setLCDpinsAsInputs();

    // for MSCP23S08 extender...
    //  b. check that it will assert its interrupt output
    if (digitalRead(iSPI_PIN) != HIGH)
    {
        pass = false;
        Serial.println(F("INT0 initial HIGH failed."));
    }
    else
    {
        Serial.println(F("Initial iSPI is OK (high)"));
        auto pins = extender.getPins();
        if (pins != 0xbf)
        {
            pass = false;
            Serial.println(F("Extender port failed to read 0xbf ***********************"));
        }
        else
            Serial.println(F("Read pins 0xbf correct (PD is only low input)"));

        digitalWrite(WAKEUP_PIN, LOW);
        if (digitalRead(iSPI_PIN) != LOW)
        {
            pass = false;
            Serial.println(F("FAILED. INT0 pin did not respond to WAKEUP"));
        }
        else
            Serial.println(F("extender WAKEUP_PIN OK (went low)"));

        digitalWrite(WAKEUP_PIN, HIGH);
        auto interruptPins = extender.getCapturedInterruptMask();
        interruptPins &= ~0x3F; // only top two bits are of interest
        if (interruptPins != 0x0) // at the interrupt, both WAKEUP and PD were LOW
        {
            pass = false;
            Serial.print(F("interrupts: 0x")); Serial.println((int)interruptPins, HEX);
            Serial.println(F("FAILED. WAKEUP_PIN does not appear in interrupt mask*************"));
        }
        else
            Serial.println(F("WAKEUP_PIN is in interrupt mask. OK"));

        // Note--getCapturedInterruptMask caused iSPI_Pin to go low
        if (digitalRead(iSPI_PIN) != HIGH)
        {
            pass = false;
            Serial.println(F("FAILED. WAKEUP to high kept interrupt ************************"));
        }
        else
            Serial.println(F("extender removed iSPI on WAKEUP high, OK"));

        digitalWrite(PD_PIN, HIGH);
        pins = extender.getPins();
        if (pins != 0xff)
        {
            pass = false;
            Serial.println(F("FAILED. Extender port failed to read 0xff  ***********************"));
        }
        else
            Serial.println(F("extender PD_PIN HIGH OK"));

        if (digitalRead(iSPI_PIN) != LOW)
        {
            pass = false;
            Serial.println(F("FAILED. PD to high did not interrupt ************************"));
        }
        else
            Serial.println(F("Extender interrupt on PD high, OK"));
    }

    digitalWrite(WAKEUP_PIN, HIGH);
    digitalWrite(PD_PIN, LOW);
    return pass;
}

bool testF4()
{
    bool pass = true;
    Serial.println(F("Init FREQ_DIV4 counter"));
    // The JK flipflop starts in an unknown state.
    // clock it until we see HIGH, then again
    // until we see LOW and then we know its state
    bool gotHigh = false;
    for (int i = 0; i < 4; i++)
    {
        if (digitalRead(FREQ_DIV4_PIN) == HIGH)
        {
            gotHigh = true;
            break;
        }
        digitalWrite(FREQ_PIN, HIGH);
        digitalWrite(FREQ_PIN, LOW);
    }

    if (!gotHigh)
    {
        pass = false;
        Serial.println(F("Failed to find FREQ_DIV4_PIN high. ************** "));
    }
    else
        Serial.println(F("FREQ_DIV4 counter high OK"));


    // now push FREQ_DIV4_PIN LOW
    bool gotLow = false;
    for (int i = 0; i < 4; i++)
    {
        if (digitalRead(FREQ_DIV4_PIN) == LOW)
        {
            gotLow = true;
            break;
        }
        digitalWrite(FREQ_PIN, HIGH);
        digitalWrite(FREQ_PIN, LOW);
    }
    if (!gotLow)
    {
        pass = false;
        Serial.println(F("Failed to find FREQ_DIV4_PIN low. ************** "));
    }
    else
        Serial.println(F("FREQ_DIV4 input low OK")); // and we know the flipflop state

    // check that what we clock out on FREQ appears at F/4
    bool f4Ok = true;
    for (int i = 0; i < 512; i++)
    {
        digitalWrite(FREQ_PIN, HIGH);
        digitalWrite(FREQ_PIN, LOW);
        int val = digitalRead(FREQ_DIV4_PIN);
        int expected = (((i+1)/2) & 1) == 0 ? LOW : HIGH;
        if (val != expected )
        {
            pass = false;
            f4Ok = false;
            Serial.print(F("Failed to read F/4 at i ="));
            Serial.print(i);
            Serial.print(F(" expected: "));
            Serial.print(expected);
            Serial.print(F(" got: "));
            Serial.print(val);
            Serial.println(F(" *************************"));
            break;
        }    
    }
    if (f4Ok)
        Serial.println(F("Got F/4"));
    return pass;
}

bool testAll()
{
    // these checks cover all the SMD pins that get covered up when
    // the arduino is soldered in place
    bool pass = true;
    pass &= testEEprom();

    pass &= testF4();
 
    pass &= testExtender();

    // check that the RFM69 will initialize...don't bother to transmit or receive.
    // like the eeprom, its not under the Arduino on the board so could be resoldered
    // after the arduino install, but lets force it through its SPI init here anyway.

    static const int RFM69_FREQ = 91;
    static const int RFM69_NODEID = 1;
    static const int RFM69_NETID = 1;
    bool res = rfm69.initialize(RFM69_FREQ,RFM69_NODEID, RFM69_NETID);
    Serial.println(res ? F("RFM69 test passed"):F("RFM69 TEST FAILED***********************"));

    pass &= res;
    return pass;
}   

void loop()
{
    while (Serial.available() > 0)
    {
        auto c = Serial.read();
        if (c == 'a' || c == 'A')
        {
            Serial.println(F("\r\nAll Tests"));
            if (testAll())
                Serial.println(F("All tests passed"));
        }
        else if (c == 'e' || c == 'E')
        {
            Serial.println(F("\r\nEEPROM test"));
            if (testEEprom())
                Serial.println(F("EEPROM test passed"));
        }
        else if (c == 'x' || c == 'X')
        {
            Serial.println(F("\r\nExtender test"));
            if (testExtender())
                Serial.println(F("Extender test passed"));
        }
        else if (c == 'f' || c == 'F')
        {
            Serial.println(F("\r\nFrequency divider"));
            if (testF4())
                Serial.println(F("Frequency divider passed"));
        }
    }
}