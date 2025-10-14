import  struct


MAGIC    = b"RCSA"
HDR_FMT  = "<4sHBH"                     # 4s, H, B, H
HDR_LEN  = struct.calcsize(HDR_FMT)     # ควรได้ 9

def parse_packet(dat: bytes):
    if len(dat) < HDR_LEN:
        raise ValueError(f"too small for header: {len(dat)} < {HDR_LEN}")

    magic, pkt_id, cmd, psize = struct.unpack(HDR_FMT, dat[:HDR_LEN])

    if magic != MAGIC:
        raise ValueError(f"bad magic: {magic!r} != {MAGIC!r}")

    if len(dat) < HDR_LEN + psize:
        raise ValueError(f"payload truncated: have {len(dat)-HDR_LEN}, need {psize}")

    payload = dat[HDR_LEN:HDR_LEN+psize]
    extra   = dat[HDR_LEN+psize:]   # เผื่อมี CRC/ตรึงท้ายอื่น ๆ

    return pkt_id, cmd, psize, payload, extra
