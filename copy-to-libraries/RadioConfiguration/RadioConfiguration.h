#pragma once
#include <EEPROM.h> // depends on the Arduinio EEPROM library

/* class to read RFM69 radio configuration from EEPROM
** AND to parse text commands that set the configuration parameters.
** Presumably, the sketch reads those commands from the serial port.
** Note that writing the EEPROM does nothing to the radio. It must
** be reinitialized when its configuration parameters are changed.
*/
class RadioConfiguration
{
public:
    enum { ENCRYPT_KEY_LENGTH = 16 };
    RadioConfiguration(uint16_t EEpromOffset = 0 /* default to beginning of EEPROM*/)
        : m_EEpromOffset(EEpromOffset)
    {}

    // read the NodeId from the EEPROM
    uint8_t NodeId()
    {
    	return (uint8_t)EEPROM.read(m_EEpromOffset + NODEID_POSITION);
    }

    // read the NetworkId from the EEPROM
    uint8_t NetworkId()
    {
    	return (uint8_t)EEPROM.read(m_EEpromOffset + NETWORKID_POSITION);
    }

    // read the Frequency Band ID from the EEPROM
    uint8_t FrequencyBandId()
    {
    	return (uint8_t)EEPROM.read(m_EEpromOffset + FREQUENCYBAND_POSITION);
    }

    bool FrequencyKHz(uint32_t &ret)
    {
    	EEPROM.get(m_EEpromOffset + FREQUENCYKHZ_POSITION, ret);
        return (ret >= 890000) && (ret <= 1020000);
    }

    // read the encryption key from the EEPROM
    const char *EncryptionKey()
    {
        EEPROM.get(m_EEpromOffset + ENCRYPTION_KEY_POSITION, m_EncryptionKey);
        return &m_EncryptionKey[0];
    }

    bool encrypted() const
    {
        for (unsigned i = 0; i < sizeof(m_EncryptionKey); i++)
            if (m_EncryptionKey[i] != (char)0xff)
                return true;
        return false;
    }

    // parse text command. If it is one of "our" commands,
    // compute the new configuration value and write it to EEPROM
    // 
    bool ApplyCommand(const char *pCmd)
    {
        static const int NUM_BYTE_COMMANDS = 3;

        // MUST BE SAME ORDER AS enum NODEID_POSITION below
        static const char SET_NODEID[] = "SetNodeId";
        static const char SET_NETWORKID[] = "SetNetworkId";
        static const char SET_FREQBAND[] = "SetFrequencyBand";

        static const char * const Commands[NUM_BYTE_COMMANDS] = {
            SET_NODEID, SET_NETWORKID, SET_FREQBAND };
        static const unsigned CommandSizes[NUM_BYTE_COMMANDS] = {
            sizeof(SET_NODEID) - 1,
            sizeof(SET_NETWORKID) - 1,
            sizeof(SET_FREQBAND) - 1,
        };

        for (unsigned i = 0; i < NUM_BYTE_COMMANDS; i++)
        {
            if (strncmp(pCmd, Commands[i], CommandSizes[i]) == 0)
            {
                pCmd = SkipWhiteSpace(pCmd + CommandSizes[i]);
                if (pCmd)
                {
                    unsigned v = toDecimalUnsigned(pCmd);
                    // zero is not allowed value for any of these configuration settings
                    if ((v > 0) && (v <= 255))
                    {
                        EEPROM.write(m_EEpromOffset + i, v);
                        return true;
                    }
                }
                return false;
            }
        }

        static const char SET_KEY[] = "SetEncryptionKey";
        if (strncmp(pCmd, SET_KEY, sizeof(SET_KEY) - 1) == 0)
        {
            pCmd = SkipWhiteSpace(pCmd + sizeof(SET_KEY) - 1);
            memset(m_EncryptionKey, -1, sizeof(m_EncryptionKey)); // default off
            if (pCmd)
            {
                if (pCmd[0] == '0' && pCmd[1] == 'x')
                {   // support hex string starting with 0x
                    unsigned thisByte(0);
                    pCmd += 2; // skip 0x
                    for (unsigned idx = 0; idx < ENCRYPT_KEY_LENGTH * 2; idx++)
                    {
                        if (isdigit(*pCmd))
                            thisByte += *pCmd++ - '0';
                        else if (*pCmd >= 'a' && *pCmd <= 'f')
                            thisByte += *pCmd++ - 'a' + 10;
                        if (1 & idx)
                        {
                            m_EncryptionKey[idx / 2] = (char)thisByte;
                            thisByte = 0;
                        }
                        else
                            thisByte <<= 4;
                    }
                }
                else
                {   // support ascii pass phrase
                    for (unsigned idx = 0; idx < ENCRYPT_KEY_LENGTH;)
                    {
                        if ((m_EncryptionKey[idx++] = *pCmd)) // yes, test if assignment is non-zero
                            pCmd++;
                    }
                }
            }
            EEPROM.put(m_EEpromOffset + ENCRYPTION_KEY_POSITION, m_EncryptionKey);
            return true;
        }

        static const char SET_FREQUENCY[] = "SetFrequencyKhz";
        if (strncmp(pCmd, SET_FREQUENCY, sizeof(SET_FREQUENCY)-1) == 0)
        {
            pCmd = SkipWhiteSpace(pCmd + sizeof(SET_FREQUENCY)-1);
            if (pCmd)
            {
                uint32_t v = toDecimalUnsigned(pCmd);
                if (v != 0)
                {
                    EEPROM.put(m_EEpromOffset + FREQUENCYKHZ_POSITION, v);
                    return true;
                }
            }
        }
        return false;
    }

