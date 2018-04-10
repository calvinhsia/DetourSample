
using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;
using System.Data;
using System.Threading;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

// from https://blogs.msdn.microsoft.com/calvin_hsia/2008/07/14/cellular-automata-the-game-of-life/
namespace CSLife
{
    /// <summary>
    /// Summary description for Form1.
    /// </summary>
    public partial class Form1 : System.Windows.Forms.Form
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        [DllImport("kernel32.dll")]
        public static extern bool Beep(int freq, int dura);
        private System.ComponentModel.Container components = null;
        private TextBox m_box;
        private Button m_button;
        private Thread m_Thread = null;
        bool m_nReset = false;
        static int m_numx = 200;
        static int m_numy = 200;
        static int m_cellx = 5;
        static int m_celly = 5;
        static int m_Offx = 10;
        static int m_Offy = 10;
        static int MAXCOLOR = 8;
        static int COLORDIV = 16;   // # of gens before color changes
        bool m_fDrawAll = false;
        SolidBrush m_brushBlack = new SolidBrush(Color.Black);
        SolidBrush m_brushWhite = new SolidBrush(Color.White);
        SolidBrush[] m_brushArray = new SolidBrush[MAXCOLOR];
        Queue m_queue = new Queue();
        int[,] m_cells = new int[m_numx, m_numy];
        private System.Windows.Forms.Button button1;
        int[,] m_diff = new int[m_numx, m_numy];

        private int DrawCells(bool fDrawAll)
        {
            int nTotal = 0;
            Graphics g = Graphics.FromHwnd(this.Handle);

            int x, y;
            if (fDrawAll)
            {
                System.Diagnostics.Debug.Write("\nDraw");
            }
            var p = Marshal.AllocCoTaskMem(1024 * 1024 + 10);
            Marshal.FreeCoTaskMem(p);
            for (x = 0; x < m_numx; x++)
            {
                for (y = 0; y < m_numy; y++)
                {
                    var ptr = Marshal.AllocHGlobal(1000); // heapalloc
                    Marshal.FreeHGlobal(ptr);
                    if (fDrawAll || m_diff[x, y] == 1)
                    {
                        if (m_cells[x, y] == 0)
                        {
                            g.FillRectangle(m_brushWhite, m_Offx + x * m_cellx, m_Offy + y * m_celly, m_cellx - 1, m_celly - 1);

                        }
                        else
                        {
                            nTotal++;
                            g.FillRectangle(m_brushArray[m_cells[x, y] / COLORDIV], m_Offx + x * m_cellx, m_Offy + y * m_celly, m_cellx - 1, m_celly - 1);
                        }
                    }
                }

            }
            return nTotal;
        }
        private void Shuffle()
        {
            int x, y;
            Random myrandom = new Random();

            for (x = 0; x < m_numx; x++)
            {
                for (y = 0; y < m_numy; y++)
                {
                    m_diff[x, y] = m_cells[x, y] = myrandom.Next(100) < 50 ? 1 : 0;
                }

            }
            DrawCells(false);
        }
        private int CountNeighbors(int x, int y)
        {
            if (x >= 0 && x < m_numx && y >= 0 && y < m_numy)
            {
                return m_cells[x, y] > 0 ? 1 : 0;
            }
            return 0;

        }
        private int Generation()
        {
            int[,] tmp = new int[m_numx, m_numy];
            int x, y;

            for (x = 0; x < m_numx; x++)
            {
                for (y = 0; y < m_numy; y++)
                {
                    tmp[x, y] = CountNeighbors(x - 1, y - 1) +
                        CountNeighbors(x - 1, y) +
                        CountNeighbors(x - 1, y + 1) +
                        CountNeighbors(x, y - 1) +
                        CountNeighbors(x, y + 1) +
                        CountNeighbors(x + 1, y - 1) +
                        CountNeighbors(x + 1, y) +
                        CountNeighbors(x + 1, y + 1);
                }
            }
            for (x = 0; x < m_numx; x++)
            {
                for (y = 0; y < m_numy; y++)
                {
                    if (tmp[x, y] == 3 && m_cells[x, y] == 0)           // an empty cell with exactly 3 neighbors: 1 is born
                    {
                        m_diff[x, y] = m_cells[x, y] = 1;
                    }
                    else if (tmp[x, y] >= 2 && tmp[x, y] <= 3 && m_cells[x, y] > 0)       // a live cell with 2 or more neighbors lives
                    {
                        if (m_cells[x, y] == COLORDIV * MAXCOLOR - 1)
                        {
                            m_diff[x, y] = 0;   // no change
                        }
                        else
                        {
                            m_cells[x, y]++;
                            m_diff[x, y] = 1;
                        }
                    }
                    else
                    {   // loneliness 
                        if (m_cells[x, y] == 0)       // already empty
                        {
                            m_diff[x, y] = 0;   // no change
                        }
                        else
                        {
                            m_cells[x, y] = 0;
                            m_diff[x, y] = 1;
                        }
                    }
                }
            }
            if (m_queue.Count > 0)
            {
                lock (m_queue.SyncRoot)
                {
                    foreach (Point pt in m_queue)
                    {
                        m_diff[pt.X, pt.Y] = 1;
                        if (m_cells[pt.X, pt.Y] == 0)
                        {
                            m_cells[pt.X, pt.Y] = 1;
                        }
                        else
                        {
                            m_cells[pt.X, pt.Y] = 0;
                        }

                    }
                    m_queue.Clear();

                }
            }
            int nResult = DrawCells(m_fDrawAll);
            m_fDrawAll = false;
            return nResult;
        }
        private void Generations()
        {
            int nTotal;
            while (true)
            {
                Shuffle();

                for (int i = 0; i < 100000; i++)
                {
                    nTotal = Generation();
                    this.Text = i.ToString() + " : " + nTotal.ToString();
                    if (m_nReset)
                    {
                        m_nReset = false;
                        break;
                    }
                }
            }
        }
        private void Handlethebutton(object sender, EventArgs e)
        {
            m_nReset = true;
        }
        private void OnClick(object sender, EventArgs e)
        {
        }
        private void Onmmove(object sender, MouseEventArgs e)
        {
            Point pt = this.PointToClient(Cursor.Position);
            Point ptcell = new Point
            {
                X = (pt.X - m_Offx) / m_cellx,
                Y = (pt.Y - m_Offy) / m_celly
            };
            if (ptcell.X >= 0 && ptcell.X < m_numx && ptcell.Y >= 0 && ptcell.Y < m_numy)
            {
                lock (m_queue.SyncRoot)
                {
                    m_queue.Enqueue(ptcell);
                }
            }
        }

