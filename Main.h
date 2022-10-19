#pragma once

__declspec(dllexport) HRESULT CALLBACK DebugExtensionInitialize(_Out_ PULONG Version, _Out_ PULONG Flags);

__declspec(dllexport) HRESULT CALLBACK structscan(_In_ IDebugClient4* Client, _In_opt_ PCSTR Args);