#include <wdm.h>  // For WDM drivers

#define DEVICE_NAME        L"\\Device\\MyCustomPort"
#define SYMBOLIC_LINK_NAME L"\\DosDevices\\MyCustomPort"
#define COM2_PORT_BASE_ADDRESS 0x2F8  // COM2 base address

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
NTSTATUS DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS ConfigureSerialPort(USHORT PortBase);
VOID EvtIoWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNICODE_STRING deviceName;
    UNICODE_STRING symbolicLinkName;
    PDEVICE_OBJECT deviceObject;
    NTSTATUS status;

    // Initialize the device and symbolic link names
    RtlInitUnicodeString(&deviceName, DEVICE_NAME);
    RtlInitUnicodeString(&symbolicLinkName, SYMBOLIC_LINK_NAME);

    // Create the device object
    status = IoCreateDevice(
        DriverObject,                  // Driver object
        0,                             // Device extension size (no extra data for now)
        &deviceName,                   // Device name
        FILE_DEVICE_UNKNOWN,           // Device type (serial port)
        0,                              // Device characteristics
        FALSE,                          // Not exclusive
        &deviceObject                  // Device object to be created
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to create device object 0x%x\n", status);
        return status;
    }

    // Create the symbolic link
    IoDeleteSymbolicLink(&symbolicLinkName);
    status = IoCreateSymbolicLink(&symbolicLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to create symbolic link 0x%x\n", status);
        IoDeleteDevice(deviceObject);  // Clean up the device object
        return status;
    }

    // Set up the driver unload routine
    DriverObject->DriverUnload = DriverUnload;

    DbgPrint("Driver loaded successfully.\n");

    return STATUS_SUCCESS;
}

NTSTATUS DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING symbolicLinkName;

    // Initialize symbolic link name
    RtlInitUnicodeString(&symbolicLinkName, SYMBOLIC_LINK_NAME);

    // Delete symbolic link
    IoDeleteSymbolicLink(&symbolicLinkName);

    // Delete device object
    IoDeleteDevice(DriverObject->DeviceObject);

    DbgPrint("Driver unloaded successfully.\n");

    return STATUS_SUCCESS;
}

NTSTATUS ConfigureSerialPort(USHORT PortBase)
{
    UCHAR divisor;
    NTSTATUS status;

    // Reset the COM port (disable interrupts)
    WRITE_PORT_UCHAR(PortBase + 4, 0x00);  // Disable interrupts (IER)

    // Set the baud rate - Divisor Latch Access Bit (DLAB = 1)
    WRITE_PORT_UCHAR(PortBase + 3, 0x80);  // Enable DLAB
    divisor = 3; // Example divisor for 9600 baud rate
    WRITE_PORT_UCHAR(PortBase, divisor);   // Set LSB of divisor
    WRITE_PORT_UCHAR(PortBase + 1, divisor); // Set MSB of divisor

    // Set 8 data bits, no parity, 1 stop bit
    WRITE_PORT_UCHAR(PortBase + 3, 0x03);  // 8 data bits, no parity, 1 stop bit

    // Enable FIFO for serial port
    WRITE_PORT_UCHAR(PortBase + 2, 0xC7);  // Enable FIFO, clear them, 14-byte threshold

    // Enable modem control
    WRITE_PORT_UCHAR(PortBase + 4, 0x0F);  // Enable both receiver and transmitter

    DbgPrint("COM2 port configured successfully.\n");

    return STATUS_SUCCESS;
}

VOID EvtIoWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION irpStack;
    PVOID buffer;
    NTSTATUS status;
    PUCHAR pBuffer;
    int length;
    int i;

    // Get the IRP stack location and output buffer
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    length = irpStack->Parameters.Write.Length;
    buffer = Irp->AssociatedIrp.SystemBuffer;
    pBuffer = (PUCHAR)buffer;

    for (i = 0; i < length; i++) {
        WRITE_PORT_UCHAR(COM2_PORT_BASE_ADDRESS, pBuffer[i]);
        DbgPrint("Written byte 0x%x to COM2\n", pBuffer[i]);
    }

    // Complete the IRP
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = length;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}
