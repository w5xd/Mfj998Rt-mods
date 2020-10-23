using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Mfj998Desktop
{

    class ToTunerEeprom
    {
        private Dictionary<int, tunerPoint> tunerPoints;
        private System.IO.Ports.SerialPort m_mfj998Port;
        private int rawTunerIncrement;
        public bool debug = false;

        public ToTunerEeprom(Dictionary<int, tunerPoint> points, int tunerIncrement, System.IO.Ports.SerialPort port)
        {
            tunerPoints = new Dictionary<int, tunerPoint>();
            foreach (var p in points)
                tunerPoints.Add(p.Key, new tunerPoint(p.Value));
            m_mfj998Port = port;
            rawTunerIncrement = tunerIncrement;
        }

        public delegate void OnFinished(bool ok);

        public void toTuner(OnFinished onfinished, bool initMemory = false)
        {
            var t = new System.Threading.Thread(() =>
            {
                bool ok = false;
                if (null != m_mfj998Port && m_mfj998Port.IsOpen)
                {
                    if (initMemory)
                    {
                        if (!debug) m_mfj998Port.WriteLine("EEPROM INDEX ERASE");
                        System.Threading.Thread.Sleep(1000);
                    }

                    int f = tunerPoints.Keys.First();
                    string command = String.Format("EEPROM INDEX f={0} i={1} n={2}", f, rawTunerIncrement, tunerPoints.Count);
                    if (!debug) m_mfj998Port.WriteLine(command);
                    System.Threading.Thread.Sleep(50);
                    foreach (var te in tunerPoints)
                    {
                        command = String.Format("EEPROM TABLE f={0} C={1} L={2} P={3}", f, te.Value.cIdx, te.Value.lIdx, te.Value.load ? 1 : 2);
                        System.Threading.Thread.Sleep(500);
                        if (!debug) m_mfj998Port.WriteLine(command);
                        f += rawTunerIncrement;
                    }
                    ok = true;
                }
                onfinished(ok);
            });
            t.Start();
        }
    }
}

public class tunerPoint
    {
        public static int toCidx(double C)
        { return (int)((C / 15.5) + 0.5); }

        public static double pFfromCidx(int cIdx)
        { return cIdx * 15.5;   }

        public static double nHfromLidx(int lIdx)
        {  return lIdx * 0.095; }

        public static int toLidx(double L)
        { return (int)((L / 0.095) + 0.5); }

        public int cIdx; public int lIdx; public bool load;
        public tunerPoint(double L, double C, bool load)
        {
            this.load = load;
            cIdx = toCidx(C);
            lIdx = toLidx(L);
        }
        public tunerPoint(tunerPoint other)
        {
            this.load = other.load;
            this.cIdx = other.cIdx;
            this.lIdx = other.lIdx;
        }
    }

