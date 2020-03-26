#include <Arduino.h>
#include "Mfj998Relays.h"
// make masks from bit numbers
const uint32_t Relays::C0 = 1ul << (int)BitNumber::C15pF_K27_28;
const uint32_t Relays::C1 = 1ul << (int)BitNumber::C31pF_K17_18;
const uint32_t Relays::C2 = 1ul << (int)BitNumber::C62pF_K25_26;
const uint32_t Relays::C4 = 1ul << (int)BitNumber::C124pF_K23_24;
const uint32_t Relays::C8 = 1ul << (int)BitNumber::C248pF_K21_22;
const uint32_t Relays::C16 = 1ul << (int)BitNumber::C496pF_K19_20;
const uint32_t Relays::C32 = 1ul << (int)BitNumber::C1000pF_K29;
const uint32_t Relays::C64 = 1ul << (int)BitNumber::C1950pF_K30;
const uint32_t Relays::C_MASK = C0 | C1 | C2 | C4 | C8 | C16 | C32 | C64;

const uint32_t Relays::L0 = 1 << (int)BitNumber::L95nH_K10_11;    // preferred 95nH
const uint32_t Relays::L01 = 1 << (int)BitNumber::L95nH_K8; // small values and fills gaps
const uint32_t Relays::L1 = 1 << (int)BitNumber::L190nH_K6;
const uint32_t Relays::L2 = 1 << (int)BitNumber::L380nH_K7;
const uint32_t Relays::L4 = 1 << (int)BitNumber::L760nH_K3;
const uint32_t Relays::L8 = 1 << (int)BitNumber::L1520nH_K4;
const uint32_t Relays::L16 = 1 << (int)BitNumber::L3040nH_K5;
const uint32_t Relays::L32 = 1 << (int)BitNumber::L6080nH_K9;
const uint32_t Relays::L64 = 1 << (int)BitNumber::L12160nH_K12;
const uint32_t Relays::L_MASK = L0 | L01 | L1 | L2 | L4 | L8 | L16 | L32 | L64;

static_assert((Relays::C_MASK & Relays::L_MASK) == 0, "Conflicting relay defintions");
static_assert(Relays::C_MASK == 0x3F8010L, "capacitor mask not right yet");

bool Relays::setLindex(uint16_t idx)
{   // set the inductance value
    // the mapping from idx to uHenries is in the lookup table below.
    // But the short answer is its about 95nH time idx (which ranges to 258)
    if (idx > MAX_LINDEX)
        return false;
    relaystate &= ~L_MASK;
    uint16_t maskValue = pgm_read_word_near(LVALUES + idx);
    relaystate |= maskValue;
    lIndex = idx;
    return true;
}

bool Relays::setCindex(uint8_t idx)
{   // set the capacitance value
    // Compared to the L values, the scaling is easy:
    //  C = 15.5pF times idx with a maximum idx of 255.
    if (idx > getMaxCindex())
        return false;

    cIndex = idx;
    relaystate &= ~C_MASK;
    if (idx == 0)
    {   // zero capacitance special case: turn off both GEN/LOAD relays
        // ...yet leave capstate where it was
        relaystate &= ~(1ul << (int)BitNumber::K15_16_C_GENERATOR);
        relaystate &= ~(1ul << (int)BitNumber::K13_14_C_LOAD);
    }
    else
    {
        uint8_t one = 1;
        static const uint32_t CMASKS[8] PROGMEM= 
        {        C0, C1, C2, C4, C8, C16, C32, C64    };
        for (int i = 0; i < 8; i++)
        {
            if (0 != (one & idx))
            {
                uint32_t m = pgm_read_dword_near(CMASKS+i);
                relaystate |= m;
            }
            one <<= 1;
        }
        setCapRelayState(getCapRelayState());
    }
    return true;
}
/* setCindex and setCapRelayState call each other!
** There are NOT intended to recurse. Each should only call the other when
** the other's conditions need changing.
*/
void Relays::setCapRelayState(CapRelayState v) {
    capstate = v;
    relaystate &= ~(1ul << (int)BitNumber::K15_16_C_GENERATOR);
    relaystate &= ~(1ul << (int)BitNumber::K13_14_C_LOAD);
    if (cIndex != 0)// cIndex zero is special case: leave relays alone
    {   
        switch (v) {
                case CapRelayState::STATE_C_LOAD:
                    relaystate |= 1ul << (int)BitNumber::K13_14_C_LOAD;
                    if (cIndex > MAX_LOAD_CINDEX)
                        setCindex(MAX_LOAD_CINDEX); // check MAX_LOAD_CINDEX           
                    break;
                case CapRelayState::STATE_C_GENERATOR:
                    relaystate |= 1ul << (int)BitNumber::K15_16_C_GENERATOR;
                    break;
                case CapRelayState::STATE_BYPASS: // leave both relays off
                    break;
                // don't turn both relays on...
        }
    }
}

