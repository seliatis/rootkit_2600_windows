#include <ntddk.h>

#define IOCTL_ECHO CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\EchoDriver");
UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\EchoDriverLink");

NTSTATUS EchoDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR info = 0;

    switch (stack->MajorFunction) {
    case IRP_MJ_CREATE:
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Handle opened]\n");
        break;
    case IRP_MJ_CLOSE:
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Handle closed]\n");
        break;
    case IRP_MJ_DEVICE_CONTROL:
        if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_ECHO) {
            ULONG inputLen = stack->Parameters.DeviceIoControl.InputBufferLength;
            PVOID buffer = Irp->AssociatedIrp.SystemBuffer;

            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Received IOCTL_ECHO with %lu bytes: \"%.*s\"\n",
                inputLen, inputLen, (char*)buffer);

            info = inputLen;
        }
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

VOID EchoUnload(PDRIVER_OBJECT DriverObject) {
    IoDeleteSymbolicLink(&symLink);
    IoDeleteDevice(DriverObject->DeviceObject);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Driver unloaded]\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    PDEVICE_OBJECT devObj = NULL;
    NTSTATUS status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &devObj
    );

    if (!NT_SUCCESS(status)) return status;

    IoCreateSymbolicLink(&symLink, &deviceName);

    for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
        DriverObject->MajorFunction[i] = EchoDispatch;

    DriverObject->DriverUnload = EchoUnload;

    return STATUS_SUCCESS;
}