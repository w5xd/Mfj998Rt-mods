#include <Arduino.h>
#include "eepromSpi.h"
const uint8_t EepromSPI::CMD_READ =     0b11;
const uint8_t EepromSPI::CMD_WRITE =    0b10;
const uint8_t EepromSPI::CMD_WREN =    0b110;
const uint8_t EepromSPI::CMD_WRDI =    0b100;
const uint8_t EepromSPI::CMD_RDSR =    0b101;
const uint8_t EepromSPI::CMD_WRSR =      0b1;
const uint8_t EepromSPI::CMD_PE =  0b1000010;
const uint8_t EepromSPI::CMD_SE = 0b11011000;
const uint8_t EepromSPI::CMD_CE = 0b11000111;
const uint8_t EepromSPI::CMD_RDID=0b10101011;
const uint8_t EepromSPI::CMD_DPD =0b10111001;

// test routine. takes an unreasonable amount of memory.
// do not call in production builds.
void EepromSPI::testWritePageBoundaries()
{
    static unsigned char testpattern[258];
    static unsigned char readbuffer[sizeof(testpattern)];
    static const uint16_t testAddr = 0x52; // spans 3 by 128byte pages

    for (int i = 0; i < sizeof(testpattern); i++)
        testpattern[i] = rand();
    if (writeAcrossPageBoundary(testAddr, testpattern, sizeof(testpattern)))
    {
        if (waitReadyForWrite())
        {
            read(testAddr, readbuffer, sizeof(readbuffer));
            for (int i = 0; i < sizeof(testpattern); i++)
            {
                if (readbuffer[i] != testpattern[i])
                {
                    Serial.print(F("Read test pattern failed i="));
                    Serial.print((int)i);
                    Serial.print(F(" t=0x"));
                    Serial.print((unsigned int)testpattern[i], HEX);
                    Serial.print(F(" b=0x"));
                    Serial.println((unsigned int)readbuffer[i], HEX);
                    return;
                }
            }
            Serial.println(F("testWritePageBoundaries PASSED."));
        }
        else
            Serial.println(F("testWritePageBoundaries FAILED waitReadyForWrite"));
    }
    else
        Serial.println(F("writeAcrossPageBoundary failed"));
}