using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace WpfClient64
{
    using static NativeMethods;
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            this.Loaded += MainWindow_Loaded;
        }
//        [UnmanagedFunctionPointer(CallingConvention.Winapi)]
        delegate void delTestInterop(
            //IntPtr pContext,
            //int nSkipFrames,
            //int nFrames,
            //[MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] IntPtr[] frames,
            //ref UInt64 pHash
            );

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            var fname = @"C:\Users\calvinh\source\repos\DetourSample\x64\Debug\VUnwind64.exe";
            var hmod = LoadLibrary(fname);
            {
                var addr = GetProcAddress(hmod, "TestInterop");
                var TestInterop = Marshal.GetDelegateForFunctionPointer<delTestInterop>(addr);
                //TestContext.WriteLine($"hmod = {hmod.ToInt64():x}  addr= {addr.ToInt64():x}   del = {TestInterop}");
                UInt64 hash = 0;
                int nFrames = 20;
                var arrFrames = new IntPtr[nFrames];
                TestInterop(
                    //                    IntPtr.Zero, 1, nFrames, arrFrames, ref hash
                    );
            }
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
