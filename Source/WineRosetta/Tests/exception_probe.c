#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>

static volatile LONG stop_worker;
static volatile LONG setup_count;
static volatile LONG handled_exceptions;

static LONG CALLBACK handle_probe_exception(PEXCEPTION_POINTERS exception) {
    if (exception->ExceptionRecord->ExceptionCode != 0xe0420001) return EXCEPTION_CONTINUE_SEARCH;
    InterlockedIncrement(&handled_exceptions);
    return EXCEPTION_CONTINUE_EXECUTION;
}

static DWORD WINAPI adapter_worker(void *unused) {
    (void)unused;
    while (!InterlockedCompareExchange(&stop_worker, 0, 0)) {
        IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (d3d) {
            D3DADAPTER_IDENTIFIER9 id;
            D3DCAPS9 caps;
            IDirect3D9_GetAdapterIdentifier(d3d, 0, 0, &id);
            IDirect3D9_GetDeviceCaps(d3d, 0, D3DDEVTYPE_HAL, &caps);
            IDirect3D9_Release(d3d);
            InterlockedIncrement(&setup_count);
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    int seconds = argc > 1 ? atoi(argv[1]) : 15;
    const char *path = argc > 2 ? argv[2] : "C:\\probe.log";
    FILE *log = fopen(path, "w");
    if (!log) return 2;
    setvbuf(log, NULL, _IONBF, 0);
    const BYTE *gate = (const BYTE *)(ULONG_PTR)__readfsdword(0xc0);
    fprintf(log, "pid=%lu gate=%p", GetCurrentProcessId(), gate);
    if (gate) {
        fprintf(log, " bytes=");
        for (unsigned i = 0; i < 12; ++i) fprintf(log, "%02x", gate[i]);
    }
    fputc('\n', log);
    PVOID handler = AddVectoredExceptionHandler(1, handle_probe_exception);
    if (!handler) return 4;
    HANDLE thread = CreateThread(NULL, 0, adapter_worker, NULL, 0, NULL);
    if (!thread) return 3;
    DWORD start = GetTickCount(), report = start;
    while (GetTickCount() - start < (DWORD)seconds * 1000) {
        RaiseException(0xe0420001, 0, 0, NULL);
        Sleep(1);
        if (GetTickCount() - report >= 1000) {
            report = GetTickCount();
            fprintf(log, "second=%lu adapters=%ld exceptions=%ld\n",
                (report-start)/1000, InterlockedCompareExchange(&setup_count, 0, 0), handled_exceptions);
        }
    }
    RemoveVectoredExceptionHandler(handler);
    InterlockedExchange(&stop_worker, 1);
    DWORD result = WaitForSingleObject(thread, 5000);
    if (result != WAIT_OBJECT_0 || !setup_count) {
        fprintf(log, "FAIL worker_wait=%lu adapters=%ld\n", result, setup_count);
        return 1;
    }
    fprintf(log, "PASS adapters=%ld\n", setup_count);
    CloseHandle(thread);
    fclose(log);
    return 0;
}
