from ovcam_lib.param_conf import config
from ovcam_lib.overhead import PacketOverhead
from ovcam_lib.crc32 import crc32_calc
import socket
import struct

IP = "192.168.1.235"
PORT = 5009
CRC = 1
# ----- helpers สำหรับ payload -----
# def build_set_payload(pid: int, fmt: int, v: int) -> bytes:
#     # โปรโตคอล: [u8 param_id][u8 fmt][u8 size][value...]
#     if   fmt == 0:  # int8
#         return struct.pack("<BBBb",  pid, fmt, 1, v)
#     elif fmt == 1:  # uint8
#         return struct.pack("<BBBB",  pid, fmt, 1, v)
#     elif fmt == 2:  # int16
#         return struct.pack("<BBBh",  pid, fmt, 2, v)
#     elif fmt == 3:  # uint16
#         return struct.pack("<BBBBH", pid, fmt, 2, v)
#     else:
#         raise ValueError("fmt unsupported")

# def build_name_payload(name: str) -> bytes:
#     # โปรโตคอล: [u8 len][ASCII name...]
#     b = name.encode("ascii")
#     if len(b) > 255: raise ValueError("name too long")
#     return bytes([len(b)]) + b

# def build_id_payload(rid: int) -> bytes:
#     # ถ้าฝั่งบอร์ดรองรับส่งเป็น "id" ตรง ๆ (ง่ายและสั้น)
#     return struct.pack("<B", rid)

def value_to_bytes(fmt: int, v: int) -> bytes:
    if   fmt == 0: return struct.pack("<b",  v)  # int8
    elif fmt == 1: return struct.pack("<B",  v)  # uint8
    elif fmt == 2: return struct.pack("<h",  v)  # int16
    elif fmt == 3: return struct.pack("<H",  v)  # uint16
    else: raise ValueError("fmt unsupported")

def main():

    con =config()
    con.help()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
         while True:
            try:
                line = input("> ")
            except (EOFError, KeyboardInterrupt):
                print(); break
            keep, result = con.handle_line(line)  
            if not keep:
                break
            if result:
                cmd = result["cmd"]
                # pkt = None

                if result["kind"] == "set":
                    key   = result["key"]
                    value = result["value"]
                    pid   = result["param_id"]
                    fmt   = result["fmt"]
                    print("DEBUG:", cmd, key, value, pid, fmt)

                    # payload = build_set_payload(pid, fmt, value)
                    payload = value_to_bytes(fmt, value)    
                    ovh = PacketOverhead.create_overhead("RCSA", id=pid, cmd=cmd, payload_len=len(payload))
                    ovh_bytes = ovh.to_bytes()
                    
                    pkt = ovh_bytes + payload
                    if(CRC):
                        crc = crc32_calc(pkt)
                        pkt += struct.pack("<I", crc)
                    
                    

                elif result["kind"] in ("fs","pf"):
                    rid = result["id"]
                    ovh = PacketOverhead.create_overhead("RCSA", id=rid, cmd=cmd, payload_len=0)
                    pkt = ovh.to_bytes()
                    if CRC:
                        crc =  crc32_calc(pkt)
                        pkt += struct.pack("<I",crc)

                elif result["kind"] == "flip":
                    # flip = คำสั่งล้วน ไม่มี payload; ใช้ id=0
                    ovh = PacketOverhead.create_overhead("RCSA", id=0, cmd=cmd, payload_len=0)
                    pkt = ovh.to_bytes()
                    if CRC:
                        crc =  crc32_calc(pkt)
                        pkt += struct.pack("<I",crc)
                if pkt:
                    n = sock.sendto(pkt, (IP, PORT))
                    if(CRC):
                        print("CRC32 =", f"{crc:08X}")
                    print(f"total {n} bytes")
                    print(f"packet :{pkt}")
                    print(' '.join(f'{b:02X}'for b in pkt))
                    print(f"sent {n} bytes to {IP}:{PORT}")

                

    finally:
        sock.close()
           

if __name__ == "__main__":
    main()