/* How can it be there are 258 values in the array of L values?
**
** There are 9 relays: L0 through L64 are 8 of them (first 8 powers of two.)
** Then there is L01. it "matches" the value of L0.
** It is not used as much. These factoids were determined by a DVM on a working
** MFJ998 with factory firmware and using the Lup and Ldown buttons:
**  a) L01 is EXCLUSIVELY used in combination with L0
**  b) small L values: .76 uH and below, the L0|L01 combination is used
**  wherever their inductance sum (.095 * 2) is needed, even if that means
**  not using L1 (at .190), or not using L2 (at .380)
**  c) The 1.52 uH value is gotten not with L8 alone, but instead with L01 combined
**  with all of L0 through L4
**  d) The 3.04 uH value is gotten not with L16 alone, but with L01 combined
**  with all of L0 through L8.
**  e) The 6.08 uH value is gotten not with L32 alone, but with L01 combined
**  with all of L0 through L16
**  f) extra#1. There is a gap because L64 is not actually 12.16uH. It is instead 12.22uH.
**      L01 is combined with all of L0 through L32 to fill that gap. 
**  g) extra#2. The final, highest inductance value in the table, is all the L's
**  The two extras and the 256 combinations of L0 through L64 make 258 unique values.
** "extra" means that the MFJ998 front panel Lup/Ldown go through 256 values including zero.
**
** The toroids are probably not 1% tolerance parts, so all the verbiage above and
** and the values in the tables below should be taken with a grain of salt. However,
** The author did confirm with an antenna analyzer set about 3MHz that as the index into 
** LVALUES increases, so does the total inductance. That monotonic increase is all that is
** important to the firmware. But even the assertion of monotonic progression must
** be taken with a grain of salt, especially as you consider higher frequencies.
** The relays change capacitances as well as inductance, so something more complex
** is happening as the relays switch. Its modeled in this sketch as if it
** were as drawn on the circuit diagram: a series inductor and a capacitance to ground.
**
** The table below converts an index in the range 0 through MAX_LINDEX into a mask
** of the relay bits to turn on/off.
**
** optimized: the relay shift register is 22 bits wide...but the L's are all in the
** bottom 16, therefore this table need only be 16bits wide per table entry.
*/

