import socket

def run():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 54001))
    s.send(b'\x00\x1aLsaiful                     1\n')
    data = b""
    while len(data) < 20000:
        chunk = s.recv(4096)
        if not chunk: break
        data += chunk
    with open("dump.bin", "wb") as f:
        f.write(data)

run()
