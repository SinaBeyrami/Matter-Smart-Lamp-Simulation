from zeroconf import Zeroconf, ServiceBrowser
import time

class Listener:
    def add_service(self, zc, t, name):
        info = zc.get_service_info(t, name)
        if info:
            print(f"\nFound: {name}")
            print("  ↳ IPv4:", ".".join(map(str, info.addresses[0])))
            if info.properties:
                for k, v in info.properties.items():
                    print(f"  ↳ {k.decode()} = {v.decode()}")
Zeroconf().close()

zc = Zeroconf()
ServiceBrowser(zc, "_matter._udp.local.", Listener())
print("Scanning for 5 s …")
time.sleep(5)
zc.close()