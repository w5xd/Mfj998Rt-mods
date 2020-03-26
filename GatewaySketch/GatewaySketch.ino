#include <RFM69.h>      // the wireless transceiver
#include <SPI.h>        // The relay
#include <RadioConfiguration.h>

#define SERIAL_DEBUG 1
/*  See the mfj998sketch for more details.
** This sketch implents a serial to 900MHz gateway for
** two-way communication with the mfj998 sketch.
** 
** It can run on any Arduino hardware that also has support for
**  Serial library
**  EEPROM library
**  RFM69 library. (which in turn requires SPI)
*/

class RadioRFM69 :  public RFM69
{
public:
    RadioRFM69() : RFM69(RF69_SPI_CS, RF69_IRQ_PIN, true)
    {}

    void setup();
    
    bool loop()
    {
        if (receiveDone()) // Got one!
        {
            Serial.print("N="); Serial.print(SENDERID, DEC);
            Serial.print(" R="); Serial.print(RSSI);
            Serial.print(" [");

            // The actual message is contained in the DATA array,
            // and is DATALEN bytes in size:

            for (byte i = 0; i < DATALEN; i++)
                Serial.print((char)DATA[i]);
            Serial.println("]");
            if (ACKRequested())
            {
                sendACK();
#if SERIAL_DEBUG > 0
                Serial.println("ACK sent");
#endif
            }
        }
    }
protected:
};

namespace {
    RadioRFM69 radio;
    RadioConfiguration radioConfiguration;
    const int AM_GATEWAY_NODEID = 1;
    const int HOST_BUFFER_LENGTH = 64;

    bool processHostCommand(const char *pHost, unsigned short count)
    {
        static const char SENDMESSAGETONODE[] = "SendMessageToNode";
        if (strncmp(pHost, SENDMESSAGETONODE, sizeof(SENDMESSAGETONODE) - 1) == 0)
        {	// send a message to a node now.
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
                    Serial.print(SENDMESSAGETONODE);
                    Serial.print(" ");
                    Serial.print(nodeId, DEC);
                    Serial.flush();
                    bool acked = radio.sendWithRetry(nodeId, pHost, --count);
#if SERIAL_DEBUG > 0
                    Serial.println(acked? " ACK" : " NAC");
#endif
                 }
            }
            return true;
        }
        else
            return false;
    }
}

void RadioRFM69::setup()
{
    initialize(
        radioConfiguration.FrequencyBandId(),
        AM_GATEWAY_NODEID,	// we're the gateway. we use a non-programmable nodeId
        radioConfiguration.NetworkId());

    uint32_t freq;
    if (radioConfiguration.FrequencyKHz(freq))
        setFrequency(1000*freq);

    setHighPower(); // Always use this for RFM69HCW
    const char *key = radioConfiguration.EncryptionKey();
    if (radioConfiguration.encrypted())
        encrypt(key);
}

void setup()
{
    Serial.begin(9600);
    Serial.print("MFJ998 Gateway ");
    Serial.print(AM_GATEWAY_NODEID, DEC);
    Serial.print(" on network ");
    Serial.print(radioConfiguration.NetworkId(), DEC);
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
    radio.setup();
    Serial.print(" Radio on ");
    Serial.print(radio.getFrequency() / 1000);
    Serial.println(" KHz");
    Serial.println(" ready");
}

void loop()
{
    static char fromHostbuffer[HOST_BUFFER_LENGTH + 1]; // +1 so we can add trailing null
    static int charsInBuf = 0;

    while (Serial.available() > 0)
    {
        char input = Serial.read();
        bool isRet = (input == (char)'\r') || (input == (char)'\n');
        if (!isRet) 
        {
            fromHostbuffer[charsInBuf] = input;
            charsInBuf++;
        }

        // If the input is a carriage return, or the buffer is full:
        if (isRet || (charsInBuf == sizeof(fromHostbuffer) - 1)) // CR or buffer full
        {
            fromHostbuffer[charsInBuf] = 0;
            if (radioConfiguration.ApplyCommand(fromHostbuffer))
            {
                Serial.print(fromHostbuffer);
                Serial.println(" command accepted for radio");
            }
            else if (processHostCommand(fromHostbuffer, charsInBuf))
            {
            }
            else
                Serial.println("Invalid command");
            charsInBuf = 0; // reset the packet
        }
    }

    radio.loop();
}
