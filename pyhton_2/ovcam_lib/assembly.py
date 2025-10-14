# reasm.py
from typing import Optional, Dict


PIXFORMAT = {
    0: "RGB565",      # 2 BPP
    1: "YUV422",      # 2 BPP
    2: "YUV420",      # 1.5 BPP
    3: "GRAYSCALE",   # 1 BPP
    4: "JPEG",        # compressed
    5: "RGB888",      # 3 BPP
    6: "RAW",
    7: "RGB444",      # 3 bytes per 2 pixels
    8: "RGB555",      # 3 bytes per 2 pixels
    9: "RAW8",        # 8-bit
}
FRAMESIZE = {
    0:  ("96X96",     96,   96),
    1:  ("QQVGA",     160,  120),
    2:  ("128X128",   128,  128),
    3:  ("QCIF",      176,  144),
    4:  ("HQVGA",     240,  176),
    5:  ("240X240",   240,  240),
    6:  ("QVGA",      320,  240),
    7:  ("320X320",   320,  320),
    8:  ("CIF",       400,  296),
    9:  ("HVGA",      480,  320),
    10: ("VGA",       640,  480),
    11: ("SVGA",      800,  600),
    12: ("XGA",       1024, 768),
    13: ("HD",        1280, 720),
    14: ("SXGA",      1280, 1024),
    15: ("UXGA",      1600, 1200),
    # 3MP sensors
    16: ("FHD",       1920, 1080),
    17: ("P_HD",       720, 1280),
    18: ("P_3MP",      864, 1536),
    19: ("QXGA",      2048, 1536),
    # 5MP sensors
    20: ("QHD",       2560, 1440),
    21: ("WQXGA",     2560, 1600),
    22: ("P_FHD",     1080, 1920),
    23: ("QSXGA",     2560, 1920),
    24: ("5MP",       2592, 1944),
    25: ("INVALID",   None, None),
}

# def get_pixformat_at(dat: bytes, offset: int):
#     """
#     อ่าน 1 ไบต์ที่ตำแหน่ง offset ของ dat แล้วคืน (mode_id, mode_name)
#     - ถ้า offset เกินความยาว จะคืน (None, "OUT_OF_RANGE")
#     - ถ้าไม่รู้จักหมายเลข จะคืน (mode_id, "UNKNOWN")
#     """
#     if offset < 0 or offset >= len(dat):
#         return None, "OUT_OF_RANGE"
#     mode_id = dat[offset]
#     return mode_id, PIXFORMAT.get(mode_id, "UNKNOWN")

def get_miniheader_at(dat: bytes, offset: int):
    """
    อ่าน 1 ไบต์ที่ตำแหน่ง offset เป็น mode_id และอ่าน 1 ไบต์ที่ offset+6 เป็น framesize_id
    คืนค่า: (mode_id, mode_name, framesize_id, framesize_name, width, height)

    กรณีพิเศษ:
      - ถ้า offset เกินความยาว:   (None, "OUT_OF_RANGE", None, "OUT_OF_RANGE", None, None)
      - ถ้า offset+6 เกินความยาว: framesize จะเป็น (None, "OUT_OF_RANGE", None, None)
      - ถ้าไม่รู้จักหมายเลข:      name="UNKNOWN" และขนาดเป็น None
    """
    # ตรวจ mode_id
    if offset < 0 or offset >= len(dat):
        return None, "OUT_OF_RANGE", None, "OUT_OF_RANGE", None, None

    mode_id = dat[offset]
    mode_name = PIXFORMAT.get(mode_id, "UNKNOWN")

    # ตรวจ framesize_id (offset+6)
    fs_off = offset + 5
    if fs_off < 0 or fs_off >= len(dat):
        return mode_id, mode_name, None, "OUT_OF_RANGE", None, None

    framesize_id = dat[fs_off]
    fs_info = FRAMESIZE.get(framesize_id)
    if fs_info is None:
        # ไม่รู้จัก framesize_id
        return mode_id, mode_name, framesize_id, "UNKNOWN", None, None

    fs_name, w, h = fs_info
    return mode_id, mode_name, framesize_id, fs_name, w, h


