

import sys, os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from ovcam_lib.overhead import PacketOverhead
import socket
def main():

    oh = PacketOverhead.create_overhead("RCSA", id=1, cmd=2, payload_len=10)
    payload = bytes(range(10))  
    packet = oh.to_bytes() + payload  

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    try:
        n = sock.sendto(packet, ("192.168.1.235", 5005))  # <-- ส่ง packet ไม่ใช่ b"hello"
        print(f"sent {n} bytes to 192.168.1.235:5005")
    finally:
        sock.close()

    print(packet)  
    print(' '.join(f'{b:02X}' for b in packet))
    # print(oh)
    # print("bytes:", oh.to_bytes())

if __name__ == "__main__":
    main()
