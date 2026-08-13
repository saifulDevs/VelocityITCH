import socket
import struct

def run():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 54001))
    s.send(b'\x00\x1aLsaiful                     1\n')
    
    while True:
        header = s.recv(2)
        if not header: break
        length = struct.unpack('>H', header)[0]
        data = s.recv(length)
        if not data: break
        
        packet_type = chr(data[0])
        if packet_type == 'S':
            msg_type = chr(data[1])
            if msg_type == 'A':
                print("Got A! Raw payload:", data.hex())
                break

run()
