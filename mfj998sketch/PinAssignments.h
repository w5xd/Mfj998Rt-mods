#pragma once
/* The daughterboard plugs into the PIC18F2520 28pin DIP socket on the MFJ998 main board.
** 13 of those 28 are signals wired directly to Arduino pins (see the circuit diagram)
** The Arduino is a 32 pin part (and so has more pin capacity) but not enough to cover
** all of the PIC and also add the desired new functionality. So an MCP23S08 port extender
** on the Arduino's SPI bus helps out. */
namespace { // constants
    const int SPI_CLOCK_PIN = 13; // must match the CPU hardware because sketch uses the SPI library
    const int SPI_MOSI_PIN = 11; // ditto
    const int SPI_MISO_PIN = 12;    // ditto

    // MFJ998 mainboard pins connected to the Arduino
    const int FREQ_DIV4_PIN = 5;
    const int WAKEUP_PIN = 2;
    const int MFJ998_SDA_PIN = SPI_MOSI_PIN;
    const int MFJ998_CLOCK_AND_MODE_PIN = 9;

    // ... LCD display needs 7 pins
    //const int LCD_D4_PIN = on extender;
    //const int LCD_D5_PIN = on extender;
    //const int LCD_D6_PIN = on extender;
    //const int LCD_D7_PIN = on extender;
    const int LCD_RS_PIN = 4;
    const int LCD_RW_PIN = 7;
    const int LCD_E_PIN = 19;

    const int BUZZ_PIN = 18;

    // ... analog inputs
    const int FWD_ANALOG_PIN = A6; // Empirically: analogRead of 248 corresponds to 100W
    const int REF_ANALOG_PIN = A7;

    const int RELAYS_STROBE_PIN = 10; // on main board

    // front panel switches share pins mobo pins defined above
    const int ANT_BUTTON_PIN = LCD_RS_PIN;
    const int MODE_BUTTON_PIN = MFJ998_CLOCK_AND_MODE_PIN;
    const int CDOWN_BUTTON_PIN = LCD_RW_PIN;
    // these four on extender board
    //const int SW3_PIN = BUTTON_CUP;
    //const int SW5_PIN = BUTTON_LUP;
    //const int SW6_PIN = BUTTON_LDOWN;
    //const int SW7_PIN = BUTTON_TUNE;

    // SPI slaves on daughter board
    const int RFM69_SELECT_PIN = 6; // on daughter board
    const int EXTENDER_CS_PIN = 17;
    const int MEM512_SELECT_PIN = 8;    // on daughter board
    const int RFM69_INTERRUPT_PIN = 3;

    // stuff on MFJ998 main board that we ignore
    // const int MFJ998_START_PIN connected but not supported cuz
    //  we don't support the know the rig interfaces
    // const int MFJ998_SCL_PIN is not connected cuz
    //   we don't use the 25LC512 on the main board

    // these two would be hardware PWM, but the arduino pins that can do hardware
    // PWM are used elsewhere. This sketch uses Timer2 to generate interrupts for 
    // software-implemented PWM
    const int MREF_PIN = 14;
    const int MFWD_PIN = 15;

    //EEPROM locations in on-CPU EEPROM
    const int FREQ_CALI_OFFSET = 0; // beyond end of RadioConfiguration
    const int TRIGGER_SWR = FREQ_CALI_OFFSET + 2;
    const int STOP_SEARCH_SWR = TRIGGER_SWR + 2;

}

