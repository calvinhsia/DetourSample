using System;
using System.CodeDom;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTestProject1
{

    [ComVisible(true)]
    [Guid("45BAEA8A-59DC-4417-9BD5-CC4ED2A10C53")] //{45BAEA8A-59DC-4417-9BD5-CC4ED2A10C53}
    [StructLayout(LayoutKind.Sequential)]
    public struct HeapCollectStats
    {
        public int nStacksCollected;
        public int MyRtlAllocateHeapCount;
        public int NumDetailRecords;
        private HeapCollectStatDetail[] details; //ignore: Type library exporter warning processing 'UnitTestProject1.HeapCollectStats.details, UnitTestProject1'. Warning: The public struct contains one or more non-public fields that will be exported.
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
        public int NumStacks;
    }


    [ComVisible(true)]
    [Guid("1491F27F-5EB8-4A70-8651-23F1AB98AEC6")] ///{1491F27F-5EB8-4A70-8651-23F1AB98AEC6}
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface ITestHeapStacks
    {
        void DoHeapStackTests(int parm1, out int parm2, string strIn, out string strOut);
        void StartDetours(out IntPtr parm2);
        void SetHeapCollectParams(string HeapSizesToCollect, int NumFramesToCapture, int HeapAllocSizeMinValue, int StlAllocLimit);

        void GetHeapCollectionStats(IntPtr HeapStats);
        void StopDetours(IntPtr pDetours);
    }

    [TestClass]
    public class UnitTest1
    {
        Guid guidComClass = new Guid("A90F9940-53C9-45B9-B67B-EE2EDE51CC00");
        ITestHeapStacks GetTestHeapStacks(Interop oInterop)
        {
            var hr = oInterop.CoCreateFromFile("DetourClient.dll", guidComClass, typeof(ITestHeapStacks).GUID, out var pObject);
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
                obj.StopDetours(pDetours);
                Marshal.ReleaseComObject(obj);
            }
        }

        [TestMethod]
        public void TestCollectStacks()
        {
            var oInterop = new Interop();
            {
                var obj = GetTestHeapStacks(oInterop);
                var sb = new StringBuilder(500);
                int nSizeSpecial = 1027;
                var strStacksToCollect = $"16:16, 32:32,{nSizeSpecial}:0";
                //                strStacksToCollect = "";
                obj.SetHeapCollectParams(strStacksToCollect, NumFramesToCapture: 20, HeapAllocSizeMinValue: 1048576, StlAllocLimit: 65536 * 2);
                obj.StartDetours(out var pDetours);
                int nIter = 100000;
                for (int i = 0; i < nIter; i++)
                {
                    var x = Heap.HeapAlloc(Heap.GetProcessHeap(), 0, nSizeSpecial);
                    Heap.HeapFree(Heap.GetProcessHeap(), 0, x);
                }
                obj.StopDetours(pDetours);

                var heapStats = new HeapCollectStats(strStacksToCollect);
                var ptrHeapStats = heapStats.GetPointer();
                obj.GetHeapCollectionStats(ptrHeapStats);
                heapStats = HeapCollectStats.FromPointer(ptrHeapStats);
                Assert.IsTrue(heapStats.MyRtlAllocateHeapCount > nIter, $"Expected > {nIter}, got {heapStats.MyRtlAllocateHeapCount}");
                Marshal.FreeHGlobal(ptrHeapStats);
                Marshal.ReleaseComObject(obj);
                //                Assert.Fail($"#HeapAlloc={heapStats.MyRtlAllocateHeapCount}");
            }
            oInterop.Dispose();
        }


        [DllImport("kernel32.dll", SetLastError = true)]
        [PreserveSig]
        public static extern uint GetModuleFileName
        (
            [In] IntPtr hModule,
            [Out] StringBuilder lpFilename,
            [In][MarshalAs(UnmanagedType.U4)] int nSize
        );
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
