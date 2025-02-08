#include <windows.h>
#include <stdio.h>

#define IOCTL_GPD_SET_BAUD_RATE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_GET_DATA_FROM_PORT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS)

void log_error(const char* action) {
    DWORD error = GetLastError();
    printf("%s failed. Error code: %lu\n", action, error);
    if (error == ERROR_INVALID_FUNCTION) {
        printf("Possible issue with handler implementation in the driver.\n");
    }
}

int main() {
    HANDLE hDevice;
    DWORD bytesReturned;
    char readBuffer[256];  // Buffer to read data
    ULONG baudRate = 9600; // Baud rate to set

    // Open the device (this will communicate with the driver)
    hDevice = CreateFile(
        L"\\\\.\\GpdDev",            // Symbolic link to the driver
        GENERIC_READ | GENERIC_WRITE, // Both read and write access
        0,                            // No sharing
        NULL,                         // Default security attributes
        OPEN_EXISTING,                // Open the existing device
        FILE_ATTRIBUTE_NORMAL,        // Normal file attributes
        NULL                          // No template file
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        log_error("Opening device");
        return 1;
    }

    printf("Device opened successfully.\n");

    // Set the baud rate via IOCTL
    if (!DeviceIoControl(
        hDevice,                     // Device handle
        IOCTL_GPD_SET_BAUD_RATE,     // IOCTL code to set the baud rate
        &baudRate,                   // Input buffer (baud rate)
        sizeof(baudRate),            // Size of input buffer
        NULL,                        // No output buffer
        0,                           // No output
        &bytesReturned,              // Bytes returned
        NULL                         // Synchronous call
    )) {
        log_error("Setting baud rate");
        CloseHandle(hDevice);
        return 1;
    }

    printf("Baud rate set: %lu baud\n", baudRate);

    printf("Attempting to read data from the device...\n");
    if (!DeviceIoControl(hDevice, IOCTL_GET_DATA_FROM_PORT, NULL, 0, readBuffer, sizeof(readBuffer), &bytesReturned, NULL)) {
        log_error("Reading data from port");
    } else {
       if (bytesReturned > 0) {
            printf("Received %lu bytes from driver: ", bytesReturned);
            for (DWORD i = 0; i < bytesReturned; i++) {
                printf("%02X ", (unsigned char)readBuffer[i]);
            }
            printf("\n");
        } else {
            printf("No data received from driver.\n");
        }
    }

    // Close the device handle
    CloseHandle(hDevice);
    printf("Device handle closed.\n");

    return 0;
}
