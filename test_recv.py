import socket
import struct

def run():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 54001))
    
    # Handshake L
    s.send(b'\x00\x1aL' + b'saiful    ' + b'                 1\n')
    header = s.recv(3)
    if not header:
        return
    length = struct.unpack('>H', header[:2])[0]
    payload = s.recv(length - 1)
    print("Handshake response:", header[2:3], payload)
    
    while True:
        header = s.recv(2)
        if not header: break
        length = struct.unpack('>H', header)[0]
        data = s.recv(length)
        if not data: break
        msg_type = chr(data[0])
        print(f"Msg: {msg_type} len: {length}")
        if msg_type == 'H':
            ts = chr(data[9])
            print(f"Trading State: '{ts}'")
            break

run()
