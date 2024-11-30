typedef struct IUnknown IUnknown;

#include <windows.h>
#include <stdio.h>

#define IOCTL_SEND_DATA_TO_PORT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_ACCESS)

int main() {
    HANDLE hDevice;
    DWORD bytesReturned;
    char buffer[256]; // Buffer for data to send to the driver

    // Open the device (this will communicate with the driver)
    hDevice = CreateFile(
        L"\\\\.\\IoctlDevice",        // The symbolic link to the driver
        GENERIC_READ | GENERIC_WRITE, // We need both read and write access
        0,                            // No sharing
        NULL,                         // Default security attributes
        OPEN_EXISTING,                // Open the existing device
        FILE_ATTRIBUTE_NORMAL,        // Normal file attributes
        NULL                          // No template file
    );

    // Check if the device was opened successfully
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("Failed to open device. Error: %lu\n", GetLastError());
        return 1;
    }

    // Prepare data to send to the serial port
    snprintf(buffer, sizeof(buffer), "Hello, Serial Port!\n");

    // Send data via IOCTL to the driver
    if (!DeviceIoControl(
        hDevice,                    // Device handle
        IOCTL_SEND_DATA_TO_PORT,    // IOCTL code
        buffer,                     // Input buffer (data to send)
        strlen(buffer) + 1,         // Size of the data (including null terminator)
        NULL,                       // No output buffer
        0,                          // No output
        &bytesReturned,             // Bytes returned
        NULL                        // No overlapped structure
    )) {
        printf("DeviceIoControl failed. Error: %lu\n", GetLastError());
        CloseHandle(hDevice);
        return 1;
    }

    printf("Data sent to driver: %s\n", buffer);

    // Close the device handle
    CloseHandle(hDevice);

    return 0;
}
