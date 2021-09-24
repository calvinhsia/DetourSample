using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Runtime.InteropServices;

namespace Test64
{
    using static NativeMethods;
    [TestClass]
    public class Test64
    {
        public TestContext TestContext { get; set; }
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate void delTestInterop(
            //IntPtr pContext,
            //int nSkipFrames,
            //int nFrames,
            //[MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] IntPtr[] frames,
            //ref UInt64 pHash
            );

//        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        delegate int delGetCallStack(
            IntPtr pContext,
            int nSkipFrames,
            int nFrames,
            [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] IntPtr[] frames,
            ref int nFramesWritten,
            ref UInt64 pHash
            );

        [TestMethod]
        [Ignore]
        public void TestMethod1()
        {
            var fname = @"C:\Users\calvinh\source\repos\DetourSample\x64\Debug\VUnwind64.exe";
            fname = @"C:\Users\calvinh\Source\Repos\DetourSample\x64\Debug\Dll64.dll";
            var hmod = LoadLibrary(fname);
            {
//                var addr = GetProcAddress(hmod, "TestInterop");
//                var TestInterop = Marshal.GetDelegateForFunctionPointer<delTestInterop>(addr);
//                TestContext.WriteLine($"hmod = {hmod.ToInt64():x}  addr= {addr.ToInt64():x}   del = {TestInterop}");
//                UInt64 hash = 0;
//                int nFrames = 20;
//                var arrFrames = new IntPtr[nFrames];
//                TestInterop(
////                    IntPtr.Zero, 1, nFrames, arrFrames, ref hash
//                    );
//                //TestContext.WriteLine($"Res = {res}");
//                Array.ForEach(arrFrames, (f) =>
//                 {
//                     TestContext.WriteLine($" {f.ToInt64():x}");

//                 });
            }
            {
                var addr = GetProcAddress(hmod, "GetCallstack64");
                var GetCallStack = Marshal.GetDelegateForFunctionPointer<delGetCallStack>(addr);
                TestContext.WriteLine($"hmod = {hmod.ToInt64():x}  addr= {addr.ToInt64():x}   del = {GetCallStack}");
                int nFrames = 200;
                var arrFrames = new IntPtr[nFrames];
                UInt64 hash = 0;
                int nFramesCollected = 0;
                var hr = GetCallStack(pContext: IntPtr.Zero, nSkipFrames: 0, nFrames: nFrames, frames: arrFrames,nFramesWritten:ref nFramesCollected, pHash: ref hash);
                TestContext.WriteLine($"hr = {hr} #frames = {nFramesCollected}  Hash = {hash:x}");
                for (int i = 0; i < nFramesCollected; i++)
                {
                    TestContext.WriteLine($"{i,3} {arrFrames[i].ToInt64():x}");
                }
                //Array.ForEach(arrFrames, (f) =>
                // {
                //     TestContext.WriteLine($" {f.ToInt64():x}");

                // });

            }


            FreeLibrary(hmod);

        }
    }
    internal static class NativeMethods
    {
        private const string Kernel32LibraryName = "kernel32.dll";

        [DllImport(Kernel32LibraryName, SetLastError = true)]
        public static extern IntPtr OpenProcess(int dwDesiredAccess, bool bInheritHandle, int dwProcessId);

        [DllImport(Kernel32LibraryName, SetLastError = true)]
        public static extern bool CloseHandle(IntPtr hObject);

        [DllImport(Kernel32LibraryName)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool FreeLibrary(IntPtr hModule);

        [DllImport(Kernel32LibraryName, CharSet = CharSet.Unicode, SetLastError = true, EntryPoint = "LoadLibraryW")]
        public static extern IntPtr LoadLibrary(string lpLibFileName);

        [DllImport(Kernel32LibraryName)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool IsWow64Process(IntPtr hProcess, out bool isWow64);

        [DllImport(Kernel32LibraryName)]
        public static extern IntPtr GetProcAddress(IntPtr hModule, string procedureName);
    }
}
