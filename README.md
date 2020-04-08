# DetourSample
Sample of using Microsoft Detours

What is Detours?  https://github.com/Microsoft/Detours (you can find my name in the credits.txt)

A few years ago I rewrote the Visual Studio detouring code to be more performant.
We were detouring from 3 times from 3 different DLLs throughout process startup. 
The later in the process startup, the more expensive it is to suspend all threads to detour
as many threads are created by starting threadpools and managed code .

This sample shows how to detour multiple sets of various Windows APIs very early in startup, 
which means fewer threads to suspend, shared memory, faster implementation, etc.
