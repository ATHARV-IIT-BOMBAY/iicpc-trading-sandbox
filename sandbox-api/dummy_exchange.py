import socket
import struct

HOST = '127.0.0.1'
PORT = 8080

# The format string matching our C++ struct:
# <  = Little-endian (Standard for Mac/Intel processors)
# Q  = uint64_t (8 bytes)
# 8s = char[8] (8 bytes)
# B  = uint8_t (1 byte)
# d  = double (8 bytes)
# I  = uint32_t (4 bytes)
ORDER_FORMAT = '<Q8sBdI'
ORDER_SIZE = struct.calcsize(ORDER_FORMAT) # Exactly 29 bytes

def start_exchange():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen()
        print(f"--- BINARY EXCHANGE ONLINE ---")
        print(f"Listening for custom binary payloads on port {PORT}...")
        
        while True:
            conn, addr = s.accept()
            with conn:
                while True:
                    # 1. Read exactly 29 bytes
                    data = conn.recv(ORDER_SIZE)
                    if not data or len(data) < ORDER_SIZE:
                        break
                    
                    # 2. Unpack the C-struct directly into Python variables
                    order_id, symbol_bytes, side, price, qty = struct.unpack(ORDER_FORMAT, data)
                    
                    # 3. Fire the 8-byte binary ACK back (Just the uint64_t order_id)
                    conn.sendall(struct.pack('<Q', order_id))

if __name__ == "__main__":
    start_exchange()