    void printEncryptionKey(Print &p)
    {
        EncryptionKey();
        if (!encrypted())
            return;
        bool isPrint = true;
        for (int i = 0; i < ENCRYPT_KEY_LENGTH; i++)
        {
            if (!isPrintable(m_EncryptionKey[i]))
            {
                isPrint = false;
                break;
            }
        }
        if (isPrint)
            for (int i = 0; i < ENCRYPT_KEY_LENGTH; i++)
                p.print(m_EncryptionKey[i]);
        else
        {
            p.print("0x");
            for (int i = 0; i < ENCRYPT_KEY_LENGTH; i++)
            {
                unsigned char low = m_EncryptionKey[i] & 0xF;
                unsigned char high = m_EncryptionKey[i] & 0xF0; high >>= 4;
                if (high < 10)
                    p.print(static_cast<char>('0' + high));
                else
                    p.print(static_cast<char>('a' + high - 10));
                if (low < 10)
                    p.print(static_cast<char>('0' + low));
                else
                    p.print(static_cast<char>('a' + low - 10));
            }
        }

    }

    // return pointer to character after white space, if any
    static const char *SkipWhiteSpace(const char *p)
    {
        const char *pRet(0);
        while (isspace(*p))
            pRet = ++p;
        return pRet;
    }

    static uint32_t toDecimalUnsigned(const char *p)
    {
        uint32_t ret = 0;
        while (isdigit(*p))
        {
            ret *= 10;
            ret += *p++ - '0';
        }
        return ret;
    }

    static unsigned TotalEpromUsed() { return TOTAL_EEPROM_USED;}
    enum EepromAddresses {
        NODEID_POSITION = 0,
        NETWORKID_POSITION = NODEID_POSITION + sizeof(uint8_t),
        FREQUENCYBAND_POSITION = NETWORKID_POSITION + sizeof(uint8_t),
        ENCRYPTION_KEY_POSITION = FREQUENCYBAND_POSITION + sizeof(uint8_t),
        FREQUENCYKHZ_POSITION = ENCRYPTION_KEY_POSITION + ENCRYPT_KEY_LENGTH,
        TOTAL_EEPROM_USED = FREQUENCYKHZ_POSITION + sizeof(uint32_t)
    };

protected:

    const uint16_t m_EEpromOffset; // offset into the EEPROM for the radio configuration
    char m_EncryptionKey[ENCRYPT_KEY_LENGTH];
};
