import socket
import qrcode
from tlv import (build_onoff, build_basic_request, decode_basic_response,
                build_descriptor_request, decode_descriptor_response,
                build_level_request, build_timer_write, build_timer_read,
                decode_timer_resp, build_level_read, build_identify)

ESP_IP   = "192.168.105.22"
ESP_PORT = 5540
PIN = 20202021
DISC     = 4520
VID_PID  = "FFF1-8000"

def recv_skip_ack(sock, bufsize=256, timeout=1.0):
    sock.settimeout(timeout)
    while True:
        try:
            data, _ = sock.recvfrom(bufsize)
        except socket.timeout:
            return None
        if data == b"ACK":
            continue
        return data

def print_qr():
    payload = f"MT:{VID_PID}:{DISC}:{PIN}"
    qr = qrcode.QRCode(border=1)
    qr.add_data(payload)
    qr.make(fit=True)
    qr.print_ascii(invert=True)
    print(f"\nQR Payload → {payload}\n")

def cli_loop():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    print("Type 'on', 'off', 'toggle', 'level <0-100>', 'level?', 'timer <sec>', 'timer?', 'identify', 'info', 'desc' or 'exit'")
    while True:
        cmd = input(">> ").strip().lower()
        if cmd == "exit":
            break
        if cmd in ("on", "off"):
            tlv = build_onoff(0x01 if cmd == "on" else 0x00)
            sock.sendto(tlv, (ESP_IP, ESP_PORT))
            print("sent", cmd.upper())
        elif cmd == "info":
            sock.sendto(build_basic_request(), (ESP_IP, ESP_PORT))
            print("sent BASIC request …")
            sock.settimeout(1.0)

            while True:
                try:
                    data, _ = sock.recvfrom(256)
                except socket.timeout:
                    print("no response")
                    break

                if data == b"ACK":
                    continue

                info = decode_basic_response(data)
                if info:
                    print("Basic-Info decoded:", info)
                else:
                    print("unparsed:", data.hex(" "))
                break

        elif cmd == "desc":
            sock.sendto(build_descriptor_request(), (ESP_IP, ESP_PORT))
            print("sent DESCRIPTOR request …")
            data = recv_skip_ack(sock)
            if data is None:
                print("no response")
            else:
                info = decode_descriptor_response(data)
                if info:
                    print("Descriptor decoded:", info)
                else:
                    print("unparsed:", data.hex(" "))
            
        elif cmd.startswith("level "):
            try:
                pct = int(cmd.split()[1])
            except (IndexError, ValueError):
                print("usage: level 0-100")
                continue
            raw = int(pct * 254 / 100)
            sock.sendto(build_level_request(raw), (ESP_IP, ESP_PORT))
            print(f"sent LEVEL → {pct}%  ({raw}/254)")
            
        elif cmd == "timer?":
            sock.sendto(build_timer_read(), (ESP_IP, ESP_PORT))
            data = recv_skip_ack(sock)
            if data is None:
                print("no response")
            else:
                remaining = decode_timer_resp(data)
                print("Remaining:" if remaining is not None else "unparsed:",
                    remaining if remaining is not None else data.hex(" "))



        elif cmd.startswith("timer "):
            try:
                sec = int(cmd.split()[1])
            except (IndexError, ValueError):
                print("usage: timer <seconds>")
                continue
            sock.sendto(build_timer_write(sec), (ESP_IP, ESP_PORT))
            print(f"DelayedOff set to {sec} s")
            
        elif cmd == "toggle":
            sock.sendto(build_onoff(0x02), (ESP_IP, ESP_PORT))
            print("sent TOGGLE")
            
        elif cmd == "level?":
            sock.sendto(build_level_read(), (ESP_IP, ESP_PORT))
            data = recv_skip_ack(sock)
            if data is None:
                print("no response")
            elif len(data) >= 3:
                print("Current level:", data[-1])
            else:
                print("unparsed:", data.hex(" "))
                
        elif cmd.startswith("identify "):
            try:
                sec = int(cmd.split()[1])
            except (IndexError, ValueError):
                print("usage: identify <seconds>")
                continue
            sock.sendto(build_identify(sec), (ESP_IP, ESP_PORT))
            print(f"sent IDENTIFY → {sec} s")

        else:
            print("unknown cmd")

if __name__ == "__main__":
    print_qr()
    cli_loop()