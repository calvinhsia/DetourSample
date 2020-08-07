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
        void StartDetours(out int parm2);

        void StopDetours(int pDetours);
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
                obj.DoHeapStackTests(parm1:123, out var x);
                Assert.AreEqual(124, x);

                obj.StartDetours(out var pDetours);
                obj.StopDetours(pDetours);


                Marshal.ReleaseComObject(obj);
            }
        }
    }
}
