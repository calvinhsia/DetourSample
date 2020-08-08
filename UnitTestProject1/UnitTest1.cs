using System;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTestProject1
{

    [ComVisible(true)]
    [Guid("45BAEA8A-59DC-4417-9BD5-CC4ED2A10C53")] //{45BAEA8A-59DC-4417-9BD5-CC4ED2A10C53}
    public struct HeapCollectStats
    {
        public int nStacksCollected;
        public int MyRtlAllocateHeapCount;
    }


    [ComVisible(true)]
    [Guid("1491F27F-5EB8-4A70-8651-23F1AB98AEC6")] ///{1491F27F-5EB8-4A70-8651-23F1AB98AEC6}
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface ITestHeapStacks
    {
        void DoHeapStackTests(int parm1, out int parm2, string strIn, out string strOut);
        void StartDetours(out IntPtr parm2);
        void SetHeapCollectParams(string HeapSizesToCollect, int NumFramesToCapture, int HeapAllocSizeMinValue, int StlAllocLimit);

        void GetHeapCollectionStats(ref HeapCollectStats HeapStats);
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
            using (var oInterop = new Interop())
            {
                var obj = GetTestHeapStacks(oInterop);
                var sb = new StringBuilder(500);
                obj.StartDetours(out var pDetours);
                int nSizeSpecial = 1027;
                obj.SetHeapCollectParams($"8:271 , 72:220,{nSizeSpecial}:0", NumFramesToCapture: 20, HeapAllocSizeMinValue: 1048576, StlAllocLimit: 65536 * 2);
                for (int i = 0; i < 100; i++)
                {
                    var x = Heap.HeapAlloc(Heap.GetProcessHeap(), 0, nSizeSpecial);
                    Heap.HeapFree(Heap.GetProcessHeap(), 0, x);
                }

                var heapStats = new HeapCollectStats();
                obj.GetHeapCollectionStats(ref heapStats);
                obj.StopDetours(pDetours);
                Marshal.ReleaseComObject(obj);
                Assert.Fail($"#HeapAlloc={heapStats.MyRtlAllocateHeapCount}");
            }
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
