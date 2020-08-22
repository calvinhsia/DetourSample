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
    public struct HeapCollectStats : IDisposable
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
        public void Dispose()
        {

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

    // for each alloc of particular alloc size, there is a hash and a variable length array of IntPtrs
    [ComVisible(true)]
    [Guid("7133738F-0654-43D1-AAE9-75C947C11253")]//{7133738F-0654-43D1-AAE9-75C947C11253}
    [StructLayout(LayoutKind.Sequential)]
    public struct CollectedStack
    {
        public int sizeAlloc;
        public int stackHash;
        public int numOccur; // the # of times this stack was found 
        public int numFrames;
        public IntPtr pFrameArray; // the frames // must be freed freed
        public override string ToString()
        {
            return $"{nameof(sizeAlloc)}={sizeAlloc} {nameof(stackHash)}={stackHash:x8} {nameof(numOccur)}={numOccur} {nameof(numFrames)}={numFrames}";
        }
    }
    struct LiveHeapAllocation
    {
        public IntPtr addr;
        public uint stackhash;
    }

    [ComVisible(true)]
    [Guid("1491F27F-5EB8-4A70-8651-23F1AB98AEC6")] ///{1491F27F-5EB8-4A70-8651-23F1AB98AEC6}
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface ITestHeapStacks
    {
        void DoHeapStackTests(int parm1, out int parm2, string strIn, out string strOut);
        void StartDetours(out IntPtr parm2);
        void SetHeapCollectParams(string HeapSizesToCollect, int NumFramesToCapture, int HeapAllocSizeMinValue, int StlAllocLimit);

        void StopDetours(IntPtr pDetours);

        void GetStats(IntPtr HeapStats);
        void CollectStacksUninitialize();
        void GetAllocationAddresses(
            ref int numAllocs,
            ref IntPtr ptrData);
        void GetCollectedAllocStacks(int allocSize, ref int numStacks, ref IntPtr allocStacks);
    }

    [TestClass]
    public class UnitTest1
    {
        readonly Random _random = new Random(1);
        Guid guidComClass = new Guid("A90F9940-53C9-45B9-B67B-EE2EDE51CC00");
        private bool _EnableLogging;
        private bool _DoCSLife;

        public TestContext TestContext { get; set; }


        public void LogMessage(string msg)
        {
            if (this._EnableLogging)
            {
                msg = DateTime.Now.ToString("hh:mm:ss:fff") + $" {Thread.CurrentThread.ManagedThreadId} " + msg;
                TestContext.WriteLine(msg);
            }
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
        [TestInitialize]
        public void TestInit()
        {
            this._EnableLogging = true; // for leak testing we don't want to log
            this._DoCSLife = true;
        }

        [TestMethod]
        public void TestPlumbing()
        {
            for (int iter = 0; iter < 100; iter++)
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
                    obj.StopDetours(pDetours);
                    obj.CollectStacksUninitialize();
                    Marshal.ReleaseComObject(obj);
                }
            }
        }
        [TestMethod]
        public async Task TestStressStartup()
        {
            int nSizeSpecial = 1027;
            var strStacksToCollect = $"{nSizeSpecial}:0";
            using (var oInterop = new Interop())
            {
                var obj = GetTestHeapStacks(oInterop);
                obj.SetHeapCollectParams(strStacksToCollect, NumFramesToCapture: 20, HeapAllocSizeMinValue: 1048576, StlAllocLimit: 65536 * 10);
                int nIter = 100;
                int nThreads = 80;
                bool DidStartDetouring = false;
                IntPtr pDetours = IntPtr.Zero;
                var lstTasks = new List<Task>
                {
                    Task.Run(() => {LogMessage("Start CSLife");DoCSLife(); })
                };
                var procHeap = Heap.GetProcessHeap();
                int nIndex = 0;
                for (int iThread = 0; iThread < nThreads; iThread++)
                {
                    var task = Task.Run(() =>
                    {
                        if (Interlocked.Increment(ref nIndex) == 4)
                        {
                            if (!DidStartDetouring)
                            {
                                LogMessage($"Start Detours");
                                obj.StartDetours(out pDetours);
                            }
                        }
                        LogMessage($"Doallocs");
                        DoSomeNativeAllocs(nIter, iThread, nSizeSpecial, lstIntentionalLeaks: null); // don't do any intentional leaks
                    });
                    lstTasks.Add(task);
                }
                await Task.WhenAll(lstTasks);
                obj.StopDetours(pDetours);

                obj.CollectStacksUninitialize();
                Marshal.ReleaseComObject(obj);
                //                Assert.Fail($"#HeapAlloc={heapStats.MyRtlAllocateHeapCount}");
            }
        }

        [TestMethod]
        public async Task TestStressStartupNoTPool()
        {
            int nSizeSpecial = 1027;
            var strStacksToCollect = $"{nSizeSpecial}:0";
            using (var oInterop = new Interop())
            {
                var obj = GetTestHeapStacks(oInterop);
                obj.SetHeapCollectParams(strStacksToCollect, NumFramesToCapture: 20, HeapAllocSizeMinValue: 1048576, StlAllocLimit: 65536 * 10);
                int nIter = 100;
                int nThreads = 100;
                var lstTCS = new List<TaskCompletionSource<int>>();
                for (int i = 0; i < nThreads + 1; i++)
                {
                    lstTCS.Add(new TaskCompletionSource<int>());
                }
                bool DidStartDetouring = false;
                IntPtr pDetours = IntPtr.Zero;
                var lstThreads = new List<Thread>();
                var thrCSLife = new Thread((p) =>
                {
                    LogMessage("Start CSLife"); DoCSLife(); lstTCS[nThreads].SetResult(0);
                });
                thrCSLife.Start();
                lstThreads.Add(thrCSLife);
                var procHeap = Heap.GetProcessHeap();
                int nIndex = 0;

                for (int iThread = 0; iThread < nThreads; iThread++)
                {
                    var thrIndex = iThread; // lambda loop iterator
                    var thr = new Thread((p) =>
                    {
                        if (Interlocked.Increment(ref nIndex) == 4)
                        {
                            if (!DidStartDetouring)
                            {
                                LogMessage($"Start Detours");
                                obj.StartDetours(out pDetours);
                            }
                        }
                        LogMessage($"Doallocs");
                        DoSomeNativeAllocs(nIter, iThread, nSizeSpecial, lstIntentionalLeaks: null); // don't do any intentional leaks
                        lstTCS[thrIndex].SetResult(0);
                    });
                    thr.Start();
                    lstThreads.Add(thr);
                }
                await Task.WhenAll(lstTCS.Select(t => t.Task));
                obj.StopDetours(pDetours);

                obj.CollectStacksUninitialize();
                Marshal.ReleaseComObject(obj);
                //                Assert.Fail($"#HeapAlloc={heapStats.MyRtlAllocateHeapCount}");
            }
        }

        [TestMethod]
        public async Task TestStress()
        {
            this._EnableLogging = false;
            this._DoCSLife = false;
            for (int i = 0; i < 10; i++)
            {
                LogMessage($"Stress iter {i}");
                await TestCollectStacks();
            }
            await Task.Yield();
        }

        [TestMethod]
        public void TestCollectStackAddresses()
        {
            int nSizeSpecial = 1027;
            var strStacksToCollect = $"{nSizeSpecial}:0";
            using (var oInterop = new Interop())
            {
                var obj = GetTestHeapStacks(oInterop);
                obj.SetHeapCollectParams(strStacksToCollect, NumFramesToCapture: 20, HeapAllocSizeMinValue: 1048576, StlAllocLimit: 65536 * 10);
                obj.StartDetours(out var pDetours);
                int nIter = 100;
                int nThreads = 1;
                var lstIntentionalLeaks = new List<IntPtr>();
                for (int iThread = 0; iThread < nThreads; iThread++)
                {
                    DoSomeNativeAllocs(nIter, iThread, nSizeSpecial, lstIntentionalLeaks);
                }
                LogMessage($"Intentional Leaks {lstIntentionalLeaks.Count:n0}  TotAllocs={nIter * nThreads:n0}");
                var heapStats = new HeapCollectStats(strStacksToCollect);
                obj.StopDetours(pDetours);
                var ptrHeapStats = heapStats.GetPointer();
                obj.GetStats(ptrHeapStats);

                heapStats = HeapCollectStats.FromPointer(ptrHeapStats);
                LogMessage($"Allocation stacks {heapStats}");
                Array.ForEach<HeapCollectStatDetail>(heapStats.details, (detail) =>
                {
                    LogMessage($"  {detail}");
                });
                Assert.IsTrue(heapStats.MyRtlAllocateHeapCount > nIter * nThreads, $"Expected > {nIter * nThreads}, got {heapStats.MyRtlAllocateHeapCount}");
                if (!heapStats.fReachedMemLimit)
                {
                    var det = heapStats.details.Where(s => s.AllocSize == nSizeSpecial).Single();
                    Assert.IsTrue(det.NumStacksCollected >= nIter, $"Expected numstacks collected ({det.NumStacksCollected}) > {nIter}");
                }
                {
                    DumpCallStacksCollected(obj, heapStats, nSizeSpecial);
                }
                {
                    var lstLiveAllocs = new List<LiveHeapAllocation>();
                    int numAllocs = 0;
                    IntPtr ptrData = IntPtr.Zero;
                    obj.GetAllocationAddresses(ref numAllocs, ref ptrData);
                    LogMessage($"# LiveAllocStacks (allocs that are still live)= {numAllocs}");
                    for (int i = 0; i < numAllocs; i++)
                    {
                        var allocdata = Marshal.PtrToStructure<LiveHeapAllocation>(ptrData + i * IntPtr.Size);
                        var str = "Freed";
                        if (lstIntentionalLeaks.Contains(allocdata.addr))
                        {
                            str = Marshal.PtrToStringAnsi(allocdata.addr);
                            Assert.IsTrue(str.StartsWith("This is a test string"));
                        }
                        LogMessage($" {i}  addr={(uint)(allocdata.addr):x8} stackhash={allocdata.stackhash:x8} str={str}");
                    }
                    Marshal.FreeHGlobal(ptrData);
                }

                foreach (var addr in lstIntentionalLeaks)
                {
                    Heap.HeapFree(Heap.GetProcessHeap(), 0, addr);
                }
                heapStats.Dispose();
                obj.CollectStacksUninitialize();
                Marshal.FreeHGlobal(ptrHeapStats);
                Marshal.ReleaseComObject(obj);
                //                Assert.Fail($"#HeapAlloc={heapStats.MyRtlAllocateHeapCount}");
            }
        }

        private void DumpCallStacksCollected(ITestHeapStacks obj, HeapCollectStats heapStats, int nSizeSpecial)
        {
            foreach (var det in heapStats.details)
            {
                int numStacks = 0;
                IntPtr allocStacks = IntPtr.Zero;
                obj.GetCollectedAllocStacks(det.AllocSize, ref numStacks, ref allocStacks);
                for (int nStack = 0; nStack < numStacks; nStack++)
                {
                    var collectedStack = Marshal.PtrToStructure<CollectedStack>(allocStacks + nStack * Marshal.SizeOf<CollectedStack>());
                    LogMessage($"Num AllocStacks Collected for size {det.AllocSize}= {numStacks}  {collectedStack}");
                    //The 1st time a stack is encountered, the stack is slightly diff because the code adds an new entry for the stack.
                    // subsequent occurrences of the same stack don't need to add the entry, so they are a different stack.
                    // Release builds have optimized C++ code, so the stacks are the same
                    for (int iFrame = 0; iFrame < collectedStack.numFrames; iFrame++)
                    {
                        var addr = Marshal.ReadIntPtr(collectedStack.pFrameArray + iFrame * IntPtr.Size);
                        LogMessage($"  {(uint)addr:x8}");
                    }
                    Marshal.FreeHGlobal(collectedStack.pFrameArray);
                }
                if (!heapStats.fReachedMemLimit && det.AllocSize == nSizeSpecial)
                {
                    Assert.IsTrue(numStacks > 0, $"Expected > 0 stacks for {det}");
                }
                Marshal.FreeHGlobal(allocStacks);
            }
        }

        [TestMethod]
        public async Task TestCollectStacks()
        {
            int nSizeSpecial = 1027;
            var strStacksToCollect = $"16:16, 32:32,{nSizeSpecial}:0";
            await TestCollectStacksHelper(strStacksToCollect, nSizeSpecial, NumFramesToCapture: 20, HeapAllocSizeMinValue: 1048576, StlAllocLimit: 65536 * 350, IsLimited: false, nThreads: 60);
        }

        [TestMethod]
        public async Task TestCollectStacksWithLimit()
        {
            int nSizeSpecial = 1027;
            var strStacksToCollect = $"16:16, 32:32,{nSizeSpecial}:0";
            await TestCollectStacksHelper(strStacksToCollect, nSizeSpecial, NumFramesToCapture: 20, HeapAllocSizeMinValue: 1048576, StlAllocLimit: 4096, IsLimited: true, WaitForDoCsLife: true);
        }



        async Task TestCollectStacksHelper(
            string strStacksToCollect,
            int nSizeSpecial,
            int NumFramesToCapture,
            int HeapAllocSizeMinValue,
            int StlAllocLimit,
            int nIter = 10000,
            int nThreads = 60,
            bool IsLimited = false,
            bool WaitForDoCsLife = false)
        {
            using (var oInterop = new Interop())
            {
                var obj = GetTestHeapStacks(oInterop);
                obj.SetHeapCollectParams(strStacksToCollect, NumFramesToCapture, HeapAllocSizeMinValue, StlAllocLimit);
                obj.StartDetours(out var pDetours);
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
                        DoSomeNativeAllocs(nIter, iThread, nSizeSpecial, lstIntentionalLeaks);
                    });
                    lstTasks.Add(task);
                }
                await Task.WhenAll(lstTasks);
                //                await Task.Delay(TimeSpan.FromSeconds(1)); // let things settle down before undetouring
                LogMessage($"Intentional Leaks {lstIntentionalLeaks.Count:n0}  TotAllocs={nIter * nThreads:n0}");
                obj.StopDetours(pDetours);
                //                await Task.Delay(TimeSpan.FromSeconds(1)); // let things settle down before undetouring
                var heapStats = new HeapCollectStats(strStacksToCollect);
                var ptrHeapStats = heapStats.GetPointer();
                obj.GetStats(ptrHeapStats);

                heapStats = HeapCollectStats.FromPointer(ptrHeapStats);
                LogMessage($"Allocation stacks {heapStats}");
                Array.ForEach<HeapCollectStatDetail>(heapStats.details, (detail) =>
                 {
                     LogMessage($"  {detail}");
                 });
                Assert.IsTrue(heapStats.MyRtlAllocateHeapCount > nIter * nThreads, $"Expected > {nIter * nThreads}, got {heapStats.MyRtlAllocateHeapCount}");
                var det = heapStats.details.Where(s => s.AllocSize == nSizeSpecial).Single();
                if (IsLimited)
                {
                    Assert.IsTrue(heapStats.fReachedMemLimit, $"Test says limited memory, but didn't reach limit");
                }
                else
                {
                    Assert.IsFalse(heapStats.fReachedMemLimit, $"Test says not limited mem, but did reach limit");
                }
                if (!heapStats.fReachedMemLimit)
                {
                    Assert.IsTrue(det.NumStacksCollected >= nIter, $"Expected numstacks collected ({det.NumStacksCollected}) > {nIter}");
                }
                DumpCallStacksCollected(obj, heapStats, nSizeSpecial);

                foreach (var addr in lstIntentionalLeaks)
                {
                    Heap.HeapFree(procHeap, 0, addr);
                }
                heapStats.Dispose();
                obj.CollectStacksUninitialize();
                Marshal.FreeHGlobal(ptrHeapStats);
                Marshal.ReleaseComObject(obj);
                //                Assert.Fail($"#HeapAlloc={heapStats.MyRtlAllocateHeapCount}");
            }
        }

        private void DoSomeNativeAllocs(int nIter, int iThread, int nSizeSpecial, List<IntPtr> lstIntentionalLeaks)
        {
            var procHeap = Heap.GetProcessHeap();
            //                        LogMessage($"Task Alloc {iThread} {nIter}");
            for (int i = 0; i < nIter; i++)
            {
                var addr = Heap.HeapAlloc(procHeap, 0, nSizeSpecial);
                if (lstIntentionalLeaks != null && _random.Next(100) < 50)
                {
                    var teststr = $"This is a test string {i} {iThread}\0";
                    var bytes = ASCIIEncoding.ASCII.GetBytes(teststr);
                    Marshal.Copy(bytes, 0, addr, teststr.Length);
                    lstIntentionalLeaks.Add(addr);
                }
                else
                {
                    Heap.HeapFree(procHeap, 0, addr);
                }
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
            if (_DoCSLife)
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
