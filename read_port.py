import serial
from serial import Serial
import time

# Configure the serial port
ser = Serial(
    port='\\\\.\\pipe\\emulated_com2',
    baudrate=115200,  # Note: Adjusted to a more common baud rate for COM ports
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
    timeout=1  # 1 second timeout, adjust as needed
)

print("Connected to: " + ser.portstr)

count = 1

try:
    while True:
        # Read bytes (you can adjust the size of the read depending on your use case)
        data = ser.read(1024)  # Reads up to 1024 bytes at once, adjust as needed
        if data:
            # Process the received data
            for byte in data:
                # Ensure the byte is a printable character (or handle non-printable as needed)
                if 32 <= byte <= 126:  # ASCII printable character range
                    print(f"{count}: {chr(byte)}")
                else:
                    print(f"{count}: Non-printable byte 0x{byte:02X}")

                count += 1
        else:
            print("No data received in the last second...")
        time.sleep(0.1)  # Small delay to avoid high CPU usage

except KeyboardInterrupt:
    print("Serial reading interrupted. Closing the connection.")
finally:
    # Always close the port when done
    ser.close()
    print("Connection closed.")
