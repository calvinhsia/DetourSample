using System;
using System.CodeDom;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTestProject1
{

    [ComVisible(true)]
    [Guid("45BAEA8A-59DC-4417-9BD5-CC4ED2A10C53")] //{45BAEA8A-59DC-4417-9BD5-CC4ED2A10C53}
    [StructLayout(LayoutKind.Sequential)]
    public struct HeapCollectStats
    {
        public int NumUniqueStacks;
        public int MyStlAllocLimit;
        public int MyStlAllocCurrentTotalAlloc;
        public int MyStlAllocBytesEverAlloc;
        public int MyStlTotBytesEverFreed;
        public int nTotNumHeapAllocs; // heapallocs of specified size
        public int MyRtlAllocateHeapCount; // total # of heapallocs
        public Int64 TotNumBytesHeapAlloc;
        public int NumDetailRecords;
        public int nTotFramesCollected;
        public int NumStacksMissed;
        public bool fReachedMemLimit;
        public HeapCollectStatDetail[] details; //ignore: Type library exporter warning processing 'UnitTestProject1.HeapCollectStats.details, UnitTestProject1'. Warning: The public struct contains one or more non-public fields that will be exported.
        public override string ToString()
        {
            return $@"{nameof(NumUniqueStacks)}={NumUniqueStacks:n0} 
                    {nameof(MyStlAllocLimit)}={MyStlAllocLimit:n0} 
                    {nameof(nTotNumHeapAllocs)}={nTotNumHeapAllocs:n0} 
                    {nameof(MyStlAllocCurrentTotalAlloc)}={MyStlAllocCurrentTotalAlloc:n0} 
                    {nameof(MyStlAllocBytesEverAlloc)}={MyStlAllocBytesEverAlloc:n0} 
                    {nameof(MyStlTotBytesEverFreed)}={MyStlTotBytesEverFreed:n0} 
                    {nameof(MyRtlAllocateHeapCount)}={MyRtlAllocateHeapCount:n0} 
                    {nameof(nTotFramesCollected)}={nTotFramesCollected:n0} 
                    {nameof(NumStacksMissed)}={NumStacksMissed:n0} 
                    {nameof(TotNumBytesHeapAlloc)}={TotNumBytesHeapAlloc:n0} 
                    {nameof(fReachedMemLimit)}={fReachedMemLimit}";
        }
        //        [MarshalAs(UnmanagedType.ByValArray, ArraySubType = UnmanagedType.Struct)]

        public HeapCollectStats(string strStacksToCollect) : this()
        {
            var splt = strStacksToCollect.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
            int num = splt.Length;
            NumDetailRecords = num;
            details = new HeapCollectStatDetail[NumDetailRecords];
            for (int i = 0; i < NumDetailRecords; i++)
            {
                var detsplt = splt[i].Split(':'); // size/thresh
                details[i].AllocSize = int.Parse(detsplt[0]);
                details[i].AllocThresh = int.Parse(detsplt[1]);
            }
        }
        public IntPtr GetPointer() // must be freed
        {
            var addr = Marshal.AllocHGlobal(Marshal.SizeOf(this) + NumDetailRecords * Marshal.SizeOf<HeapCollectStatDetail>());
            Marshal.StructureToPtr<HeapCollectStats>(this, addr, fDeleteOld: false);
            for (int i = 0; i < NumDetailRecords; i++)
            {
                var a = addr + Marshal.SizeOf<HeapCollectStats>() + i * Marshal.SizeOf<HeapCollectStatDetail>();
                Marshal.StructureToPtr<HeapCollectStatDetail>(details[i], a, fDeleteOld: false);
            }
            return addr;
        }
        public static HeapCollectStats FromPointer(IntPtr addr)
        {
            var stats = Marshal.PtrToStructure<HeapCollectStats>(addr);
            stats.details = new HeapCollectStatDetail[stats.NumDetailRecords];
            for (int i = 0; i < stats.NumDetailRecords; i++)
            {
                var a = addr + Marshal.SizeOf<HeapCollectStats>() + i * Marshal.SizeOf<HeapCollectStatDetail>();
                stats.details[i] = Marshal.PtrToStructure<HeapCollectStatDetail>(a);
            }
            return stats;
        }
    }
    [ComVisible(true)]
    [Guid("1D4B9F95-CAB5-4CB0-8F85-F9599DD3689B")]//{1D4B9F95-CAB5-4CB0-8F85-F9599DD3689B}
    public struct HeapCollectStatDetail
    {
        public int AllocSize;
        public int AllocThresh;
        public int NumStacksCollected;
        public override string ToString()
        {
            return $"{AllocSize}:{AllocThresh} NumStacksCollected: {NumStacksCollected:n0}";
        }
    }


    [ComVisible(true)]
    [Guid("1491F27F-5EB8-4A70-8651-23F1AB98AEC6")] ///{1491F27F-5EB8-4A70-8651-23F1AB98AEC6}
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface ITestHeapStacks
    {
        void DoHeapStackTests(int parm1, out int parm2, string strIn, out string strOut);
        void StartDetours(out IntPtr parm2);
        void SetHeapCollectParams(string HeapSizesToCollect, int NumFramesToCapture, int HeapAllocSizeMinValue, int StlAllocLimit);

        void StopDetours(IntPtr pDetours, IntPtr HeapStats);
    }

    [TestClass]
    public class UnitTest1
    {
        readonly Random _random = new Random(1);
        Guid guidComClass = new Guid("A90F9940-53C9-45B9-B67B-EE2EDE51CC00");

        public TestContext TestContext { get; set; }

        public void LogMessage(string msg)
        {
            msg = DateTime.Now.ToString("hh:mm:ss:fff") + $" {Thread.CurrentThread.ManagedThreadId} " + msg;
            TestContext.WriteLine(msg);
        }
        ITestHeapStacks GetTestHeapStacks(Interop oInterop)
        {
            var hr = oInterop.CoCreateFromFile("DetourClient.dll", guidComClass, typeof(ITestHeapStacks).GUID, out var pObject);
            if (hr != 0)
            {
                throw new InvalidOperationException("Couldn't create COM dll");
            }
            var obj = (ITestHeapStacks)Marshal.GetTypedObjectForIUnknown(pObject, typeof(ITestHeapStacks));
            return obj;

        }
        [TestMethod]
        public void TestPlumbing()
        {
            using (var oInterop = new Interop())
            {
                var obj = GetTestHeapStacks(oInterop);
                var sb = new StringBuilder(500);
                GetModuleFileName(IntPtr.Zero, sb, sb.Capacity);
                Assert.AreEqual(System.Diagnostics.Process.GetCurrentProcess().MainModule.FileName, sb.ToString());
                obj.StartDetours(out var pDetours);
                GetModuleFileName(IntPtr.Zero, sb, sb.Capacity);
                Assert.AreEqual("InDetouredGetModuleFileName", sb.ToString());
                for (int i = 0; i < 1000; i++)
                {
                    obj.DoHeapStackTests(parm1: 123, out var x, "StringIn", out var str);
                    Assert.AreEqual(124, x);

                    Assert.AreEqual(str, "StringInGotStr");
                }
                obj.StopDetours(pDetours, HeapStats: IntPtr.Zero);
                Marshal.ReleaseComObject(obj);
            }
        }
        [TestMethod]
        public async Task TestCollectStacks()
        {
            int nSizeSpecial = 1027;
            var strStacksToCollect = $"16:16, 32:32,{nSizeSpecial}:0";
            await TestCollectStacksHelper(strStacksToCollect, nSizeSpecial, NumFramesToCapture: 20, HeapAllocSizeMinValue: 1048576, StlAllocLimit: 65536 * 20);
        }
        [TestMethod]
        public async Task TestCollectStacksWithLimit()
        {
            int nSizeSpecial = 1027;
            var strStacksToCollect = $"16:16, 32:32,{nSizeSpecial}:0";
            await TestCollectStacksHelper(strStacksToCollect, nSizeSpecial, NumFramesToCapture: 20, HeapAllocSizeMinValue: 1048576, StlAllocLimit: 4096 , WaitForDoCsLife: true);
        }

        async Task TestCollectStacksHelper(
            string strStacksToCollect, 
            int nSizeSpecial, 
            int NumFramesToCapture, 
            int HeapAllocSizeMinValue, 
            int StlAllocLimit, 
            bool WaitForDoCsLife = false)
        {
            using (var oInterop = new Interop())
            {
                var obj = GetTestHeapStacks(oInterop);
                var sb = new StringBuilder(500);
                obj.SetHeapCollectParams(strStacksToCollect, NumFramesToCapture, HeapAllocSizeMinValue, StlAllocLimit);
                obj.StartDetours(out var pDetours);
                int nIter = 10000;
                int nThreads = 60;
                var lstTasks = new List<Task>
                {
                    Task.Run(() => DoCSLife())
                };
                if (WaitForDoCsLife)
                {
                    await lstTasks[0];
                }
                var procHeap = Heap.GetProcessHeap();
                var lstIntentionalLeaks = new List<IntPtr>();
                for (int iThread = 0; iThread < nThreads; iThread++)
                {
                    var task = Task.Run(() =>
                    {
                        //                        LogMessage($"Task Alloc {iThread} {nIter}");
                        for (int i = 0; i < nIter; i++)
                        {
                            var x = Heap.HeapAlloc(procHeap, 0, nSizeSpecial);
                            if (_random.Next(100) < 50)
                            {
                                lstIntentionalLeaks.Add(x);
                            }
                            else
                            {
                                Heap.HeapFree(procHeap, 0, x);
                            }
                        }
                    });
                    lstTasks.Add(task);
                }
                await Task.WhenAll(lstTasks);
                LogMessage($"Intentional Leaks {lstIntentionalLeaks.Count:n0}  TotAllocs={nIter * nThreads:n0}");
                var heapStats = new HeapCollectStats(strStacksToCollect);
                var ptrHeapStats = heapStats.GetPointer();
                obj.StopDetours(pDetours, ptrHeapStats);

                heapStats = HeapCollectStats.FromPointer(ptrHeapStats);
                LogMessage($"Allocation stacks {heapStats}");
                Array.ForEach<HeapCollectStatDetail>(heapStats.details, (d) =>
                 {
                     LogMessage($"  {d}");
                 });
                Assert.IsTrue(heapStats.MyRtlAllocateHeapCount > nIter * nThreads, $"Expected > {nIter * nThreads}, got {heapStats.MyRtlAllocateHeapCount}");
                var det = heapStats.details.Where(s => s.AllocSize == nSizeSpecial).Single();
                if (!heapStats.fReachedMemLimit)
                {
                    Assert.IsTrue(det.NumStacksCollected >= nIter, $"Expected numstacks collected ({det.NumStacksCollected}) > {nIter}");
                }

                foreach (var addr in lstIntentionalLeaks)
                {
                    Heap.HeapFree(procHeap, 0, addr);
                }

                Marshal.FreeHGlobal(ptrHeapStats);
                Marshal.ReleaseComObject(obj);
                //                Assert.Fail($"#HeapAlloc={heapStats.MyRtlAllocateHeapCount}");
            }
        }

        //[TestMethod]
        //public async Task TestDoCSLife()
        //{
        //    doCSLife();
        //}

        [DllImport("kernel32.dll", SetLastError = true)]
        [PreserveSig]
        public static extern uint GetModuleFileName
        (
            [In] IntPtr hModule,
            [Out] StringBuilder lpFilename,
            [In][MarshalAs(UnmanagedType.U4)] int nSize
        );
        void DoCSLife()
        {
            LogMessage($"Doing cslife");
            var csLife = new FileInfo(Path.Combine(Environment.CurrentDirectory, @"..\..\..\cslife.exe")).FullName;
            var csLifeForm = Assembly.LoadFrom(csLife);
            var typCSLife = csLifeForm.GetTypes().Where(t => t.Name == "Form1").First();
            var showDialogMeth = typCSLife.GetMethod("ShowDialog", new Type[] { });
            var form = Activator.CreateInstance(typCSLife);
            showDialogMeth.Invoke(form, null);
            LogMessage($"cslife done");
        }
    }
    public class Heap
    {
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr GetProcessHeap();

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr HeapCreate(uint flOptions, UIntPtr dwInitialsize, UIntPtr dwMaximumSize);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr HeapAlloc(IntPtr hHeap, uint dwFlags, int dwSize);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool HeapFree(IntPtr hHeap, uint dwFlags, IntPtr lpMem);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool HeapDestroy(IntPtr hHeap);
    }
}
