#!/usr/bin/env python3
# client.py
import socket
import time
import os
import struct
import threading
import sys
import queue

# --- Configuration ---
# You must edit this list to include the IP addresses of the
# multi-homed server you are sending data TO.
SERVER_IPS = ['127.0.0.1'] # Replace with your server's actual IP addresses

# The port the server is listening on.
PORT = 12345
# The size of the random data payload in each packet.
PACKET_SIZE = 8000
# The duration of the speed test in seconds.
TEST_DURATION = 10
# A special message to signal the end of the test.
END_MESSAGE_FLAG = b'__END__'

# A thread-safe queue to hold packets from the master to the workers.
packet_queue = queue.Queue(maxsize=1024)

class ThreadWithReturnValue(threading.Thread):
    """
    A custom thread class that allows retrieving a return value from the target function.
    """
    def __init__(self, group=None, target=None, name=None,
                 args=(), kwargs={}, Verbose=None):
        threading.Thread.__init__(self, group, target, name, args, kwargs)
        self._return = None

    def run(self):
        if self._target is not None:
            self._return = self._target(*self._args, **self._kwargs)

    def join(self, *args):
        threading.Thread.join(self, *args)
        return self._return

def master_packet_producer():
    """
    This is the "producer" thread. It creates all the packets
    and puts them into a shared queue for the worker threads to send.
    """
    print(f"[Master Thread] Starting packet production...")
    start_time = time.monotonic()
    sequence_number = 0
    data_payload = os.urandom(PACKET_SIZE)

    while (time.monotonic() - start_time) < TEST_DURATION:
        packet = struct.pack('!Q', sequence_number) + data_payload
        try:
            # Put the created packet into the queue. This will block if the
            # queue is full, preventing memory from exploding.
            packet_queue.put(packet, timeout=1) # Add timeout to prevent indefinite block
            sequence_number += 1
        except queue.Full:
            print("[Master Thread] Queue is full, producer is slowing down.")
            continue
        except Exception as e:
            print(f"[Master Thread] Error producing packet: {e}")
            break
            
    print(f"[Master Thread] Production finished. Produced {sequence_number} packets.")
    
    # After finishing, put a "sentinel" value (None) into the queue for each
    # worker thread. This signals them to stop.
    for _ in range(len(SERVER_IPS)):
        packet_queue.put(None)
        
    return sequence_number


def client_worker(target_ip, port, packets_sent_counter):
    """
    This is a "consumer" thread. It takes ready-made packets from the
    queue and sends them using its dedicated socket.
    """
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            server_address = (target_ip, port)
            print(f"[Worker {threading.get_ident()}] Sending to {server_address}")

            while True:
                # Get a packet from the queue. This will block until a packet is available.
                try:
                    packet = packet_queue.get(timeout=1)
                except queue.Empty:
                    # If the queue is empty for a second, assume master is done and break
                    print(f"[Worker {threading.get_ident()}] Queue empty, assuming completion.")
                    break

                # Check for the sentinel value to know when to stop.
                if packet is None:
                    break

                try:
                    s.sendto(packet, server_address)
                    packets_sent_counter[0] += 1
                except socket.error as e:
                    # This error means the OS send buffer is full. The packet is dropped.
                    # The master producer will pause due to the full queue, which is the desired backpressure.
                    pass 

    except OSError as e:
        print(f"[Worker {threading.get_ident()}] Error creating socket for {target_ip}: {e}")
    
    print(f"[Worker {threading.get_ident()}] Finished.")


if __name__ == '__main__':
    if not SERVER_IPS:
        print("[!] The SERVER_IPS list cannot be empty. Please configure it.")
        sys.exit(1)
        
    worker_threads = []
    
    # A simple list to hold the total packet count, passed by reference.
    total_packets_sent = [0] 

    print(f"[*] Starting a sender thread for each of {len(SERVER_IPS)} server IPs...")

    # Create and start a worker/consumer thread for each destination IP
    for ip in SERVER_IPS:
        thread = threading.Thread(target=client_worker, args=(ip, PORT, total_packets_sent))
        worker_threads.append(thread)
        thread.start()

    # Start the master/producer thread using our custom class
    master_thread = ThreadWithReturnValue(target=master_packet_producer)
    master_thread.start()

    # Wait for the master thread to finish and get its return value
    final_sequence_number = master_thread.join()
    
    # Wait for all worker threads to finish sending remaining packets
    for thread in worker_threads:
        thread.join()
        
    print("\n[*] All threads finished.")

    # --- Signal End of Test ---
    print("[*] Sending end signal to all server IPs.")
    # Ensure final_sequence_number is an integer before packing
    if final_sequence_number is None:
        print("[!] Master thread did not return a sequence number. Using total packets sent.")
        final_sequence_number = total_packets_sent[0]

    end_message = END_MESSAGE_FLAG + struct.pack('!Q', final_sequence_number)
    
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        for _ in range(10):
            for ip in SERVER_IPS:
                try:
                    s.sendto(end_message, (ip, PORT))
                except socket.error:
                    pass
            time.sleep(0.01)

    total_data_sent = total_packets_sent[0] * (PACKET_SIZE + 8)
    total_megabytes_sent = total_data_sent / (1024 * 1024)
    print("\n--- Client Summary ---")
    print(f"Total Packets Sent (by workers): {total_packets_sent[0]}")
    print(f"Total Data Sent:                 {total_megabytes_sent:.2f} MB")
    print("----------------------")
