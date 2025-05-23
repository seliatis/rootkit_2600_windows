#include <ntddk.h>
#include <windef.h>
#include <ntimage.h>


VOID ProcessNotifyRoutine(HANDLE ParentId, HANDLE ProcessId, BOOLEAN Create) {
    if (Create) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Process created] PID=%u (Parent PID=%u)\n", (ULONG)(ULONG_PTR)ProcessId, (ULONG)(ULONG_PTR)ParentId);
    }
    else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Process exited] PID=%u\n", (ULONG)(ULONG_PTR)ProcessId);
    }
}

VOID ThreadNotifyRoutine(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create) {
    if (Create) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Thread created] TID=%u (PID=%u)\n", (ULONG)(ULONG_PTR)ThreadId, (ULONG)(ULONG_PTR)ProcessId);
    }
    else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Thread exited] TID=%u (PID=%u)\n", (ULONG)(ULONG_PTR)ThreadId, (ULONG)(ULONG_PTR)ProcessId);
    }
}

VOID ImageLoadCallback(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo) {
    UNREFERENCED_PARAMETER(ImageInfo);
    if (FullImageName && FullImageName->Buffer) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Image loaded] PID=%u: %wZ\n", (ULONG)(ULONG_PTR)ProcessId, FullImageName);
    }
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);

    PsSetCreateProcessNotifyRoutine(ProcessNotifyRoutine, TRUE); 
    PsRemoveCreateThreadNotifyRoutine(ThreadNotifyRoutine);      
    PsRemoveLoadImageNotifyRoutine(ImageLoadCallback);           

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Driver unloaded]\n");
}

// Entry
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    DriverObject->DriverUnload = DriverUnload;

    PsSetCreateProcessNotifyRoutine(ProcessNotifyRoutine, FALSE);
    PsSetCreateThreadNotifyRoutine(ThreadNotifyRoutine); 
    PsSetLoadImageNotifyRoutine(ImageLoadCallback);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Driver loaded]\n");

    return STATUS_SUCCESS;
}
