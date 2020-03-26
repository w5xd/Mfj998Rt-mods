using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Mfj998Desktop
{
    public partial class DesktopForm : Form
    {
        public DesktopForm()
        {
            InitializeComponent();
        }
        public class Measurement
        {
            public double fq { get; set; }
            public double r { get; set; }
            public double x { get; set; }
        }
        public class Asd
        {
            public List<Measurement> Measurements { get; set; }
            public int DotsNumber { get; set; }
        }

        Asd measurements;
        Asd limitedMeasurements;
        Asd smoothed;
        private class networkValue { public double cPf; public double luH; public bool load; }
        Dictionary<double, networkValue> networkDict;
        Dictionary<int, tunerPoint> tunerPoints;

        void applyMinMax()
        {
            if (null == measurements)
                limitedMeasurements = null;
            else
            {
                limitedMeasurements = new Asd();
                limitedMeasurements.Measurements = new List<Measurement>();
                double fMax = (double)numericUpDownFmax.Value;
                fMax *= 0.001;
                double fMin = (double)numericUpDownFmin.Value;
                fMin *= 0.001;
                for (int i = 0; i < measurements.Measurements.Count; i++)
                {
                    var m = measurements.Measurements[i];
                    if ((m.fq >= fMin) && (m.fq <= fMax))
                        limitedMeasurements.Measurements.Add(m);
                }

            }
        }

        void minMaxToChart()
        {
            // put four labels edge to edge and at multiples of 100KHz
            double range = 0.001 * (double)(numericUpDownFmax.Value - numericUpDownFmin.Value);
            double span = range / 5;
            double fLabelApproX = (.001 * (double)numericUpDownFmin.Value) + span / 2;
            chart.ChartAreas[0].AxisX.CustomLabels.Clear();
            chart.ChartAreas[1].AxisX.CustomLabels.Clear();
            chart.ChartAreas[2].AxisX.CustomLabels.Clear();
            int prevKhz = 0;
            for (int i = 0; i < 4; i++)
            {
                int nearest100Khz = (int)((fLabelApproX * 10) + 0.5);
                if (nearest100Khz != prevKhz)
                {
                    prevKhz = nearest100Khz;
                    double pos = nearest100Khz;
                    pos /= 10.0;
                    var cl = new System.Windows.Forms.DataVisualization.Charting.CustomLabel(pos - span, pos + span,
                        String.Format("{0}Khz", nearest100Khz * 100),
                        0, System.Windows.Forms.DataVisualization.Charting.LabelMarkStyle.Box);
                    chart.ChartAreas[0].AxisX.CustomLabels.Add(cl);
                    chart.ChartAreas[1].AxisX.CustomLabels.Add(cl);
                    chart.ChartAreas[2].AxisX.CustomLabels.Add(cl);
                }
                fLabelApproX += span;
            }
        }

        private void buttonSelectData_Click(object sender, EventArgs e)
        {
            OpenFileDialog ofd = new OpenFileDialog();
            ofd.Title = "Open Analyzer File";
            ofd.Filter = "analyzer files (*.asd)|*.asd|CSV files (*.csv)|*.csv";
            ofd.CheckFileExists = true;
            if (ofd.ShowDialog() == DialogResult.OK)
            {
                if (ofd.FilterIndex == 1)
                {
                    var str = System.IO.File.ReadAllText(ofd.FileName);
                    measurements = System.Text.Json.JsonSerializer.Deserialize<Asd>(str);
                }
                else if (ofd.FilterIndex == 2)
                {
                    using (var fs = new System.IO.StreamReader(ofd.FileName))
                    {
                        string line;
                        measurements = new Asd();
                        measurements.Measurements = new List<Measurement>();
                        while ((line = fs.ReadLine()) != null)
                        {
                            int commapos1 = line.IndexOf(',');
                            if (commapos1 > 0)
                            {
                                int commapos2 = line.IndexOf(',', commapos1 + 1);
                                if (commapos2 > 0)
                                {
                                    var m = new Measurement();
                                    string fq = line.Substring(0, commapos1);
                                    m.fq = Double.Parse(fq);
                                    string r = line.Substring(commapos1 + 1, commapos2 - commapos1 - 1);
                                    m.r = Double.Parse(r);
                                    string x = line.Substring(commapos2 + 1);
                                    m.x = Double.Parse(x);
                                    measurements.Measurements.Add(m);
                                }
                            }
                        }
                    }
                }
                if ((null != measurements) && (null != measurements.Measurements))
                {
                    double minFreq = Double.MaxValue;
                    double maxFreq = -minFreq;
                    foreach (var m in measurements.Measurements)
                    {
                        if (m.fq < minFreq)
                            minFreq = m.fq;
                        if (m.fq > maxFreq)
                            maxFreq = m.fq;
                    }
                    decimal decFmax = (decimal)(maxFreq * 1000);
                    decimal decFmin = (decimal)(minFreq * 1000);
                    var temp = pauseUpdate;
                    pauseUpdate = true;
                    numericUpDownFmin.Maximum = decFmax;
                    numericUpDownFmax.Minimum = decFmin;
                    numericUpDownFmin.Minimum = decFmin;
                    numericUpDownFmax.Maximum = decFmax;
                    if (numericUpDownFmax.Value == numericUpDownFmin.Value)
                    {
                        numericUpDownFmax.Value  = decFmax;
                        numericUpDownFmin.Value = decFmin;
                    }

                    pauseUpdate = temp;
                    limitedMeasurements = smoothed = measurements;
                    minMaxToChart();
                    recalculate();
                    numericUpDownSmoother.Enabled = true;
                    buttonDecimate.Enabled = true;
                }
                else
                    smoothed = null;
            }
        }

        private const double Z0 = 50;
        private double TWO_PI = 8 * Math.Atan(1);

        private networkValue fromRXf(double r, double x, double fMhz)
        {
            double R = r / Z0; double X = x / Z0; // normalize to 50 ohms
            double f = fMhz;
            double w = f * TWO_PI; // radians
            var ret = new networkValue();
            // Y defined to be G + jB
            // Z defined to be R + jX
            // Bc is capacitor susceptance in normalized mhos
            // Xl is inductor reactance in normalized ohms
            if (R >= 1) // C on load
            {
                double denom = R * R + X * X;
                double G = R / denom;
                double B = -X / denom;
                double alpha = Math.Sqrt(G - G * G);
                double Bc = alpha - B;
                double Xl = alpha / (G * G + alpha * alpha);
                double C = Bc / (w * Z0); // uF
                C *= 1000000; // pF
                double L = Xl * Z0 / w; // uH
                ret.cPf = C; ret.luH = L; ret.load = true;
            }
            else
            {
                double beta = Math.Sqrt(R - R * R);
                double Xl = beta - X;
                double Bc = beta / (beta * beta + R * R);
                double C = Bc / (w * Z0); // uF
                C *= 1000000; // pF
                double L = Xl * Z0 / w; // uH
                ret.cPf = C; ret.luH = L; ret.load = false;
            }
            return ret;
        }

        private void recalculate()
        {
            applyMinMax();
            if (null == limitedMeasurements)
                return;
            smoothed = new Asd();
            smoothed.Measurements = new List<Measurement>();
            networkDict = new Dictionary<double, networkValue>();
            int range = (int)numericUpDownSmoother.Value / 2;
            for (int i = 0; i < limitedMeasurements.Measurements.Count; i++)
            {
                int k = 0;
                double r = 0;
                double x = 0;
                for (int j = -range; j <= range; j++)
                {
                    int m = i - j;
                    if ((m >= 0) && (m < limitedMeasurements.Measurements.Count))
                    {
                        k += 1;
                        r += limitedMeasurements.Measurements[m].r;
                        x += limitedMeasurements.Measurements[m].x;
                    }
                }
                var rx = new Measurement();
                r /= k;
                x /= k;
                rx.x = x;
                rx.r = r;
                rx.fq = limitedMeasurements.Measurements[i].fq;
                smoothed.Measurements.Add(rx);
            }
            foreach (var s in chart.Series)
                s.Points.Clear();
            foreach (Measurement a in smoothed.Measurements)
            {
                chart.Series[0].Points.Add(new System.Windows.Forms.DataVisualization.Charting.DataPoint(a.fq, a.r));
                chart.Series[1].Points.Add(new System.Windows.Forms.DataVisualization.Charting.DataPoint(a.fq, a.x));
                var p = fromRXf(a.r, a.x, a.fq);
                networkDict.Add(a.fq, p);
                chart.Series["L"].Points.Add(new System.Windows.Forms.DataVisualization.Charting.DataPoint(a.fq, p.luH));
                var cPoint = new System.Windows.Forms.DataVisualization.Charting.DataPoint(a.fq, p.cPf);
                if (p.load)
                    chart.Series["C load"].Points.Add(cPoint);
                else
                    chart.Series["C gen"].Points.Add(cPoint);
            }
            if (null != m_mfj998Port && m_mfj998Port.IsOpen)
                    buttonDecimate.Enabled = true;

        }

        private void numericUpDownSmoother_ValueChanged(object sender, EventArgs e)
        {
            int curVal = (int)numericUpDownSmoother.Value;
            if (0 != (curVal & 1))
                numericUpDownSmoother.Value = curVal & ~1;
            recalculate();
        }

        static int toTunerUnits(double fMhz)
        {
            return (int)((fMhz * 2048) + 1.0);
        }
        static double fromTunerUnits(int fRaw)
        {
            return fRaw / 2048.0;
        }

        private void buttonDecimate_Click(object sender, EventArgs e)
        {
            // generate number of points in numericUpDownDecimate from data in smoothed.
            // simple linear interpolation
            tunerPoints = new Dictionary<int, tunerPoint>();
            int pointCount = (int)numericUpDownDecimate.Value;
            double fMin = networkDict.First().Key;
            double fMax = networkDict.Last().Key;
            double fSpan = fMax - fMin;
            double fIncrementMhz = fSpan / pointCount;

            int rawTunerIncrement = toTunerUnits(fIncrementMhz);
            int incrementHalved = rawTunerIncrement / 2;
            // force fIncrement into quantize like tuner
            fIncrementMhz = fromTunerUnits(rawTunerIncrement);
            double interpolatedFreq;
            for (int i = 0; i < pointCount; i++)
            {
                interpolatedFreq = fMin + i * fIncrementMhz;
                int tunerFreq = toTunerUnits(interpolatedFreq); // units of halfKhz
                var Above = networkDict.Keys.Where(p => p >= interpolatedFreq);
                var Below = networkDict.Keys.Where(p => p <= interpolatedFreq);
                if (!Above.Any())
                    break;
                if (!Below.Any())
                    break;
                var keyAbove = Above.First();
                var keyBelow = Below.Last();
                double L1 = networkDict[keyAbove].luH;
                double C1 = networkDict[keyAbove].cPf;
                if ((keyBelow != keyAbove) && (networkDict[keyAbove].load == networkDict[keyBelow].load))
                {
                    double L2 = networkDict[keyBelow].luH;
                    double C2 = networkDict[keyBelow].cPf;
                    double fracOfHigher = (interpolatedFreq - keyBelow) / (keyAbove - keyBelow);
                    double fracOfLower = 1.0 - fracOfHigher;
                    tunerPoints.Add(tunerFreq - incrementHalved, new tunerPoint(L1 * fracOfHigher + L2 * fracOfLower,
                                            C1 * fracOfHigher + C2 * fracOfLower, networkDict[keyAbove].load));
                }
                else
                    tunerPoints.Add(tunerFreq - incrementHalved, new tunerPoint(L1, C1, networkDict[keyAbove].load));
            }
            buttonDecimate.Enabled = false;
            buttonDump.Enabled = false;
            var toMfj = new ToTunerEeprom(tunerPoints, rawTunerIncrement, m_mfj998Port);
            toMfj.toTuner((bool ok) =>
            {
                this.BeginInvoke(new Action(EnableDecimateButton));
            });
        }
        private void EnableDecimateButton()
        {
            buttonDecimate.Enabled = true;
            buttonDump.Enabled = true;
        }

        private void DesktopForm_Load(object sender, EventArgs e)
        {
            labelFwd.Text = "";
            labelRef.Text = "";
            labelSwr.Text = "";
            labelFreq.Text = "";
        }
        private System.IO.Ports.SerialPort m_mfj998Port;
        private void comboBoxTunerPorts_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (m_mfj998Port != null)
                m_mfj998Port.Dispose();
            m_mfj998Port = null;
            String comName = comboBoxTunerPorts.SelectedItem.ToString();
            if (comName == "None")
                return;
            m_mfj998Port = new System.IO.Ports.SerialPort(comName,
               38400, System.IO.Ports.Parity.None, 8, System.IO.Ports.StopBits.One);
            m_mfj998Port.Handshake = System.IO.Ports.Handshake.None;
            m_mfj998Port.Encoding = new System.Text.ASCIIEncoding();
            m_mfj998Port.RtsEnable = true;
            m_mfj998Port.DtrEnable = false;
            m_mfj998Port.DataReceived += new System.IO.Ports.SerialDataReceivedEventHandler(port_DataReceived);
            m_mfj998Port.Open();
            if (null != networkDict)
                buttonDecimate.Enabled = true;
            buttonClearEEPROM.Enabled = true;
            buttonDump.Enabled = true;
            if (m_mfj998Port.IsOpen)
                m_mfj998Port.WriteLine("SET S=0");
        }
        private void port_DataReceived(object sender,
                      System.IO.Ports.SerialDataReceivedEventArgs e)
        {
            BeginInvoke(new Action(ReadNow));
        }

        private string mfj998Incoming;

        private void ReadNow()
        {
            String s = m_mfj998Port.ReadExisting();
            mfj998Incoming += s;
            int idx;
            char[] nl = { '\r', '\n' };
            while (0 <= (idx = mfj998Incoming.IndexOfAny(nl)))
            {
                string toPrint = null;
                if (idx > 0)
                    toPrint = mfj998Incoming.Substring(0, idx);
                mfj998Incoming = mfj998Incoming.Substring(idx+1);
                if (!String.IsNullOrEmpty(toPrint))
                    textBoxLocalPort.AppendText(toPrint+"\r\n");
            }
        }

        private void buttonClearEEPROM_Click(object sender, EventArgs e)
        {
            if (null != m_mfj998Port && m_mfj998Port.IsOpen)
            {
                m_mfj998Port.WriteLine("EEPROM INDEX ERASE");
            }
        }

        private void buttonDump_Click(object sender, EventArgs e)
        {
            if (null != m_mfj998Port && m_mfj998Port.IsOpen)
            {

                string cmd = "EEPROM DUMP INDEX";
                int tunerMin = toTunerUnits(0.001 * (double)numericUpDownFmin.Value);
                if (tunerMin > 100)
                {
                    cmd += " f=";
                    cmd += tunerMin.ToString();
                }
                m_mfj998Port.WriteLine(cmd);
            }
        }

        private System.IO.Ports.SerialPort m_gatewayPort;
        private void comboBoxGatewayPorts_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (m_gatewayPort != null)
                m_gatewayPort.Dispose();
            m_gatewayPort = null;
            String comName = comboBoxGatewayPorts.SelectedItem.ToString();
            if (comName == "None")
                return;
            m_gatewayPort = new System.IO.Ports.SerialPort(comName,
               9600, System.IO.Ports.Parity.None, 8, System.IO.Ports.StopBits.One);
            m_gatewayPort.Handshake = System.IO.Ports.Handshake.None;
            m_gatewayPort.Encoding = new System.Text.ASCIIEncoding();
            m_gatewayPort.RtsEnable = true;
            m_gatewayPort.DtrEnable = false;
            m_gatewayPort.DataReceived += new System.IO.Ports.SerialDataReceivedEventHandler(gateway_DataReceived);
            m_gatewayPort.Open();

        }
        private void gateway_DataReceived(object sender,
                            System.IO.Ports.SerialDataReceivedEventArgs e)
        {
            BeginInvoke(new Action(ReadGatewayNow));
        }

        private int lastTunerFreq;
        private int lastTunerC;
        private int lastTunerL;
        private int lastTunerP;
        string gatewayIncoming;
        private void ReadGatewayNow()
        {
            String s = m_gatewayPort.ReadExisting();
            gatewayIncoming += s;
            int idx;
            char[] nl = { '\r', '\n' };
            while (0 <= (idx = gatewayIncoming.IndexOfAny(nl)))
            {
                string toPrint = null;
                if (idx > 0)
                    toPrint = gatewayIncoming.Substring(0, idx);
                gatewayIncoming = gatewayIncoming.Substring(idx + 1);
                if (!String.IsNullOrEmpty(toPrint))
                {
                    labelTelemetry.Text = toPrint;
                    if (!String.IsNullOrEmpty(toPrint))
                       textBoxGatewayPort.AppendText(toPrint+"\r\n");
                    int xidx = toPrint.IndexOf("X:");
                    if (xidx > 0)
                    {
                        int xend = toPrint.IndexOf(" ", xidx);
                        if (xend > 0)
                        {
                            var f = Int32.Parse(toPrint.Substring(xidx + 2, xend - xidx - 2));
                            if (f != 0)
                            {
                                lastTunerFreq = f;
                                labelFreq.Text = String.Format("{0:0.} Khz", 1000.0 * fromTunerUnits(f));
                            }
                            buttonSetEEPROM.Enabled = lastTunerFreq != 0;
                        }
                    }
                    int fwdidx = toPrint.IndexOf("F:");
                    int refidx = toPrint.IndexOf("R:");
                    if ((fwdidx > 0) && (refidx > 0))
                    {
                        int fwdend = toPrint.IndexOf(" ", fwdidx);
                        int refend = toPrint.IndexOf(" ", refidx);
                        if ((fwdend > 0) && (refend > 0))
                        {
                            int fwd = Int32.Parse(toPrint.Substring(fwdidx+2, fwdend - fwdidx - 2));
                            int refl = Int32.Parse(toPrint.Substring(refidx + 2, refend - refidx - 2));
                            const double CALI = 2443.0;
                            double fPower = 100.0 * (fwd * fwd) / (CALI * CALI);
                            double rPower = 100.0 * (refl * refl) / (CALI * CALI);
                            labelFwd.Text = String.Format("{0:0.0}", fPower);
                            labelRef.Text = String.Format("{0:0.0}", rPower);
                            if (fwd != 0)
                            {
                                if (refl == 0)
                                {
                                    labelSwr.Text = "1.0";
                                }
                                else if (refl >= fwd)
                                    labelSwr.Text = "~";
                                else
                                {
                                    double swr = (fwd + refl);
                                    swr /= (fwd - refl);
                                    labelSwr.Text = String.Format("{0:0.00}", swr);
                                }
                            }
                        }
                    }
                    ToLabel(toPrint, "C:", numericUpDownC, (int i) => { labelCpf.Text = String.Format("{0:0.} pF", 15.5*i); lastTunerC = i; return i; });
                    ToLabel(toPrint, "L:", numericUpDownL, (int i) => { labelLnH.Text = String.Format("{0:0.0} nH", .095 * i); lastTunerL = i; return i; });
                    ToLabel(toPrint, "P:", numericUpDownP, (int i) => { lastTunerP = i; return i; });
                    ToSwrSetting(toPrint, "T:", numericUpDownTrigger);
                    ToSwrSetting(toPrint, "S:", numericUpDownStop);
                }
            }
        }
        private delegate int ConvertToUI(int tuner);

        private void ToLabel(string incoming, string tomatch, System.Windows.Forms.NumericUpDown lbl, ConvertToUI cui )
        {
            int fwdidx = incoming.IndexOf(tomatch);
            char[] nl = { ']', ' ' };
            if (fwdidx > 0)
            {
                int fwdend = incoming.IndexOfAny(nl, fwdidx);
                if (fwdend > 0)
                {
                    bool temp = m_ignoreChange;
                    m_ignoreChange = true;
                    lbl.Value = cui(Int32.Parse(incoming.Substring(fwdidx + 2, fwdend - fwdidx - 2)));
                    m_ignoreChange = temp;
                }
            }
        }

        private bool m_ignoreChange = false;

        private void ToSwrSetting(string incoming, string tomatch, System.Windows.Forms.NumericUpDown lbl)
        {
            int fwdidx = incoming.IndexOf(tomatch);
            char[] nl = { ']', ' ' };
            if (fwdidx > 0)
            {
                int fwdend = incoming.IndexOfAny(nl, fwdidx);
                if (fwdend > 0)
                {
                    int rawSwr = Int32.Parse(incoming.Substring(fwdidx + 2, fwdend - fwdidx - 2));
                    decimal calculated = rawSwr;
                    calculated = Decimal.Divide(calculated, 64);
                    calculated = Decimal.Add(calculated, 1);
                    if (calculated > lbl.Maximum)
                        calculated = lbl.Maximum;
                    bool temp = m_ignoreChange;
                    m_ignoreChange = true;
                    lbl.Value = calculated;
                    m_ignoreChange = temp;
                }
            }
        }

        private void EnableNodeIdChange()
        {
            numericUpDownNodeId.Enabled = true;
            buttonRestart.Enabled = true;
        }

        private void restartTelemetry()
        {
            if (!m_ignoreChange && null != m_gatewayPort && m_gatewayPort.IsOpen)
            {
                numericUpDownNodeId.Enabled = false;
                buttonRestart.Enabled = false;
                string cmd = String.Format("SendMessageToNode {0} GET", (int)numericUpDownNodeId.Value);
                m_gatewayPort.WriteLine(cmd);
                var nodeId = (int)numericUpDownNodeId.Value;
                var timerChangeNodeId = new System.Timers.Timer(1000);
                timerChangeNodeId.Elapsed +=
                (o, ev) => {
                    string cmd2 = string.Format("SendMessageToNode {0} SET T=1000", nodeId);
                    m_gatewayPort.WriteLine(cmd2);
                    timerChangeNodeId.Dispose();
                    this.BeginInvoke(new Action(EnableNodeIdChange));
                };
                timerChangeNodeId.Enabled = true;
            }
        }

        private void numericUpDownNodeId_ValueChanged(object sender, EventArgs e)
        {
            restartTelemetry();
        }

        private void numericUpDownC_ValueChanged(object sender, EventArgs e)
        {
            if (!m_ignoreChange && null != m_gatewayPort && m_gatewayPort.IsOpen)
            {
                string cmd = String.Format("SendMessageToNode {0} SET C={1}", (int)numericUpDownNodeId.Value, (int)numericUpDownC.Value);
                m_gatewayPort.WriteLine(cmd);
            }
        }

        private void numericUpDownL_ValueChanged(object sender, EventArgs e)
        {
            if (!m_ignoreChange && null != m_gatewayPort && m_gatewayPort.IsOpen)
            {
                string cmd = String.Format("SendMessageToNode {0} SET L={1}", (int)numericUpDownNodeId.Value, (int)numericUpDownL.Value);
                m_gatewayPort.WriteLine(cmd);
            }
        }

        private void numericUpDownP_ValueChanged(object sender, EventArgs e)
        {
            if (!m_ignoreChange && null != m_gatewayPort && m_gatewayPort.IsOpen)
            {
                string cmd = String.Format("SendMessageToNode {0} SET P={1}", (int)numericUpDownNodeId.Value, (int)numericUpDownP.Value);
                m_gatewayPort.WriteLine(cmd);
            }
        }

        private void numericUpDownTrigger_ValueChanged(object sender, EventArgs e)
        {
            if (!m_ignoreChange && null != m_gatewayPort && m_gatewayPort.IsOpen)
            {
                int toTuner = 0;
                decimal presented = numericUpDownTrigger.Value;
                if (presented < numericUpDownTrigger.Maximum)
                {
                    presented -= 1;
                    presented *= 64;
                    toTuner = (int)presented;
                }
                else
                    toTuner = 0xffff;
                string cmd = String.Format("SendMessageToNode {0} SET TSWR={1}", (int)numericUpDownNodeId.Value, toTuner);
                m_gatewayPort.WriteLine(cmd);
            }
            if (numericUpDownStop.Value > numericUpDownTrigger.Value)
                numericUpDownStop.Value = numericUpDownTrigger.Value;
            numericUpDownStop.Maximum = numericUpDownTrigger.Value;
        }

        private void numericUpDownStop_ValueChanged(object sender, EventArgs e)
        {
            if (!m_ignoreChange && null != m_gatewayPort && m_gatewayPort.IsOpen)
            {
                decimal presented = numericUpDownStop.Value;
                presented -= 1;
                presented *= 64;
                int toTuner = (int)presented;
                string cmd = String.Format("SendMessageToNode {0} SET SSWR={1}", (int)numericUpDownNodeId.Value, toTuner);
                m_gatewayPort.WriteLine(cmd);
            }
        }

        private bool pauseUpdate = false;
        private void numericUpDownFmin_ValueChanged(object sender, EventArgs e)
        {
            if (pauseUpdate)
                return;
            minMaxToChart();
            recalculate();
        }

        private void numericUpDownFmax_ValueChanged(object sender, EventArgs e)
        {
            if (pauseUpdate)
                return;
            minMaxToChart();
            recalculate();
        }

        private void comboBoxGatewayPorts_DropDown(object sender, EventArgs e)
        {
            comboBoxGatewayPorts.Items.Clear();
            comboBoxGatewayPorts.Items.Add("None");
            foreach (string s in System.IO.Ports.SerialPort.GetPortNames())
                comboBoxGatewayPorts.Items.Add(s);
        }

        private void comboBoxTunerPorts_DropDown(object sender, EventArgs e)
        {
            comboBoxTunerPorts.Items.Clear();
            comboBoxTunerPorts.Items.Add("None");
            foreach (string s in System.IO.Ports.SerialPort.GetPortNames())
                comboBoxTunerPorts.Items.Add(s);
        }

        private void buttonSetEEPROM_Click(object sender, EventArgs e)
        {
            if (lastTunerFreq == 0)
                return;
            string cmd = String.Format("SendMessageToNode {0} EEPROM TABLE f={1} C={2} L={3} P={4}", (int)numericUpDownNodeId.Value,
                lastTunerFreq, lastTunerC, lastTunerL, lastTunerP);
            m_gatewayPort.WriteLine(cmd);
        }

        private void buttonRestart_Click(object sender, EventArgs e)
        {
            restartTelemetry();
        }
    }
}
