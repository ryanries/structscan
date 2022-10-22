#pragma once

// This is the entry point of our DLL. EngHost calls this as soon as you load the DLL.
__declspec(dllexport) HRESULT CALLBACK DebugExtensionInitialize(_Out_ PULONG Version, _Out_ PULONG Flags);

__declspec(dllexport) HRESULT CALLBACK structscan(_In_ IDebugClient4* Client, _In_opt_ PCSTR Args);


// All of these CbXXX functions are my implementations of the methods that you would
// find on a IDebugCallbacks2 interface. I will put pointers to all of these functions
// into a "lpVtbl" and set that onto my callback during the extension initialization routine.
// All of these functions need to exist even if they don't do anything because the debug engine
// will try to call them and enghost will crash if any function pointers are null.

ULONG __cdecl CbAddRef(IDebugOutputCallbacks2* This);

ULONG __cdecl CbQueryInterface(IDebugOutputCallbacks2* This, IN REFIID InterfaceId, OUT PVOID* Interface);

ULONG __cdecl CbRelease(IDebugOutputCallbacks2* This);

HRESULT __cdecl CbGetInterestMask(IDebugOutputCallbacks2* This, OUT PULONG Mask);

HRESULT __stdcall CbOutput(IDebugOutputCallbacks2* This, IN ULONG Mask, IN PCSTR Text);

HRESULT __stdcall CbOutput2(IDebugOutputCallbacks2* This, IN ULONG Which, IN ULONG Flags, IN ULONG64 Arg, PCWSTR Text);