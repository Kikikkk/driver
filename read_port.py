import win32pipe
import win32file
import time

# Define the pipe name
pipe_name = r'\\.\pipe\emulated_com2'

def read_from_pipe():
    # Open the pipe
    try:
        pipe = win32file.CreateFile(
            pipe_name,                # Pipe name
            win32file.GENERIC_READ,    # Read access
            0,                         # No sharing
            None,                      # Default security attributes
            win32file.OPEN_EXISTING,   # Open existing pipe
            0,                         # No flags
            None                       # No template file
        )
    except Exception as e:
        print(f"Failed to open pipe: {e}")
        return
    
    print(f"Connected to pipe: {pipe_name}")
    
    # Continuously read from the pipe in a while loop
    while True:
        try:
            # Read data from the pipe (set buffer size)
            _, data = win32file.ReadFile(pipe, 4096)
            
            # if hr == 0:
            #     break
            
            print(f"Received data: {data.decode('utf-8', errors='ignore')}")
            
        except Exception as e:
            print(f"Error reading from pipe: {e}")
            break
        
        time.sleep(1)  # Sleep for a short period to prevent high CPU usage

if __name__ == "__main__":
    read_from_pipe()
