﻿using System;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTestProject1
{
    [ComVisible(true)]
    [Guid("1491F27F-5EB8-4A70-8651-23F1AB98AEC6")] ///{1491F27F-5EB8-4A70-8651-23F1AB98AEC6}
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface ITestHeapStacks
    {
        void DoHeapStackTests(int parm1, out int parm2, string strIn, out string strOut);
        void StartDetours(out IntPtr parm2);

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
        [DllImport("kernel32.dll", SetLastError = true)]
        [PreserveSig]
        public static extern uint GetModuleFileName
        (
            [In] IntPtr hModule,
            [Out] StringBuilder lpFilename,
            [In][MarshalAs(UnmanagedType.U4)] int nSize
        );
    }
}
