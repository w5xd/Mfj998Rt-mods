<h1>MFJ998Rt tuner Desktop control</h1>

Read <a href="../LICENSE">LICENSE</a>.

This is a Windows .NET application that interfaces with an MFJ998RT tuner modified per the
the details <a href="https://github.com/w5xd/Mfj998Rt-mods">here</a>. That modification replaces the controller microprocessor
in the tuner and the result is a tuner with a very different set of features than the 
original, and a very different way of operating.

Calibration

This MFJ998 Desktop application is designed to be set up (or "calibrated") starting with
antenna analyzer data acquired at the point where the tuner will be installed. From that data this application
uses formulas to calculate the necessary inductance and capacitance to transform the
antenna impedance to 50 ohms resistive. (What formula? See the function fromRXf in DesktopForm.cs. Where
did that formula come from? See introductory electrical engineering texts on linear circuit theory for
calculating complex impedances.) In addition to requiring the complex impedances, this application requires
you to make a choice, for each frequency band, of how many segments to divide that frequency band for storing
the calculated L and C values. An antenna with steep changes in impedance over frequency needs more segments
than one that varies slowly. This Desktop application and the daughterboard firmware enable loading the
Ls and Cs and number of segments into the tuner using the 9 pin COM port connector on the tuner and cabled
to the PC running this application. That is, the initial part of the tuner setup requires its weatherproof cover
removed and a serial cable running between the desktop and the tuner. The L and C values loaded at this time
can be updated later using the UHF transceiver, however the number of segments cannot be changed except
via the serial cable.

Caveat: I have found this technique of calculating the tuner's L and C settings from analyzer data to work
reliably only up to about 7MHz. For the 40m band and above, and for my own antenna, I have found it necessary
to use L and C settings in the tuner nowhere near those calculated using this technique. This desktop application
has the capability to do the necessary tweaking over the UHF link. (Why does the calculation fail? My guess is
that the tuner itself cannot be reliably modeled as a simple series inductor with capacitor to ground at 
higher frequencies.)

Step by Step

Start with an antenna analyzer at the point where the tuner will eventually be installed. Use it to measure
the complex input impedance over the frequency ranges that you plan to operate, and create a file. The frequency
range in the file can be any that is convenient. I recommend a separate file per amateur band. The
Desktop application expects to read the real part (in ohms) and the complex part (in ohms) in either a .CSV
file (comma separated values) or a .ASD file (XML analyzer file.) With those files available, start this
MFJ998 Desktop application. 

Use this application's "Select analyzer data" button to read in one of the files. If necessary, adjust the min 
and the max frequency selectors to cover the band you want to load into the tuner. The application presents
a (very arbitrary) default selection of 50 segments for the band. You'll likely need to experiment with this,
but my experience is that 50 is high number and fewer segments work OK, and leaves less tweaking to do over the
radio link later.

Adjust the number of smoothing points to deal with the fact that the analyzer data is somewhat noisy. The application
updates its graphs as you make the changes. The three graphs show, top to bottom: <ul>
  <li>The measured complex impedance as R and X (real and imaginary parts in ohms.)</li>
  <li> The calculated series inductance required, in units of uH</li>
      <li>The calculated parallel capacitance required in units of pF. And the symbol for the capacitance graph indicates 
        whether the capacitance is required on the load side of the tuner (or antenna side) or on the generator side 
        (or transmitter side.) </li>
</ul>

With a serial cable (3 conductors are requred: TXD, RXD and ground, the other 6 are unused) connected between the tuner
and this desktop, use the "Tuner port" selector to choose the port on the PC that is connected to the tuner. The "Clear tuner 
memory" button clears out <b><i>all</i></b> L and C settings in the tuner! Use the "To MFJ998" button to replace the
tuner's settings only for the frequency range selected by min and max. Settings already in the tuner for other frequencies
are unchanged. (The tuner firmware's behavior if you try to give it overlapping min/max settings is undefined. I haven't 
tested it, but it won't work predictably.)

Repeat the process of loading analyzer data, setting the number of segments and smoothing, and "To MFJ998" for each band
that you will want to support.

Disconnect the serial cable. Relocate the tuner to the antenna feedpoint and connect a coax with 12VDC on its center 
conductor using MFJ's instructions for how to feed power to the tuner. Use the "Gateway port" selector and choose
the COM port corresponding to the Arduino running the <a href='../GatewaySketch'>GatewaySketch</a>. This desktop application (currently) can only talk to one modified MFJ998RT at 
a time, and that tuner's daughterboard must have been setup for the same frequency and at the Node ID you select here.
Once those two are set correctly in this application and while the tuner has 12VDC applied, the application will show
the text of the messages the two are exchanging.

The Trigger and Stop settings initially show how the tuner's daughterboard's EEPROM is set. Click Restart to refresh
this display. Trigger is the SWR value that causes the tuner firmware to start a search, starting with its programmed L/C
settings for the frequency it sees from your transmitter. Stop is the value that, if it finds an L/C with an SWR
of this or better, it stops searching.

The desktop's C, L and P selectors use the UHF link to override the values the tuner daughterboard has chosen.
The "P" value is zero or one, which correspond, respectively, to the capacitance on the load (antenna) side, or the generator
(transmitter) side.

The Set EEPROM button commands the tuner, over its UHF link, to overwrite its EEPROM setting just for the segment corresponding
to the current frequency it has acquired (as displayed here) and with the L, C and P values displayed.



