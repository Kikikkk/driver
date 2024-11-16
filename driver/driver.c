#define _WIN32_WINNT 0x0501

#include <ntddk.h>
#include <ntddser.h>

#define SERIAL_DEVICE_NAME L"\\DosDevices\\COM2"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);
VOID PollSerialPort(PDEVICE_OBJECT DeviceObject);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNICODE_STRING serialDeviceName;
    PFILE_OBJECT fileObject = NULL;
    PDEVICE_OBJECT deviceObject = NULL;
    NTSTATUS status;

    // Initialize serial device name
    RtlInitUnicodeString(&serialDeviceName, SERIAL_DEVICE_NAME);

    // Attempt to get the device object pointer
    status = IoGetDeviceObjectPointer(
        &serialDeviceName,
        FILE_READ_DATA | FILE_WRITE_DATA,
        &fileObject,
        &deviceObject
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to obtain serial port. Status: 0x%x\n", status);
        return status;
    }

    DbgPrint("Successfully obtained the device object for %wZ.\n", &serialDeviceName);

    // Poll the serial port
    PollSerialPort(deviceObject);

    // Release the file object reference
    ObDereferenceObject(fileObject);

    // Set up the unload routine
    DriverObject->DriverUnload = DriverUnload;
    DbgPrint("Serial Driver Loaded.\n");

    return STATUS_SUCCESS;
}

VOID PollSerialPort(PDEVICE_OBJECT DeviceObject) {
    IO_STATUS_BLOCK ioStatus;
    KEVENT event;
    SERIAL_STATUS serialStatus;
    PIRP irp;
    NTSTATUS status;
    LARGE_INTEGER delay;
    delay.QuadPart = -10000000LL; // 1-second delay in relative time

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    while (TRUE) {
        RtlZeroMemory(&serialStatus, sizeof(SERIAL_STATUS));

        // Build the IOCTL request to query serial port status
        irp = IoBuildDeviceIoControlRequest(
            IOCTL_SERIAL_GET_COMMSTATUS,
            DeviceObject,
            NULL,
            0,
            &serialStatus,
            sizeof(SERIAL_STATUS),
            FALSE,
            &event,
            &ioStatus
        );

        if (irp == NULL) {
            DbgPrint("Failed to build IOCTL request for polling.\n");
            break;
        }

        status = IoCallDriver(DeviceObject, irp);
        if (!NT_SUCCESS(status)) {
            DbgPrint("Polling failed. Status: 0x%x\n", status);
            break;
        }

        DbgPrint("Polling Status: HoldReasons=0x%x\n", serialStatus.HoldReasons);

        // Wait for 1 second before the next poll
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrint("Serial Driver Unloaded.\n");
}
