#include <windows.h>
#include <stdio.h>

#define IOCTL_ECHO CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

int main() {
    HANDLE hDevice = CreateFileA("\\\\.\\EchoDriverLink", GENERIC_WRITE | GENERIC_READ, 0,
        NULL, OPEN_EXISTING, 0, NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[!] Failed to open handle to driver: %lu\n", GetLastError());
        return 1;
    }

    char input[] = "Hello from userland!";
    DWORD returned;

    BOOL result = DeviceIoControl(hDevice, IOCTL_ECHO,
        input, sizeof(input),
        NULL, 0, &returned, NULL);

    if (result)
        printf("[+] Sent %lu bytes to driver successfully.\n", returned);
    else
        printf("[!] DeviceIoControl failed: %lu\n", GetLastError());

    CloseHandle(hDevice);
    return 0;
}