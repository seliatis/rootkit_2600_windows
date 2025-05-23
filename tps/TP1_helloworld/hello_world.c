#include <ntddk.h>

void DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrint("[HelloWorld] Unloaded\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    UNREFERENCED_PARAMETER(DriverObject);

    DbgPrint("[HelloWorld] Loaded\n");

    DriverObject->DriverUnload = DriverUnload;

    return STATUS_SUCCESS;
}
