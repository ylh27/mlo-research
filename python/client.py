# client.py
import socket
import time
import os
import select

# --- Configuration ---
# The IP address of the server.
# Change this to the server's IP address if it's on another machine.
# '127.0.0.1' is the loopback address (localhost).
HOST = '192.168.1.1'
# The port the server is listening on. Must match the server's port.
PORT = 12345
# The size of each data packet to send.
PACKET_SIZE = 1470
# The duration of the speed test in seconds.
TEST_DURATION = 10
# A special message to signal the end of the test.
END_MESSAGE = b'__END__'

def run_client():
    """
    Initializes and runs the UDP client to send data to the server.
    """
    # Create a UDP socket
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        # Generate a packet of random data to send.
        # Using random data prevents potential compression on the network
        # from affecting the test results.
        data_packet = os.urandom(PACKET_SIZE)
        
        server_address = (HOST, PORT)
        print(f"[*] UDP Client starting test to {HOST}:{PORT}")
        print(f"[*] Sending data for {TEST_DURATION} seconds...")

        start_time = time.monotonic()
        packets_sent = 0
        
        try:
            # Main loop to send data for the specified duration
            while (time.monotonic() - start_time) < TEST_DURATION:
                readable, writable, exceptional = select.select([], [s], [], 0)
                if writable:
                    s.sendto(data_packet, server_address)
                    packets_sent += 1
                else:
                    print("[!] Socket not writable, skipping send.")
        except socket.error as e:
            print(f"[!] A socket error occurred: {e}")
            print("[!] Please ensure the server is running and the IP/port are correct.")
            return
            
        finally:
            # --- Signal End of Test ---
            # Send the end message multiple times to increase the chance
            # it gets through, since UDP is an unreliable protocol.
            print("[*] Test duration finished. Sending end signal.")
            for _ in range(10):
                s.sendto(END_MESSAGE, server_address)
                time.sleep(0.01) # Small delay between end signals

    total_data_sent = packets_sent * PACKET_SIZE
    total_megabytes_sent = total_data_sent / (1024 * 1024)
    print("\n--- Client Summary ---")
    print(f"Packets Sent: {packets_sent}")
    print(f"Total Data Sent: {total_megabytes_sent:.2f} MB")
    print("----------------------")


if __name__ == '__main__':
    run_client()
