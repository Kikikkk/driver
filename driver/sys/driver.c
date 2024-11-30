#include "driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, PortIOEvtDeviceAdd)
#endif

VOID EvtIoWrite(
    __in WDFQUEUE Queue,
    __in WDFREQUEST Request,
    __in size_t Length
);

NTSTATUS ConfigureSerialPort(USHORT PortBase);

NTSTATUS PortIOEvtDeviceAdd(
    __in WDFDRIVER       Driver,
    __inout PWDFDEVICE_INIT DeviceInit
);

BOOLEAN IsTransmitterReady();

NTSTATUS WaitForTransmitterReady();

NTSTATUS
DriverEntry(
    __in PDRIVER_OBJECT  DriverObject,
    __in PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    WDF_DRIVER_CONFIG_INIT(&config,
                        PortIOEvtDeviceAdd
                        );

    status = WdfDriverCreate(DriverObject,
                            RegistryPath,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &config,
                            WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Error: WdfDriverCreate failed 0x%x\n", status);
        return status;
    }

    DbgPrint("Driver loaded successfully.\n");
    return STATUS_SUCCESS;
}

#define COM2_PORT_BASE_ADDRESS 0x2F8  // COM2 base address

NTSTATUS PortIOEvtDeviceAdd(
    __in WDFDRIVER       Driver,
    __inout PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDFDEVICE device;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_OBJECT_ATTRIBUTES attributes;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    DbgPrint("Enter PortIoDeviceAdd\n");

    // Create the device object.
    status = WdfDeviceCreate(
        &DeviceInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &device
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to create device object 0x%x\n", status);
        return status;
    }

    // Create a default queue for the device
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoWrite = EvtIoWrite; // Set the write handler

    status = WdfIoQueueCreate(
        device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        NULL
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to create I/O queue 0x%x\n", status);
        return status;
    }

    // Now we configure the serial port parameters (COM2)
    status = ConfigureSerialPort(COM2_PORT_BASE_ADDRESS);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to configure COM2 0x%x\n", status);
        return status;
    }

    DbgPrint("Device successfully added.\n");
    return STATUS_SUCCESS;
}

VOID EvtIoWrite(
    __in WDFQUEUE Queue,
    __in WDFREQUEST Request,
    __in size_t Length
)
{
    PVOID buffer;
    NTSTATUS status;
    size_t bytesWritten = 0;
    
    DbgPrint("EvtIoWrite: Start writing to port\n");

    // Retrieve output buffer from the request
    status = WdfRequestRetrieveOutputBuffer(Request, Length, &buffer, NULL);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to retrieve buffer 0x%x\n", status);
        WdfRequestComplete(Request, status);
        return;
    }

    // Start sending bytes one by one
    for (bytesWritten = 0; bytesWritten < Length; bytesWritten++) {
        // Wait for transmitter to be ready
        status = WaitForTransmitterReady();
        if (!NT_SUCCESS(status)) {
            DbgPrint("Transmitter is not ready, retrying...\n");
            WdfRequestComplete(Request, status);
            return;
        }
        
        // Send byte to COM2
        WRITE_PORT_UCHAR(COM2_PORT_BASE_ADDRESS, ((PUCHAR)buffer)[bytesWritten]);  // Send byte to COM2
        DbgPrint("Written byte 0x%x to COM2\n", ((PUCHAR)buffer)[bytesWritten]);
    }

    // Complete the request after writing all the bytes
    DbgPrint("EvtIoWrite: Completed writing to port\n");
    WdfRequestComplete(Request, STATUS_SUCCESS);
}

NTSTATUS WaitForTransmitterReady()
{
    LARGE_INTEGER delayTime;
    NTSTATUS status;

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

NTSTATUS ConfigureSerialPort(USHORT PortBase)
{
    UCHAR divisor;
    NTSTATUS status;

    DbgPrint("Configuring serial port at base address 0x%x...\n", PortBase);

    // Reset the COM port (disable interrupts)
    WRITE_PORT_UCHAR(PortBase + 4, 0x00);  // Disable interrupts (IER)

    // Set the baud rate - Divisor Latch Access Bit (DLAB = 1)
    WRITE_PORT_UCHAR(PortBase + 3, 0x80);  // Enable DLAB
    divisor = 3; // Example divisor for 9600 baud rate (assuming 115200 baud rate divisor for simplicity)
    WRITE_PORT_UCHAR(PortBase, divisor);   // Set LSB of divisor
    WRITE_PORT_UCHAR(PortBase + 1, divisor); // Set MSB of divisor

    // Set 8 data bits, no parity, 1 stop bit
    WRITE_PORT_UCHAR(PortBase + 3, 0x03);  // 8 data bits, no parity, 1 stop bit (also disables DLAB)

    // Enable FIFO (first-in, first-out) for serial port
    WRITE_PORT_UCHAR(PortBase + 2, 0xC7);  // Enable FIFO, clear them, 14-byte threshold

    // Enable modem control (optional, for handshake signals)
    WRITE_PORT_UCHAR(PortBase + 4, 0x0B);  // Set RTS/DSR for flow control

    // Enable the port (assuming the register map supports it)
    WRITE_PORT_UCHAR(PortBase + 4, 0x0F);  // Enable both receiver and transmitter

    DbgPrint("COM2 port configured successfully.\n");

    return STATUS_SUCCESS;
}

BOOLEAN IsTransmitterReady()
{
    UCHAR lsr;
    
    // Read the Line Status Register (LSR) from COM2 (PortBase + 5)
    lsr = READ_PORT_UCHAR(COM2_PORT_BASE_ADDRESS + 5);  // LSR is at offset 5

    // Check if the Transmitter Holding Register Empty (THRE) bit is set (bit 5)
    DbgPrint("LSR Register: 0x%x\n", lsr);
    return (lsr & 0x20) != 0;
}