class FrameReassembler:
    """
    ประกอบเฟรมจากแพ็กเก็ตแบบ chunk
    - payload_start   : ตำแหน่งเริ่ม payload ใน dat (เช่น 9 ถ้า header 9 ไบต์)
    - miniheader_len  : ความยาว mini-header ภายใน payload (เช่น 8)
    - has_crc         : True → ตัด 4 ไบต์ท้ายเฟรมก่อนประกอบ
    - idx_off/tot_off : offset (ภายใน payload) ของ uint16 index/total (LE)
    - fid_off         : offset (ภายใน payload) ของ frame_id (uint16 LE) ถ้าไม่มีให้ None
    - one_based       : index เริ่มที่ 1 (True) หรือ 0 (False)
    """
    def __init__(self,
                 payload_start: int = 9,
                 miniheader_len: int = 8,
                 *,
                 has_crc: bool = False,
                 idx_off: int = 1,
                 tot_off: int = 3,
                 fid_off: Optional[int] = None,
                 little_endian: bool = True,
                 one_based: bool = True):
        self.ps = payload_start
        self.mh = miniheader_len
        self.has_crc = has_crc
        self.idx_off = idx_off
        self.tot_off = tot_off
        self.fid_off = fid_off
        self.bo = "little" if little_endian else "big"
        self.one_based = one_based
        self._st: Dict[int, Dict] = {}  # fid -> {tot:int, next:int, chunks:dict}

    def _u16(self, b: bytes) -> int: return int.from_bytes(b, self.bo, signed=False)
    def _start_idx(self) -> int:      return 1 if self.one_based else 0
    def _next(self, i: int) -> int:   return i + 1
    def _flush(self, fid: int):       self._st.pop(fid, None)

    def update(self, dat: bytes) -> Optional[bytes]:
        if len(dat) <= self.ps:  # ไม่มี payload
            return None

        # ตัด header และ (ถ้ามี) CRC 4 ไบต์ท้าย
        end = -4 if self.has_crc and len(dat) >= (self.ps + 4) else None
        payload = dat[self.ps:end]

        # ต้องพออ่าน u16 (idx/tot/optional fid)
        need = max(self.idx_off+2, self.tot_off+2,
                   (self.fid_off+2) if self.fid_off is not None else 0)
        if len(payload) < need or len(payload) < self.mh:
            return None

        idx = self._u16(payload[self.idx_off:self.idx_off+2])
        tot = self._u16(payload[self.tot_off:self.tot_off+2])
        if tot <= 0:
            print("chunuk_total <=0 ")
            return None

        fid = self._u16(payload[self.fid_off:self.fid_off+2]) if self.fid_off is not None else 0
        chunk_data = payload[self.mh:]  # ต่อชิ้นหลัง mini-header 8 ไบต์

        start_idx = self._start_idx()
        st = self._st.get(fid)

        # เริ่มเฟรมใหม่ได้เฉพาะเมื่อเจอ idx == start_idx
        if st is None:
            if idx != start_idx:
                return None
            self._st[fid] = st = {"tot": tot, "next": self._next(idx), "chunks": {idx: chunk_data}}
        else:
            # tot เปลี่ยน/idx เกิน → flush แล้วเริ่มใหม่เฉพาะเมื่อ idx == start_idx
            if tot != st["tot"] or idx > tot:
                self._flush(fid)
                if idx != start_idx:
                    return None
                self._st[fid] = st = {"tot": tot, "next": self._next(idx), "chunks": {idx: chunk_data}}
            else:
                # ต้องต่อเนื่องเป๊ะ
                if idx != st["next"]:
                    self._flush(fid)
                    if idx != start_idx:
                        return None
                    self._st[fid] = st = {"tot": tot, "next": self._next(idx), "chunks": {idx: chunk_data}}
                else:
                    st["chunks"][idx] = chunk_data
                    st["next"] = self._next(idx)

        # ครบเฟรมหรือยัง
        if self.one_based:
            done = (idx == tot) and all(i in st["chunks"] for i in range(1, tot+1))
            order = range(1, tot+1)
        else:
            done = (idx + 1 == tot) and all(i in st["chunks"] for i in range(0, tot))
            order = range(0, tot)

        if done:
            frame = b"".join(st["chunks"][i] for i in order)
            self._flush(fid)
            return frame
        return None