        private void OnActivated(object sender, EventArgs e)
        {
            //    Generations();
            if (m_Thread == null)
            {
                m_Thread = new Thread(new ThreadStart(MyThread))
                {
                    IsBackground = true
                };
                m_Thread.Start();
                while (!m_Thread.IsAlive)
                {
                    m_Thread.Abort();
                }
            }
            else
            {
                m_fDrawAll = true;
            }

        }
        private void FormPaint(object sender, PaintEventArgs e)
        {
            m_fDrawAll = true;
        }
        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        protected override void Dispose(bool disposing)
        {
            if (m_Thread != null && m_Thread.IsAlive)
            {
                m_Thread.Abort();
            }
            if (disposing)
            {
                if (components != null)
                {
                    components.Dispose();
                }
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code
        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.button1 = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // button1
            // 
            this.button1.Location = new System.Drawing.Point(392, 0);
            this.button1.Name = "button1";
            this.button1.TabIndex = 0;
            this.button1.Text = "button1";
            this.button1.Click += new System.EventHandler(this.Button1_Click);
            // 
            // Form1
            // 
            this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
            this.ClientSize = new System.Drawing.Size(800, 558);
            this.Controls.AddRange(new System.Windows.Forms.Control[] {
                                                                                                              this.button1});
            this.Name = "Form1";
            this.Text = "Life";
            this.Click += new System.EventHandler(this.Form1_Click);
            this.Load += new System.EventHandler(this.Form1_Load);
            this.ResumeLayout(false);

        }
        #endregion

        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            Application.Run(new Form1());
        }
        private void MyThread()
        {
            Generations();
        }


        async Task<int> DoSomething()
        {
            await Task.Run(() =>
            {
                var ptr = Marshal.AllocHGlobal(1000); // heapalloc
                Marshal.FreeHGlobal(ptr);
            });
            return 0;
        }

        public Form1()
        {
            //
            // Required for Windows Form Designer support
            //
            InitializeComponent();
            this.Height = m_Offy + m_numy * m_celly;
            this.Width = m_Offx + m_numx * m_cellx;
            this.Left = 00;
            this.Top = 0;
            this.BackColor = Color.White;
            m_box = new TextBox
            {
                BackColor = Color.Cyan,
                Text = "Hi",
                Size = new Size(100, 100)
            };
            m_button = new Button
            {
                BackColor = Color.Red,
                Location = new Point(150, 00),
                Text = "&Reset"
            };
            Controls.Add(m_button);
            m_button.Click += new EventHandler(this.Handlethebutton);

            Controls.Add(m_box);
            var tmr = new System.Windows.Forms.Timer();
            var nStart = 0;
            tmr.Tick += async (o, e) =>
              {
                  var ptr = Marshal.AllocHGlobal(1000); // heapalloc
                  Marshal.FreeHGlobal(ptr);
                  if (nStart++ == 500)
                  {
                      this.Close();
                  }
                  await DoSomething();
              };
            tmr.Interval = (int)TimeSpan.FromMilliseconds(10).TotalMilliseconds;
            tmr.Enabled = true;



            //                this.Paint += new PaintEventHandler(this.FormPaint);
            this.Activated += new EventHandler(this.OnActivated);
            this.Click += new EventHandler(this.OnClick);
            this.MouseMove += new MouseEventHandler(this.Onmmove);
            this.Paint += new PaintEventHandler(this.FormPaint);
            this.FormClosing += (o, e) =>
            {
                m_Thread?.Abort();
            };
            for (int i = 0; i < MAXCOLOR; i++)
            {
                Color clr = Color.FromArgb(255, (MAXCOLOR - i - 1) * 32, 0, 0);
                m_brushArray[i] = new SolidBrush(clr);
            }

        }
        private void Form1_Load(object sender, System.EventArgs e)
        {
            Form.CheckForIllegalCrossThreadCalls = false;

            //                Beep(1000,1000);
        }

        private void Form1_Click(object sender, System.EventArgs e)
        {

        }

        private void Button1_Click(object sender, System.EventArgs e)
        {
            Generation();
        }
    }
}
