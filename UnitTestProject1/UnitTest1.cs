using System;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTestProject1
{
    [ComVisible(true)]
    [Guid("1491F27F-5EB8-4A70-8651-23F1AB98AEC6")] ///{1491F27F-5EB8-4A70-8651-23F1AB98AEC6}
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface ITestHeapStacks
    {
        void DoHeapStackTests(int parm1, out int parm2);
    }

    [TestClass]
    public class UnitTest1
    {
        [TestMethod]
        public void TestMethod1()
        {
            using (var oInterop = new Interop())
            {
                var guidComClass = new Guid("A90F9940-53C9-45B9-B67B-EE2EDE51CC00");

                var hr = oInterop.CoCreateFromFile("DetourClient.dll", guidComClass, typeof(ITestHeapStacks).GUID, out var pObject);
                var obj = (ITestHeapStacks)Marshal.GetTypedObjectForIUnknown(pObject, typeof(ITestHeapStacks));
                
                obj.DoHeapStackTests(parm1:123, out var x);
                Assert.AreEqual(124, x);
                Marshal.ReleaseComObject(obj);
            }
        }
    }
}
