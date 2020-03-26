#pragma once
// support for the MCP23S08 port extender chip on the MFJ998 daughterboard
template <typename BASE>
class PortExtender : public BASE
{
public:
    // bit numbers in GP as wired
    enum { LCD_D4 = 0, LCD_D5 = 1, LCD_D6 = 2, LCD_D7 = 3, PD=6, WAKEUP = 7, SPARE_1 = 4, SPARE_2 = 5,
        BUTTON_LDOWN = LCD_D4, BUTTON_TUNE = LCD_D5, BUTTON_LUP = LCD_D6, BUTTON_CUP = LCD_D7};

    template <typename A1, typename A2, typename A3, typename A4,
            typename A5, typename A6, typename A7>
    PortExtender(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7) 
        : BASE(a1, a2, a3,a4, a5, a6, a7) 
        , m_LCDportsAreInputs(true)
    {}

    PortExtender() : m_LCDportsAreInputs(true)
    {}
    
    void setup()
    {
        digitalWrite(EXTENDER_CS_PIN, HIGH); // chip not selected
        pinMode(EXTENDER_CS_PIN, OUTPUT);
        SPI.beginTransaction(settings);
        digitalWrite(EXTENDER_CS_PIN, LOW); // extender
        SPI.transfer(START_WRITE);
        SPI.transfer(REG_IODIR); // start at bottom register
        SPI.transfer(0xff); // IODIR init as all inputs
        SPI.transfer(0); // IPOL is all right-side-up
        SPI.transfer((1 << WAKEUP) | (1 << PD)); // GPINTEN interrupt on change for WAKEUP and pd
        SPI.transfer(1 << WAKEUP); // DEFVAL WAKEUP is idle HIGHT while PD is idle low.
        SPI.transfer((1 << WAKEUP) | (1 << PD)); // INTCON. interrupt when value does not match DEFVAL
        SPI.transfer(1 << IOC_HAEN); // IOCON sequential enabled, slew rate off, honor hardware addr, active INT output, active low
        SPI.transfer((1 << WAKEUP) | (1 << LCD_D4) | (1 << LCD_D5) | (1 << LCD_D6) | (1 << LCD_D7) | // GPPU enable 100K pullup on these pins
                (1 << SPARE_1) | (1 << SPARE_2));
        digitalWrite(EXTENDER_CS_PIN, HIGH); // unselect
        SPI.endTransaction();
        m_LCDportsAreInputs = true;
   }

    void getRegisters(uint8_t registers[0xa])
    {
        SPI.beginTransaction(settings);
        digitalWrite(EXTENDER_CS_PIN, LOW); // extender
        SPI.transfer(START_READ);
        SPI.transfer(REG_IODIR); // start at bottom register
        for (int i = 0; i < REG_OLAT; i++)
            registers[i] = SPI.transfer(0);
        digitalWrite(EXTENDER_CS_PIN, HIGH); // unselect
        SPI.endTransaction();
    }

    uint8_t getPins()
    {
        setLCDpinsAsInputs();
        SPI.beginTransaction(settings);
        digitalWrite(EXTENDER_CS_PIN, LOW); // extender
        SPI.transfer(START_READ);
        SPI.transfer(REG_GPIO); // register to read
        uint8_t ret = SPI.transfer(0); 
        digitalWrite(EXTENDER_CS_PIN, HIGH); // unselect
        SPI.endTransaction();
        return ret;
    }

    uint8_t getCapturedInterruptMask()
    {
        SPI.beginTransaction(settings);
        digitalWrite(EXTENDER_CS_PIN, LOW); // extender
        SPI.transfer(START_READ);
        SPI.transfer(REG_INTCAP); // register to read
        uint8_t ret = SPI.transfer(0); 
        digitalWrite(EXTENDER_CS_PIN, HIGH); // unselect
        SPI.endTransaction();
        return ret;
    }

    uint8_t getCurrentInterruptMask()
    {
        SPI.beginTransaction(settings);
        digitalWrite(EXTENDER_CS_PIN, LOW); // extender
        SPI.transfer(START_READ);
        SPI.transfer(REG_INTF); // register to read
        uint8_t ret = SPI.transfer(0); 
        digitalWrite(EXTENDER_CS_PIN, HIGH); // unselect
        SPI.endTransaction();
        return ret;
    }

    void setLCDpinsAsInputs()
    {
        if (m_LCDportsAreInputs)
            return;
        SPI.beginTransaction(settings);
        digitalWrite(EXTENDER_CS_PIN, LOW); // extender
        SPI.transfer(START_WRITE);
        SPI.transfer(REG_IODIR); // start at bottom register
        SPI.transfer(0xff); // IODIR--LCD4-LCD7 are now inputs
        digitalWrite(EXTENDER_CS_PIN, HIGH); // unselect
        SPI.endTransaction();
        m_LCDportsAreInputs = true;
    }

    void write4bits(uint8_t v) override
    {
        setLCDpinsAsOutputs();
        SPI.beginTransaction(settings);
        digitalWrite(EXTENDER_CS_PIN, LOW); // extender
        SPI.transfer(START_WRITE);
        SPI.transfer(REG_GPIO); // start at GPIO register
        SPI.transfer(v); // IODIR--LCD4-LCD7 are the outputs
        digitalWrite(EXTENDER_CS_PIN, HIGH); // unselect
        SPI.endTransaction();
        pulseEnable(); // base clase function
        return 1;
    }

    static const SPISettings settings;

protected:
    void setLCDpinsAsOutputs()
    {
        if (!m_LCDportsAreInputs)
            return;
        SPI.beginTransaction(settings);
        digitalWrite(EXTENDER_CS_PIN, LOW); // extender
        SPI.transfer(START_WRITE);
        SPI.transfer(REG_IODIR); // start at bottom register
        SPI.transfer(0xf0); // IODIR--LCD4-LCD7 are now outputs
        digitalWrite(EXTENDER_CS_PIN, HIGH); // unselect
        SPI.endTransaction();
        m_LCDportsAreInputs = false;
    }

    enum {START_WRITE = 0x40, START_READ};
    // device registers send in 2nd byte. per device documentation
    enum { REG_IODIR=0, REG_IPOL, REG_GPINTEN, REG_DFVAL, REG_INTCON, REG_IOCON, REG_GPPU, REG_INTF, REG_INTCAP, REG_GPIO, REG_OLAT};
    // bit numbers for the IOCON register bits
    enum {IOC_SEQOP = 5, IOC_DISSLW = 4, IOC_HAEN = 3, IOC_ODR = 2, IOC_INTPOL = 1};

    bool m_LCDportsAreInputs;
    static const int EXTENDER_CS_PIN;

};

