using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using Spectre.Console;
using static LauncherPaths;
using static LauncherLog;

internal static class DllInjector
{
    public static void InjectDll(int pid, string dllPath, string logPath)
    {
        var fullDllPath = Path.GetFullPath(dllPath);
        if (!File.Exists(fullDllPath))
        {
            throw new FileNotFoundException("DLL not found", fullDllPath);
        }
    
        AnsiConsole.MarkupLineInterpolated($"[green]Injecting DLL[/] into pid={pid}: {fullDllPath}");
        Log(logPath, $"inject_start pid={pid} dll={fullDllPath}");

        try
        {
            var processHandle = NativeMethods.OpenProcess(
                NativeMethods.ProcessAccessFlags.CreateThread
                | NativeMethods.ProcessAccessFlags.QueryInformation
                | NativeMethods.ProcessAccessFlags.VirtualMemoryOperation
                | NativeMethods.ProcessAccessFlags.VirtualMemoryWrite
                | NativeMethods.ProcessAccessFlags.VirtualMemoryRead,
                false,
                pid);
            if (processHandle == IntPtr.Zero)
            {
                ThrowWin32("OpenProcess");
            }

            try
            {
                var dllBytes = System.Text.Encoding.Unicode.GetBytes(fullDllPath + "\0");
                var remotePath = NativeMethods.VirtualAllocEx(
                processHandle,
                IntPtr.Zero,
                (UIntPtr)dllBytes.Length,
                NativeMethods.AllocationType.Commit | NativeMethods.AllocationType.Reserve,
                NativeMethods.MemoryProtection.ReadWrite);
                if (remotePath == IntPtr.Zero)
                {
                    ThrowWin32("VirtualAllocEx");
                }

                try
                {
                    if (!NativeMethods.WriteProcessMemory(
                            processHandle,
                            remotePath,
                            dllBytes,
                            dllBytes.Length,
                            out var bytesWritten)
                        || bytesWritten.ToInt64() != dllBytes.Length)
                    {
                        ThrowWin32("WriteProcessMemory");
                    }

                    var kernel32 = NativeMethods.GetModuleHandle("kernel32.dll");
                    if (kernel32 == IntPtr.Zero)
                    {
                        ThrowWin32("GetModuleHandle(kernel32.dll)");
                    }

                    var loadLibrary = NativeMethods.GetProcAddress(kernel32, "LoadLibraryW");
                    if (loadLibrary == IntPtr.Zero)
                    {
                        ThrowWin32("GetProcAddress(LoadLibraryW)");
                    }

                    var threadHandle = NativeMethods.CreateRemoteThread(
                        processHandle,
                        IntPtr.Zero,
                        0,
                        loadLibrary,
                        remotePath,
                        0,
                        out var threadId);
                    if (threadHandle == IntPtr.Zero)
                    {
                        ThrowWin32("CreateRemoteThread");
                    }

                    try
                    {
                        var wait = NativeMethods.WaitForSingleObject(threadHandle, 10000);
                        if (wait != NativeMethods.WaitObject0)
                        {
                            throw new TimeoutException($"Remote LoadLibraryW thread did not finish. wait=0x{wait:X}");
                        }

                        if (!NativeMethods.GetExitCodeThread(threadHandle, out var exitCode))
                        {
                            ThrowWin32("GetExitCodeThread");
                        }

                        AnsiConsole.MarkupLineInterpolated($"[green]DLL injection[/] thread={threadId} exit=0x{exitCode:X}");
                        Log(logPath, $"inject_done pid={pid} thread={threadId} exit=0x{exitCode:X}");
                        if (exitCode == 0)
                        {
                            throw new InvalidOperationException("Remote LoadLibraryW returned 0.");
                        }
                    }
                    finally
                    {
                        NativeMethods.CloseHandle(threadHandle);
                    }
                }
                finally
                {
                    NativeMethods.VirtualFreeEx(processHandle, remotePath, UIntPtr.Zero, NativeMethods.FreeType.Release);
                }
            }
            finally
            {
                NativeMethods.CloseHandle(processHandle);
            }
        }
        catch (Exception ex)
        {
            Log(logPath, $"inject_failed pid={pid} dll={fullDllPath} error=\"{ex.Message.Replace("\"", "'")}\" type={ex.GetType().Name}");
            throw;
        }
    }
    
    public static void ThrowWin32(string operation)
    {
        var error = Marshal.GetLastWin32Error();
        var message = new Win32Exception(error).Message;
        throw new Win32Exception(error, $"{operation} failed win32={error}: {message}");
    }
}
