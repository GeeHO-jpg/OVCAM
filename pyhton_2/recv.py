import logging
import struct
import select
from socket import socket, AF_INET, SOCK_DGRAM, SOL_SOCKET, SO_RCVBUF

import cv2
import numpy as np

from ovcam_lib.crc32 import crc32_calc
from ovcam_lib.fps_check import fps_on_frame_done
from ovcam_lib.assembly import get_miniheader_at, FrameReassembler
from ovcam_lib.stream import StreamFrame, stream_frame

# =========================
# Config / Logging
# =========================
logging.basicConfig(
    level=logging.WARNING,                      # เปลี่ยนเป็น DEBUG เพื่อดีบักละเอียด
    format="%(asctime)s %(levelname)s %(message)s",
)
log = logging.getLogger("rx")

HOST, PORT = "0.0.0.0", 5005
BUF = 65535

HDR_FMT = "<4sHBH"                               # magic(4) id(2) cmd(1) payload_len(2)
HDR_LEN = struct.calcsize(HDR_FMT)               # 9
MINI_HDR_LEN = 8                                 # TX: [pf][idx2][tot2][fs][rsv][rsv]
IDX_OFF = 1                                      # index เริ่มใน mini header
TOT_OFF = 3                                      # total เริ่มใน mini header

# =========================
# Socket
# =========================
s = socket(AF_INET, SOCK_DGRAM)
s.setsockopt(SOL_SOCKET, SO_RCVBUF, 4 * 1024 * 1024)  # กันดรอปตอน GRAY/RAW
s.bind((HOST, PORT))
s.setblocking(False)
log.info("listening on %s:%d ...", HOST, PORT)

# =========================
# Reassembler & Display
# =========================
reasm = FrameReassembler(
    payload_start=HDR_LEN,
    miniheader_len=MINI_HDR_LEN,
    has_crc=True,
    idx_off=IDX_OFF,
    tot_off=TOT_OFF,
    fid_off=None,
    one_based=True,
)

sf = StreamFrame(None, 4, window_name="stream")   # ใช้กับ non-JPEG

# =========================
# Main loop
# =========================
try:
    while True:
        r, _, _ = select.select([s], [], [], 1.0)
        if s not in r:
            continue

        dat, addr = s.recvfrom(BUF)

        # --- ตรวจ header ---
        if len(dat) < HDR_LEN:
            continue
        magic, pkt_id, cmd, psize = struct.unpack(HDR_FMT, dat[:HDR_LEN])
        if magic != b"RCSA":
            continue

        # --- mini header ---
        mode_id, mode_name, fs_id, fs_name, w_x, h_x = get_miniheader_at(dat, HDR_LEN)

        # chunk index/total (1-based, little-endian)
        chunk_idx   = int.from_bytes(dat[HDR_LEN + 1:HDR_LEN + 3], "little")
        chunk_total = int.from_bytes(dat[HDR_LEN + 3:HDR_LEN + 5], "little")

        # --- รีเซ็ต reassembler ทุก "ต้นเฟรม" กัน state ค้าง ---
        if chunk_idx == 1:
            if hasattr(reasm, "reset"):
                reasm.reset()
            else:
                reasm = FrameReassembler(
                    payload_start=HDR_LEN,
                    miniheader_len=MINI_HDR_LEN,
                    has_crc=True,
                    idx_off=IDX_OFF,
                    tot_off=TOT_OFF,
                    fid_off=None,
                    one_based=True,
                )

        # --- ป้อน reassembler (ภายในจัดการ CRC แล้ว) ---
        frme_bytes = reasm.update(dat)

        # --- แสดงผลเฉพาะเมื่อ "เฟรมครบ" เท่านั้น ---
        frame_done = frme_bytes is not None
        _ = fps_on_frame_done(frame_done)
        if not frame_done:
            continue

        # --- JPEG detection: ยึดระบบคุณ = 4 (สำรองด้วยชื่อ)
        is_jpeg = (mode_id == 4) or (mode_name and mode_name.upper() == "JPEG")

        if is_jpeg:
            # ทางลัด JPEG (imdecode -> imshow)
            shown, err = stream_frame(frme_bytes, 4, window_name="stream")
        else:
            # non-JPEG ต้องมี w,h ถูกต้อง (มาจาก mini header)
            if not (w_x and h_x):
                log.warning("missing width/height for non-JPEG; drop frame")
                continue
            sf.set(frme_bytes, mode_id, w_x, h_x)
            shown, err = sf.show()

except KeyboardInterrupt:
    pass
finally:
    s.close()
    log.info("socket closed")
