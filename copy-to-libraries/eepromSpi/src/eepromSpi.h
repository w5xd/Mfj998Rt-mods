#pragma once
#include <SPI.h>        
class EepromSPI {
    // see Microchip 25LC512
public:
    enum {PAGE_LENGTH = 128, PAGE_MASK = 0x7F};
    EepromSPI() 
    {}

    void init()
    {
        digitalWrite(MEM512_SELECT_PIN, HIGH);
        pinMode(MEM512_SELECT_PIN, OUTPUT);
    }

    void read(uint16_t addr, uint8_t *values, uint16_t count)
    {
        SPI.beginTransaction(settings);
        digitalWrite(MEM512_SELECT_PIN,LOW);
        SPI.transfer(CMD_READ);
        SPI.transfer(addr>>8);
        SPI.transfer(addr);
        for (uint16_t i = 0; i < count; i++)
            values[i] = SPI.transfer(0);
        digitalWrite(MEM512_SELECT_PIN,HIGH);
        SPI.endTransaction();
    }

    void write(uint16_t addr, const uint8_t *values, uint8_t count)
    {
        SPI.beginTransaction(settings);
        digitalWrite(MEM512_SELECT_PIN,LOW);
        SPI.transfer(CMD_WREN); // write enable command
        digitalWrite(MEM512_SELECT_PIN,HIGH);
        digitalWrite(MEM512_SELECT_PIN,LOW);
        SPI.transfer(CMD_WRITE);    // write command
        SPI.transfer(addr>>8);
        SPI.transfer(addr);
        for (uint8_t i = 0; i < count; i++)
            SPI.transfer(values[i]);
        digitalWrite(MEM512_SELECT_PIN,HIGH);
        SPI.endTransaction();
    }

    bool writeAcrossPageBoundary(uint16_t addr, const uint8_t *values, uint16_t count)
    {
        // you call waitReadyForWrite first
        // we'll call it again, if necessary and return false if it won't work
        // and we can write longer buffers if such a thing exists...
        uint16_t lastAddr = addr;
        lastAddr += count - 1;
        uint16_t lastPage = lastAddr & ~PAGE_MASK;
        uint16_t firstPage = addr & ~PAGE_MASK;
        if (lastPage == firstPage)
            write(addr, values, count); // fits in a page
        else
        {
            uint8_t toWrite = PAGE_LENGTH + firstPage - addr;
            write(addr, values, toWrite);
            for (;;)
            {
                count -= toWrite;
                if (count == 0)
                    break;
                values += toWrite;
                addr += toWrite;
                toWrite = count > PAGE_LENGTH ? PAGE_LENGTH : count;
                if (waitReadyForWrite())
                    write(addr, values, toWrite);
                else
                    return false;
            }
        }
        return true;
    }

    enum {WRITE_IN_PROGRESS_BITNUM = 0};
    uint8_t status()
    {
        SPI.beginTransaction(settings);
        digitalWrite(MEM512_SELECT_PIN,LOW);
        SPI.transfer(CMD_RDSR);
        uint8_t r = SPI.transfer(0);
        digitalWrite(MEM512_SELECT_PIN,HIGH);
        SPI.endTransaction();
        return r;
    }

    bool waitReadyForWrite(int maxWait = 6)
    {
        for (int i = 0; i < maxWait; i++)
        {
            if ((status() & (1 << WRITE_IN_PROGRESS_BITNUM)) == 0)
                return true;
            delay(1);
        }
        return false;
    }

    void setWriteEnable()
    {
        SPI.beginTransaction(settings);
        digitalWrite(MEM512_SELECT_PIN,LOW);
        SPI.transfer(CMD_WREN);
        digitalWrite(MEM512_SELECT_PIN,HIGH);
        SPI.endTransaction();
    }

    void chipErase()
    {   // must be immediately preceeded by setWriteEnable
        SPI.beginTransaction(settings);
        digitalWrite(MEM512_SELECT_PIN,LOW);
        SPI.transfer(CMD_CE);
        digitalWrite(MEM512_SELECT_PIN,HIGH);
        SPI.endTransaction();
    }

    void testWritePageBoundaries();

    static const SPISettings settings;

protected:
    // must be defined in sketch ino file
    static const int MEM512_SELECT_PIN;
    // defined in our CPP file
    static const uint8_t CMD_READ;
    static const uint8_t CMD_WRITE;
    static const uint8_t CMD_WREN;
    static const uint8_t CMD_WRDI;
    static const uint8_t CMD_RDSR;
    static const uint8_t CMD_WRSR;
    static const uint8_t CMD_PE;
    static const uint8_t CMD_SE;
    static const uint8_t CMD_CE;
    static const uint8_t CMD_RDID;
    static const uint8_t CMD_DPD;
};