const uint16_t Relays::LVALUES[] PROGMEM = {
    0,                                  //    minimum inductance
    L0,                                 //  .09
    L0|L01,                             //  .19
    L0|    L1,                          //  .28
    L0|L01|L1,                          //  .38
    L0|       L2,                       //  .47
           L1|L2,                       //  .57
    L0|    L1|L2,                       //  .66
    L0|L01|L1|L2,                       //  .76
    L0|          L4,                    //  .85
           L1|   L4,                    //  .95 
    L0|    L1|   L4,                    // 1.04
              L2|L4,                    // 1.14
    L0|       L2|L4,                    // 1.23
           L1|L2|L4,                    // 1.33
    L0|    L1|L2|L4,                    // 1.42
    L0|L01|L1|L2|L4,                    // 1.52  -- exception  16
    L0|             L8,                 // 1.61
           L1|      L8,                 // 1.71
    L0|    L1|      L8,                 // 1.80
              L2|   L8,                 // 1.90
    L0|       L2|   L8,                 // 1.99
           L1|L2|   L8,                 // 2.09
    L0|    L1|L2|   L8,                 // 2.18
                 L4|L8,                 // 2.28
    L0|          L4|L8,                 // 2.37
           L1|   L4|L8,                 // 2.47
    L0|    L1|   L4|L8,                 // 2.56
              L2|L4|L8,                 // 2.66
    L0|       L2|L4|L8,                 // 2.75
           L1|L2|L4|L8,                 // 2.85
    L0|    L1|L2|L4|L8,                 // 2.94
    L0|L01|L1|L2|L4|L8,                 // 3.04 -- exception 32
    L0|                L16,             // 3.13
           L1|         L16,   
    L0|    L1|         L16,
              L2|      L16,
    L0|       L2|      L16,
           L1|L2|      L16,
    L0|    L1|L2|      L16,
                 L4|   L16,
    L0|          L4|   L16,
           L1|   L4|   L16,
    L0|    L1|   L4|   L16,
              L2|L4|   L16,
    L0|       L2|L4|   L16,
           L1|L2|L4|   L16,
    L0|    L1|L2|L4|   L16,              // 4.46
                    L8|L16,              // 4.56
    L0|             L8|L16,
           L1|      L8|L16,
    L0|    L1|      L8|L16,
              L2|   L8|L16,
    L0|       L2|   L8|L16,
           L1|L2|   L8|L16,
    L0|    L1|L2|   L8|L16,
                 L4|L8|L16,
    L0|          L4|L8|L16,
           L1|   L4|L8|L16,
    L0|    L1|   L4|L8|L16,
              L2|L4|L8|L16,
    L0|       L2|L4|L8|L16,
           L1|L2|L4|L8|L16,
    L0|    L1|L2|L4|L8|L16,
    L0|L01|L1|L2|L4|L8|L16,             // 6.08 -- exception
    L0|                    L32,         // 6.17
           L1|             L32,   
    L0|    L1|             L32,
              L2|          L32,
    L0|       L2|          L32,
           L1|L2|          L32,
    L0|    L1|L2|          L32,
                 L4|       L32,
    L0|          L4|       L32,
           L1|   L4|       L32,
    L0|    L1|   L4|       L32,
              L2|L4|       L32,
    L0|       L2|L4|       L32,
           L1|L2|L4|       L32,
    L0|    L1|L2|L4|       L32, 
                    L8|    L32, 
    L0|             L8|    L32,
           L1|      L8|    L32,
    L0|    L1|      L8|    L32,
              L2|   L8|    L32,
    L0|       L2|   L8|    L32,
           L1|L2|   L8|    L32,
    L0|    L1|L2|   L8|    L32,
                 L4|L8|    L32,
    L0|          L4|L8|    L32,
           L1|   L4|L8|    L32,
    L0|    L1|   L4|L8|    L32,
              L2|L4|L8|    L32,
    L0|       L2|L4|L8|    L32,
           L1|L2|L4|L8|    L32,
    L0|    L1|L2|L4|L8|    L32,            // 9.02
                       L16|L32,            // 9.12
    L0|                L16|L32,            // 9.21
           L1|         L16|L32,   
    L0|    L1|         L16|L32,   
              L2|      L16|L32,   
    L0|       L2|      L16|L32,   
           L1|L2|      L16|L32,   
    L0|    L1|L2|      L16|L32,   
                 L4|   L16|L32,   
    L0|          L4|   L16|L32,   
           L1|   L4|   L16|L32,   
    L0|    L1|   L4|   L16|L32,   
              L2|L4|   L16|L32,   
    L0|       L2|L4|   L16|L32,   
           L1|L2|L4|   L16|L32,   
    L0|    L1|L2|L4|   L16|L32,   
                    L8|L16|L32,   
    L0|             L8|L16|L32,   
           L1|      L8|L16|L32,   
    L0|    L1|      L8|L16|L32,   
              L2|   L8|L16|L32,   
    L0|       L2|   L8|L16|L32,   
           L1|L2|   L8|L16|L32,   
    L0|    L1|L2|   L8|L16|L32,   
                 L4|L8|L16|L32,   
    L0|          L4|L8|L16|L32,   
           L1|   L4|L8|L16|L32,   
    L0|    L1|   L4|L8|L16|L32,   
              L2|L4|L8|L16|L32,   
    L0|       L2|L4|L8|L16|L32,   
           L1|L2|L4|L8|L16|L32,   
    L0|    L1|L2|L4|L8|L16|L32,            //12.06
    L0|L01|L1|L2|L4|L8|L16|L32,            //12.22 -- exception...but is actually 12.16 
                               L64,        //not used in MFJ firmware up/down
    L0|                        L64,        //12.31
           L1|                 L64,   
    L0|    L1|                 L64,
              L2|              L64,
    L0|       L2|              L64,
           L1|L2|              L64,
    L0|    L1|L2|              L64,
                 L4|           L64,
    L0|          L4|           L64,
           L1|   L4|           L64,
    L0|    L1|   L4|           L64,
              L2|L4|           L64,
    L0|       L2|L4|           L64,
           L1|L2|L4|           L64,
    L0|    L1|L2|L4|           L64, 
                    L8|        L64, 
    L0|             L8|        L64,
           L1|      L8|        L64,
    L0|    L1|      L8|        L64,
              L2|   L8|        L64,
    L0|       L2|   L8|        L64,
           L1|L2|   L8|        L64,
    L0|    L1|L2|   L8|        L64,
                 L4|L8|        L64,
    L0|          L4|L8|        L64,
           L1|   L4|L8|        L64,
    L0|    L1|   L4|L8|        L64,
              L2|L4|L8|        L64,
    L0|       L2|L4|L8|        L64,
           L1|L2|L4|L8|        L64,
    L0|    L1|L2|L4|L8|        L64,
                       L16|    L64,
    L0|                L16|    L64,
           L1|         L16|    L64,
    L0|    L1|         L16|    L64,
              L2|      L16|    L64,
    L0|       L2|      L16|    L64,
           L1|L2|      L16|    L64,
    L0|    L1|L2|      L16|    L64,
                 L4|   L16|    L64,
    L0|          L4|   L16|    L64,
           L1|   L4|   L16|    L64,
    L0|    L1|   L4|   L16|    L64,
              L2|L4|   L16|    L64,
    L0|       L2|L4|   L16|    L64,
           L1|L2|L4|   L16|    L64,
    L0|    L1|L2|L4|   L16|    L64,
                    L8|L16|    L64,
    L0|             L8|L16|    L64,
           L1|      L8|L16|    L64,
    L0|    L1|      L8|L16|    L64,
              L2|   L8|L16|    L64,
    L0|       L2|   L8|L16|    L64,
           L1|L2|   L8|L16|    L64,
    L0|    L1|L2|   L8|L16|    L64,
                 L4|L8|L16|    L64,
    L0|          L4|L8|L16|    L64,
           L1|   L4|L8|L16|    L64,
    L0|    L1|   L4|L8|L16|    L64,
              L2|L4|L8|L16|    L64,
    L0|       L2|L4|L8|L16|    L64,
           L1|L2|L4|L8|L16|    L64,
    L0|    L1|L2|L4|L8|L16|    L64,
                           L32|L64,         
    L0|                    L32|L64,         
           L1|             L32|L64,   
    L0|    L1|             L32|L64,
              L2|          L32|L64,
    L0|       L2|          L32|L64,
           L1|L2|          L32|L64,
    L0|    L1|L2|          L32|L64,
                 L4|       L32|L64,
    L0|          L4|       L32|L64,
           L1|   L4|       L32|L64,
    L0|    L1|   L4|       L32|L64,
              L2|L4|       L32|L64,
    L0|       L2|L4|       L32|L64,
           L1|L2|L4|       L32|L64,
    L0|    L1|L2|L4|       L32|L64, 
                    L8|    L32|L64, 
    L0|             L8|    L32|L64,
           L1|      L8|    L32|L64,
    L0|    L1|      L8|    L32|L64,
              L2|   L8|    L32|L64,
    L0|       L2|   L8|    L32|L64,
           L1|L2|   L8|    L32|L64,
    L0|    L1|L2|   L8|    L32|L64,
                 L4|L8|    L32|L64,
    L0|          L4|L8|    L32|L64,
           L1|   L4|L8|    L32|L64,
    L0|    L1|   L4|L8|    L32|L64,
              L2|L4|L8|    L32|L64,
    L0|       L2|L4|L8|    L32|L64,
           L1|L2|L4|L8|    L32|L64,
    L0|    L1|L2|L4|L8|    L32|L64,
                       L16|L32|L64,
    L0|                L16|L32|L64,
           L1|         L16|L32|L64,
    L0|    L1|         L16|L32|L64,
              L2|      L16|L32|L64,
    L0|       L2|      L16|L32|L64,
           L1|L2|      L16|L32|L64,
    L0|    L1|L2|      L16|L32|L64,
                 L4|   L16|L32|L64,
    L0|          L4|   L16|L32|L64,
           L1|   L4|   L16|L32|L64,
    L0|    L1|   L4|   L16|L32|L64,
              L2|L4|   L16|L32|L64,
    L0|       L2|L4|   L16|L32|L64,
           L1|L2|L4|   L16|L32|L64,
    L0|    L1|L2|L4|   L16|L32|L64,
                    L8|L16|L32|L64,
    L0|             L8|L16|L32|L64,
           L1|      L8|L16|L32|L64,
    L0|    L1|      L8|L16|L32|L64,
              L2|   L8|L16|L32|L64,
    L0|       L2|   L8|L16|L32|L64,
           L1|L2|   L8|L16|L32|L64,
    L0|    L1|L2|   L8|L16|L32|L64,
                 L4|L8|L16|L32|L64,
    L0|          L4|L8|L16|L32|L64,
           L1|   L4|L8|L16|L32|L64,
    L0|    L1|   L4|L8|L16|L32|L64,
              L2|L4|L8|L16|L32|L64,
    L0|       L2|L4|L8|L16|L32|L64,
           L1|L2|L4|L8|L16|L32|L64,
    L0|    L1|L2|L4|L8|L16|L32|L64,
    L0|L01| L1|L2|L4|L8|L16|L32|L64, // this setting is not in MFJ firmware up/down
};