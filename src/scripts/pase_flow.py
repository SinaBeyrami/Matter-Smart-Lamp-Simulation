import socket, binascii
from tlv import build_pbkdf_request, build_pake1, build_pake3

ESP_IP   = "192.168.105.22"
PORT     = 5540
TIMEOUT  = 2

def send(pkt, tag):
    print(f"\n>> {tag}: {binascii.hexlify(pkt).decode()}")
    sock.sendto(pkt, (ESP_IP, PORT))
    try:
        data, _ = sock.recvfrom(256)
        print(f"<< resp: {binascii.hexlify(data).decode()}")
        return data
    except socket.timeout:
        print("!! timeout")
        return None

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(TIMEOUT)

# PBKDFParamRequest -> Response
send(build_pbkdf_request(), "PBKDFParamRequest")

# PASE PAKE1 -> PAKE2
send(build_pake1(), "PAKE1")

# PASE PAKE3 -> Just ACK
send(build_pake3(), "PAKE3")

sock.close()