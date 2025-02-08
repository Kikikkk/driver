#include <ntddk.h>
#include <wdf.h>

#define IOCTL_GPD_SET_BAUD_RATE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_GET_DATA_FROM_PORT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS) 
#define COM2_PORT_BASE_ADDRESS 0x2F8 // Базовый адрес COM2
#define MAX_BUFFER_SIZE 256

typedef struct _DEVICE_EXTENSION {
    ULONG BaudRate; 
    ULONG DataLength;
    CHAR DataBuffer[MAX_BUFFER_SIZE];
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

NTSTATUS ConfigureSerialPort(ULONG baudRate) {
    DbgPrint("Entering ConfigureSerialPort with baudRate = %lu\n", baudRate);
    if (baudRate == 0) {
        DbgPrint(("ConfigureSerialPort: Invalid baud rate: %lu\n", baudRate));
        return STATUS_INVALID_PARAMETER;
    }
    WRITE_PORT_UCHAR((PUCHAR)(COM2_PORT_BASE_ADDRESS + 4), 0x00);  // Disable interrupts (IER)
    WRITE_PORT_UCHAR((PUCHAR)(COM2_PORT_BASE_ADDRESS + 3), 0x80); // Включаем DLAB
    WRITE_PORT_UCHAR((PUCHAR)COM2_PORT_BASE_ADDRESS, (UCHAR)(baudRate & 0xFF));   // Младший байт делителя
    WRITE_PORT_UCHAR((PUCHAR)(COM2_PORT_BASE_ADDRESS + 1), (UCHAR)((baudRate >> 8) & 0xFF)); // Старший байт делителя
    WRITE_PORT_UCHAR((PUCHAR)(COM2_PORT_BASE_ADDRESS + 3), 0x03); // 8 бит данных, 1 стоп-бит, без проверки четности
    WRITE_PORT_UCHAR((PUCHAR)(COM2_PORT_BASE_ADDRESS + 2), 0xC7);  // Enable FIFO, clear them, 14-byte threshold
    WRITE_PORT_UCHAR((PUCHAR)(COM2_PORT_BASE_ADDRESS + 4), 0x0F);  // Enable both receiver and transmitter

    DbgPrint(("COM2 port configured successfully with baud rate %lu baud\n", baudRate));
    return STATUS_SUCCESS;
}

BOOLEAN IsTransmitterReady() {
    UCHAR lsr;
    DbgPrint("Entering IsTransmitterReady\n");

    // Read the Line Status Register (LSR) from COM2 (PortBase + 5)
    lsr = READ_PORT_UCHAR(COM2_PORT_BASE_ADDRESS + 5);  // LSR is at offset 5
    DbgPrint("LSR Register: 0x%x\n", lsr);

    return (lsr & 0x20) != 0;
}

// Wait for the transmitter to be ready to send data
NTSTATUS WaitForTransmitterReady() {
    LARGE_INTEGER delayTime;
    NTSTATUS status;

    DbgPrint("Entering WaitForTransmitterReady\n");

    // Set a timeout for checking (e.g., 100 milliseconds)
    delayTime.QuadPart = -10000 * 100;  // 100ms in 100-nanosecond units

    DbgPrint("Waiting for transmitter to be ready...\n");

    while (!IsTransmitterReady()) {
        // Wait for the timeout period before checking again
        KeDelayExecutionThread(KernelMode, FALSE, &delayTime);
    }

    DbgPrint("Transmitter is ready.\n");
    return STATUS_SUCCESS;
}

NTSTATUS CustomIoRead(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    PUCHAR buffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
    ULONG bytesToRead = stack->Parameters.Read.Length;
    ULONG bytesRead = 0;
    NTSTATUS status = STATUS_SUCCESS;

    DbgPrint("Entering CustomIoRead with bytesToRead = %lu\n", bytesToRead);

    if (!buffer || bytesToRead == 0) {
        DbgPrint("CustomIoRead: Invalid buffer or bytesToRead is zero\n");
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    for (bytesRead = 0; bytesRead < bytesToRead; bytesRead++) {
        // Проверяем готовность к чтению данных
        status = WaitForTransmitterReady();
        if (!NT_SUCCESS(status)) {
            DbgPrint("CustomIoRead: Transmitter not ready. Status: 0x%x\n", status);
            break;
        }

        // Читаем байт из порта с помощью READ_PORT_UCHAR
        buffer[bytesRead] = READ_PORT_UCHAR((PUCHAR)COM2_PORT_BASE_ADDRESS);
        DbgPrint("CustomIoRead: Read byte %lu: 0x%02X\n", bytesRead, buffer[bytesRead]);
    }

    DbgPrint("CustomIoRead: Successfully read %lu bytes\n", bytesRead);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = bytesRead;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DeviceIoControlHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION ioStack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG bytesToWrite;

    DbgPrint("Entering DeviceIoControlHandler\n");

    switch (ioStack->Parameters.DeviceIoControl.IoControlCode) {
        case IOCTL_GET_DATA_FROM_PORT:
            DbgPrint("DeviceIoControlHandler: IOCTL_GET_DATA_FROM_PORT\n");

            // Чтение данных из пользовательского пространства
            if (ioStack->Parameters.DeviceIoControl.InputBufferLength > 0 && Irp->AssociatedIrp.SystemBuffer) {
                bytesToWrite = min(ioStack->Parameters.DeviceIoControl.InputBufferLength, MAX_BUFFER_SIZE);
                RtlCopyMemory(deviceExtension->DataBuffer, Irp->AssociatedIrp.SystemBuffer, bytesToWrite);
                deviceExtension->DataLength = bytesToWrite;
                DbgPrint("DeviceIoControlHandler: Received %lu bytes: %s\n", bytesToWrite, deviceExtension->DataBuffer);
            }

            // Передача данных в CustomIoRead
            status = CustomIoRead(DeviceObject, Irp);
            break;

        case IOCTL_GPD_SET_BAUD_RATE:
            DbgPrint("DeviceIoControlHandler: IOCTL_GPD_SET_BAUD_RATE\n");
            if (ioStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(ULONG)) {
                ULONG* baudRate = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
                DbgPrint("Setting baud rate to %lu\n", *baudRate);
                status = ConfigureSerialPort(*baudRate);
                if (NT_SUCCESS(status)) {
                    deviceExtension->BaudRate = *baudRate;
                }
            } else {
                DbgPrint("DeviceIoControlHandler: Invalid InputBufferLength\n");
                status = STATUS_INVALID_PARAMETER;
            }
            Irp->IoStatus.Status = status;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return status;

        default:
            DbgPrint("DeviceIoControlHandler: Unsupported IOCTL\n");
            Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_INVALID_DEVICE_REQUEST;
    }

    return status;
}


NTSTATUS CreateCloseHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
) {
    DbgPrint("CreateCloseHandler: IRP processed\n");
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// DriverEntry
NTSTATUS DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
) {
    NTSTATUS status;
    WDFDEVICE device;
    UNICODE_STRING deviceName;
    UNICODE_STRING symbolicLinkName;
    PDEVICE_EXTENSION deviceExtension;

    DbgPrint("Entering DriverEntry\n");

    RtlInitUnicodeString(&deviceName, L"\\Device\\GpdDev");
    RtlInitUnicodeString(&symbolicLinkName, L"\\DosDevices\\GpdDev");

    status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &device
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint(("DriverEntry: Failed to create device\n"));
        return status;
    }

    deviceExtension = (PDEVICE_EXTENSION)DriverObject->DeviceObject->DeviceExtension;
    deviceExtension->DataLength = 0;

    DriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCloseHandler;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateCloseHandler;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControlHandler;

    status = IoCreateSymbolicLink(&symbolicLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(device);
        DbgPrint(("DriverEntry: Failed to create symbolic link\n"));
        return status;
    }

    DbgPrint(("DriverEntry: Driver loaded successfully\n"));
    return STATUS_SUCCESS;
}
