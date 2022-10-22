// structscan - a WinDbg extension that scans data structures for which you do not have 
// private symbols and attempts to find interesting data in them.
// Joseph Ryan Ries - 2022. Watch the development on this extension on video
// here: https://www.youtube.com/watch?v=d1uT8tmnhZI
//
// TODO: Currently the only way I have gotten this to work is if I install my callback to
// get debugger output, execute the debugger command, then reinstall the original callback
// to write text to the debugger window. I feel like this constant flipping back and forth
// between the two callbacks is probably wrong. There must be a better way. I feel like I'm
// supposed to be supplying an array of callbacks that will be called in series, but I don't know...

// Need to define INITGUID before dbgeng.h because we need to use the GUIDs
// from C, since we don't have __uuidof
#define INITGUID

#include <DbgEng.h>

#include <stdio.h>

#include "Main.h"

#define EXTENSION_VERSION_MAJOR	1

#define EXTENSION_VERSION_MINOR 0


wchar_t gOutputBuffer[4096];

wchar_t gCommandBuffer[128];



IDebugOutputCallbacks2* gPrevOutputCallback;

IDebugOutputCallbacks2 gOutputCallback;

IDebugOutputCallbacks2Vtbl gOcbVtbl = { 
	.AddRef = &CbAddRef,
	.Release = &CbRelease,
	.Output = &CbOutput,
	.Output2 = &CbOutput2,
	.QueryInterface = &CbQueryInterface,
	.GetInterestMask = &CbGetInterestMask };

// This is the entry point of our DLL. EngHost calls this as soon as you load the DLL.
__declspec(dllexport) HRESULT CALLBACK DebugExtensionInitialize(_Out_ PULONG Version, _Out_ PULONG Flags)
{
	*Version = DEBUG_EXTENSION_VERSION(EXTENSION_VERSION_MAJOR, EXTENSION_VERSION_MINOR);

	*Flags = 0;

	gOutputCallback.lpVtbl = &gOcbVtbl;

	return(S_OK);
}

__declspec(dllexport) HRESULT CALLBACK structscan(_In_ IDebugClient4* Client, _In_opt_ PCSTR Args)
{
	HRESULT Hr = S_OK;

	IDebugControl4* DebugControl = NULL;

	IDebugSymbols4* Symbols = NULL;	

	wchar_t WideArgs[128] = { 0 };

	wchar_t ModuleName[64] = { 0 };

	size_t CharsConverted = 0;

	ULONG ImageIndex = 0;

	ULONG64 ImageBase = 0;

	ULONG64 SearchHandle = 0;

	ULONG64 SymbolAddress = 0;

	DEBUG_MODULE_PARAMETERS ModuleParameters = { 0 };

	wchar_t* DisplayMemCommands[] = { L"dS", L"ds" };

	ULONG Offset = 0;

	BOOL EndScan = FALSE;

	mbstowcs_s(&CharsConverted, WideArgs, _countof(WideArgs), Args, _TRUNCATE);

	if ((Hr = Client->lpVtbl->QueryInterface(Client, &IID_IDebugControl4, (void**)&DebugControl)) != S_OK)
	{
		goto Exit;
	}

	if ((Hr = Client->lpVtbl->QueryInterface(Client, &IID_IDebugSymbols4, (void**)&Symbols)) != S_OK)
	{
		DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"QueryInterface failed! Hr = 0x%x!\n\n", Hr);

		goto Exit;
	}

	// !structscan ntdsai!gAnchor
	if (WideArgs == NULL ||
		wcslen(WideArgs) >= 64 ||
		wcschr(WideArgs, L'!') == NULL ||
		WideArgs[0] == '!' ||
		wcschr(WideArgs, L' ') ||
		wcschr(WideArgs, L'*'))
	{
		DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"structscan v%d.%d, Joseph Ryan Ries 2022\n\n", EXTENSION_VERSION_MAJOR, EXTENSION_VERSION_MINOR);

		DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"Scans data structures for which you do not have private symbols, looking for interesting data.\n\n");

		DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"USAGE: !structscan module!struct\n\n");
		
		goto Exit;
	}

	for (int c = 0; c < _countof(ModuleName); c++)
	{
		if (WideArgs[c] == L'!')
		{
			break;
		}

		ModuleName[c] = WideArgs[c];
	}

	if ((Hr = Symbols->lpVtbl->GetModuleByModuleNameWide(Symbols, ModuleName, 0, &ImageIndex, &ImageBase)) != S_OK)
	{
		DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"GetModuleByModuleNameWide failed! Hr = 0x%x!\n\n");

		goto Exit;
	}

	if ((Hr = Symbols->lpVtbl->GetModuleParameters(Symbols, 1, NULL, ImageIndex, &ModuleParameters)) != S_OK)
	{
		DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"GetModuleParameters failed! Hr = 0x%x!\n\n");

		goto Exit;
	}

	DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"Module name: %s\nImage base: 0x%p\nMemory Size: %llu\n", ModuleName, ImageBase, ModuleParameters.Size);

	if ((Hr = Symbols->lpVtbl->StartSymbolMatchWide(Symbols, WideArgs, &SearchHandle)) != S_OK)
	{
		DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"StartSymbolMatchWide failed! Hr = 0x%x!\n\n");

		goto Exit;
	}
	
	Symbols->lpVtbl->GetNextSymbolMatch(Symbols, SearchHandle, NULL, 0, 0, &SymbolAddress);

	Symbols->lpVtbl->EndSymbolMatch(Symbols, SearchHandle);

	if (SymbolAddress == 0)
	{
		DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"Symbol %s was not found!\n\n", WideArgs);

		goto Exit;
	}

	DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"Symbol Address: %p\n", SymbolAddress);

	DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"Press Ctrl+C to abort...\n");

	if ((Hr = Client->lpVtbl->GetOutputCallbacks(Client, (PDEBUG_OUTPUT_CALLBACKS)&gPrevOutputCallback)) != S_OK)
	{
		DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"Failed to get existing output callback! Hr = 0x%x\n\n\n", Hr);

		goto Exit;
	}

	if ((Hr = Client->lpVtbl->SetOutputCallbacks(Client, (PDEBUG_OUTPUT_CALLBACKS)&gOutputCallback)) != S_OK)
	{
		DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"Failed to set output callback! Hr = 0x%x\n\n\n", Hr);

		goto Exit;
	}

	


	for (int c = 0; c < _countof(DisplayMemCommands); c++)
	{
		Offset = 0;

		if (EndScan == TRUE)
		{
			break;
		}

		while (EndScan == FALSE)
		{
			wchar_t SymbolName[64] = { 0 };

			_snwprintf_s(gCommandBuffer, _countof(gCommandBuffer), _TRUNCATE, L"%s %s+0x%lx", DisplayMemCommands[c], WideArgs, Offset);

			DebugControl->lpVtbl->ExecuteWide(DebugControl, DEBUG_OUTCTL_THIS_CLIENT, gCommandBuffer, DEBUG_EXECUTE_DEFAULT);

			if (gPrevOutputCallback)
			{
				if ((Hr = Client->lpVtbl->SetOutputCallbacks(Client, (PDEBUG_OUTPUT_CALLBACKS)gPrevOutputCallback)) != S_OK)
				{
					DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"Failed to restore original callback!\n\n");
				}
			}

			if (wcsstr(gOutputBuffer, L"???") == 0 && (wcslen(gOutputBuffer) > 0))
			{
				DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"%s = %s", gCommandBuffer, gOutputBuffer);
			}

			memset(gOutputBuffer, 0, sizeof(gOutputBuffer));

			Client->lpVtbl->SetOutputCallbacks(Client, (PDEBUG_OUTPUT_CALLBACKS)&gOutputCallback);

			Offset += 2;

			Symbols->lpVtbl->GetNameByOffsetWide(Symbols, (SymbolAddress + Offset), SymbolName, _countof(SymbolName), NULL, NULL);

			if (wcscmp(SymbolName, WideArgs) != 0)
			{
				break;
			}

			if ((Offset >= 0x1000) || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(0x43) & 0x8000)))
			{
				EndScan = TRUE;
			}
		}
	}

