# server.py
import socket
import time
import struct

# --- Configuration ---
# The IP address the server will listen on. 
# '0.0.0.0' makes it listen on all available network interfaces,
# which is exactly what we need for a multi-homed server.
HOST = '0.0.0.0' 
# The port to listen on. Must match the client's port.
PORT = 12345
# The maximum size of a single UDP datagram.
BUFFER_SIZE = 65535 
# A special message from the client to signal the end of the test.
END_MESSAGE_FLAG = b'__END__'

def run_server():
    """
    Initializes and runs the UDP server. It is inherently ready to handle
    traffic sent to any of its IP addresses because it listens on 0.0.0.0.
    """
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.bind((HOST, PORT))
            print(f"[*] Multi-Homed Server listening for data on {HOST}:{PORT}")

            total_data_received = 0
            start_time = None
            received_packets = set()
            total_packets_sent_by_client = 0
            client_address = None

            while True:
                try:
                    s.settimeout(20.0) # 20 second timeout
                    data, addr = s.recvfrom(BUFFER_SIZE)

                    if client_address is None:
                        client_address = addr
                        print(f"[*] Connection initiated from: {client_address}")

                    if start_time is None:
                        start_time = time.monotonic()

                    if data.startswith(END_MESSAGE_FLAG):
                        payload = data[len(END_MESSAGE_FLAG):]
                        if len(payload) == 8:
                            total_packets_sent_by_client = struct.unpack('!Q', payload)[0]
                            print(f"[*] End signal received. Client reports {total_packets_sent_by_client} packets sent.")
                            break
                    else:
                        if len(data) >= 8:
                            sequence_number = struct.unpack('!Q', data[:8])[0]
                            received_packets.add(sequence_number)
                            total_data_received += len(data)

                except socket.timeout:
                    print("[!] Server timed out waiting for data. Finalizing test.")
                    break
    
    except OSError as e:
        print(f"[!] Error: Could not bind to port {PORT}. It might be in use. Details: {e}")
        return

    # --- Calculation and Results ---
    if start_time is None:
        print("[!] No data was received. Cannot calculate speed.")
        return
        
    end_time = time.monotonic()
    duration = end_time - start_time
    if duration == 0: duration = 1e-9

    packets_received = len(received_packets)
    packets_lost = 0
    loss_percentage = 0.0
    
    if total_packets_sent_by_client > 0:
        packets_lost = total_packets_sent_by_client - packets_received
        if packets_lost < 0: packets_lost = 0 
        loss_percentage = (packets_lost / total_packets_sent_by_client) * 100
    else:
        print("[!] Did not receive final packet count. Cannot calculate packet loss accurately.")

    total_megabytes = total_data_received / (1024 * 1024)
    bytes_per_second = total_data_received / duration
    megabits_per_second = (bytes_per_second * 8) / (1000 * 1000)

    print("\n--- Aggregate UDP Ingress Test Results ---")
    print(f"Test Duration:       {duration:.2f} seconds")
    print(f"Total Data Received: {total_megabytes:.2f} MB")
    print(f"Aggregate Throughput:{megabits_per_second:.2f} Mbps")
    print("--- Packet Loss (Aggregate) ---")
    print(f"Packets Sent (by client): {total_packets_sent_by_client}")
    print(f"Packets Received:         {packets_received}")
    print(f"Packets Lost:             {packets_lost}")
    print(f"Packet Loss:              {loss_percentage:.2f}%")
    print("------------------------------------")


if __name__ == '__main__':
    run_server()
