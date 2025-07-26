# server.py
import socket
import time

# --- Configuration ---
# The IP address the server will listen on. 
# '0.0.0.0' means it will listen on all available network interfaces.
HOST = '0.0.0.0' 
# The port to listen on. Must match the client's port.
PORT = 12345
# The maximum size of a single UDP datagram. 
# 65507 is the theoretical max payload size for a UDP packet.
BUFFER_SIZE = 65535 
# A special message from the client to signal the end of the test.
END_MESSAGE = b'__END__'

def run_server():
    """
    Initializes and runs the UDP server to receive data and calculate speed.
    """
    try:
        # Create a UDP socket
        # AF_INET specifies IPv4.
        # SOCK_DGRAM specifies UDP.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            # Bind the socket to the specified host and port
            s.bind((HOST, PORT))
            print(f"[*] UDP Server listening on {HOST}:{PORT}")

            total_data_received = 0
            start_time = None
            client_address = None

            while True:
                # Wait for and receive data from a client
                try:
                    # Set a timeout to prevent the server from hanging indefinitely
                    # if the client stops sending without an END message.
                    s.settimeout(20.0) # 20 second timeout
                    data, addr = s.recvfrom(BUFFER_SIZE)

                    # Store client address on first contact
                    if client_address is None:
                        client_address = addr
                        print(f"[*] Connection initiated from: {client_address}")

                    # Start the timer on the first packet received
                    if start_time is None:
                        start_time = time.monotonic()

                    # Check if the client sent the end message
                    if data.endswith(END_MESSAGE):
                        # Remove the end message from the data count
                        total_data_received += len(data) - len(END_MESSAGE)
                        print("[*] End signal received. Finalizing test...")
                        break # Exit the loop to calculate results
                    else:
                        total_data_received += len(data)

                except socket.timeout:
                    print("[!] Server timed out waiting for data. Finalizing test.")
                    break # Exit loop if no data is received for a while
    
    except OSError as e:
        print(f"[!] Error: Could not bind to port {PORT}. It might be in use.")
        print(f"    Details: {e}")
        return

    # --- Calculation and Results ---
    if start_time is None:
        print("[!] No data was received. Cannot calculate speed.")
        return
        
    end_time = time.monotonic()
    duration = end_time - start_time

    # Avoid division by zero if the test was extremely short
    if duration == 0:
        duration = 1e-9 # A very small number to prevent error

    # Calculate speed
    total_megabytes = total_data_received / (1024 * 1024)
    bytes_per_second = total_data_received / duration
    megabits_per_second = (bytes_per_second * 8) / (1000 * 1000)

    # Print the final report
    print("\n--- UDP Speed Test Results ---")
    print(f"Total Data Received: {total_megabytes:.2f} MB")
    print(f"Test Duration:       {duration:.2f} seconds")
    print(f"Transfer Speed:      {megabits_per_second:.2f} Mbps")
    print("----------------------------")

if __name__ == '__main__':
    run_server()