Exit:

	if (gPrevOutputCallback)
	{
		if ((Hr = Client->lpVtbl->SetOutputCallbacks(Client, (PDEBUG_OUTPUT_CALLBACKS)gPrevOutputCallback)) != S_OK)
		{
			DebugControl->lpVtbl->OutputWide(DebugControl, DEBUG_OUTCTL_ALL_CLIENTS, L"Failed to restore original callback!\n\n");
		}
	}

	if (Symbols)
	{
		Symbols->lpVtbl->Release(Symbols);
	}

	if (DebugControl)
	{
		DebugControl->lpVtbl->Release(DebugControl);		
	}

	return(Hr);
}

// All of these CbXXX functions are my implementations of the methods that you would
// find on a IDebugCallbacks2 interface. I will put pointers to all of these functions
// into a "lpVtbl" and set that onto my callback during the extension initialization routine.
// All of these functions need to exist even if they don't do anything because the debug engine
// will try to call them and enghost will crash if any function pointers are null.

ULONG __cdecl CbAddRef(IDebugOutputCallbacks2* This)
{
	UNREFERENCED_PARAMETER(This);

	return(1);
}

ULONG __cdecl CbQueryInterface(IDebugOutputCallbacks2* This, IN REFIID InterfaceId, OUT PVOID* Interface)
{
	UNREFERENCED_PARAMETER(InterfaceId);

	*Interface = This;

	CbAddRef(This);

	return(S_OK);
}

ULONG __cdecl CbRelease(IDebugOutputCallbacks2* This)
{
	UNREFERENCED_PARAMETER(This);

	return(0);
}

HRESULT __cdecl CbGetInterestMask(IDebugOutputCallbacks2* This, OUT PULONG Mask)
{
	UNREFERENCED_PARAMETER(This);

	*Mask = DEBUG_OUTCBI_ANY_FORMAT;

	return(S_OK);
}

HRESULT __stdcall CbOutput(IDebugOutputCallbacks2* This, IN ULONG Mask, IN PCSTR Text)
{
	UNREFERENCED_PARAMETER(This);

	UNREFERENCED_PARAMETER(Mask);

	UNREFERENCED_PARAMETER(Text);

	return(S_OK);
}

HRESULT __stdcall CbOutput2(IDebugOutputCallbacks2* This, IN ULONG Which, IN ULONG Flags, IN ULONG64 Arg, PCWSTR Text)
{
	UNREFERENCED_PARAMETER(This);

	UNREFERENCED_PARAMETER(Which);

	UNREFERENCED_PARAMETER(Flags);

	UNREFERENCED_PARAMETER(Arg);

	wcscpy_s(gOutputBuffer, _countof(gOutputBuffer), Text);

	return(S_OK);
}