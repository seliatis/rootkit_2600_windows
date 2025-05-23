#include <fltKernel.h>

PFLT_FILTER gFilterHandle;

FLT_PREOP_CALLBACK_STATUS
PreCreateCallback(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID *CompletionContext
) {
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(CompletionContext);

    if (FltObjects && FltObjects->FileObject) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "IRP_MJ_CREATE: %wZ\n", &FltObjects->FileObject->FileName);
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "IRP_MJ_CREATE: (Objet fichier non disponible)\n");
    }


    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS
PreReadCallback(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID *CompletionContext
) {
    UNREFERENCED_PARAMETER(CompletionContext);

    ULONG len = Data->Iopb->Parameters.Read.Length;
    
    if (FltObjects && FltObjects->FileObject) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "IRP_MJ_READ (%lu octets): %wZ\n", len, &FltObjects->FileObject->FileName);
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "IRP_MJ_READ (%lu octets): (Objet fichier non disponible)\n", len);
    }

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS
PreWriteCallback(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID *CompletionContext
) {
    UNREFERENCED_PARAMETER(CompletionContext);

    ULONG len = Data->Iopb->Parameters.Write.Length;
    if (FltObjects && FltObjects->FileObject) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "IRP_MJ_WRITE (%lu octets): %wZ\n", len, &FltObjects->FileObject->FileName);
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "IRP_MJ_WRITE (%lu octets): (Objet fichier non disponible)\n", len);
    }

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_CREATE, 0, PreCreateCallback, NULL },
    { IRP_MJ_READ,   0, PreReadCallback,   NULL },
    { IRP_MJ_WRITE,  0, PreWriteCallback,  NULL },
    { IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),
    FLT_REGISTRATION_VERSION,
    0,
    NULL,
    Callbacks,
    NULL, 
    NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
) {
    UNREFERENCED_PARAMETER(RegistryPath);
    
    NTSTATUS status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "Échec de FltRegisterFilter: 0x%x\n", status); 
        return status;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Filtre enregistré !\n"); 
    status = FltStartFiltering(gFilterHandle);

    if (!NT_SUCCESS(status)) {
        FltUnregisterFilter(gFilterHandle);
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "Échec de FltStartFiltering: 0x%x\n", status); 
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Filtrage démarré.\n");
    }

    return status;
}