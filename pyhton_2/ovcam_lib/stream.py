# # stream_cv.py
# import numpy as np
# import cv2
# from typing import Optional, Tuple


from __future__ import annotations
from typing import Optional, Tuple
import numpy as np
import cv2

def stream_frame(jpeg_bytes: Optional[bytes], mode_id: int,
                 window_name: str = "stream") -> Tuple[bool, Optional[str]]:
    """
    แสดงภาพ 1 เฟรมจาก JPEG bytes ด้วย OpenCV
    - jpeg_bytes: ข้อมูล JPEG ของเฟรมที่ 'ประกอบครบแล้ว' (None = ยังไม่ครบ)
    - mode_id   : ใช้ตรวจว่าเป็น JPEG (4) หรือไม่
    - window_name: ชื่อหน้าต่าง OpenCV

    return: (shown, err)
      - shown=True  เมื่อแสดงภาพสำเร็จ
      - err เป็นข้อความเมื่อเกิดข้อผิดพลาด (เช่น โหมดไม่รองรับ / decode ล้มเหลว)
    """
    if jpeg_bytes is None:
        return False, None  # ยังไม่มีเฟรมครบ

    if mode_id != 4:  # 4 = JPEG
        return False, f"Unsupported pixformat: mode_id={mode_id} (need 4=JPEG)"

    try:
        buf = np.frombuffer(jpeg_bytes, dtype=np.uint8)
        img = cv2.imdecode(buf, cv2.IMREAD_COLOR)  # BGR
        if img is None:
            return False, "cv2.imdecode returned None"

        cv2.imshow(window_name, img)
        # หมายเหตุ: ควรเรียก waitKey(1) ทุกเฟรมเพื่ออัปเดตหน้าต่างและ process events
        cv2.waitKey(1)
        return True, None
    except Exception as e:
        return False, f"decode/display error: {e}"



class StreamFrame:
    """
    ใช้แสดงผลภาพจาก buffer เดียว โดยเลือกตาม mode_id:
      - 4 = JPEG
      - 3 = GRAYSCALE (8-bit)
      - 1 = YUV422 (YUY2)

    สร้างด้วย:
        StreamFrame(data, mode_id, w=None, h=None, window_name="stream")

    ใช้งาน:
        shown, err = StreamFrame(data, mode_id, w, h).show()
        # หรือ
        sf = StreamFrame(None, 4); sf.set(data, 4); sf.show()
    """
    # mapping โหมดที่รองรับ
    SUPPORTED = {1: "YUV422", 3: "GRAYSCALE", 4: "JPEG"}

    def __init__(self,
                 data: Optional[bytes],
                 mode_id: int,
                 w: Optional[int] = None,
                 h: Optional[int] = None,
                 *,
                 window_name: str = "stream") -> None:
        self.data = data
        self.mode_id = mode_id
        self.w = w
        self.h = h
        self.window_name = window_name

    # อนุญาตอัปเดตข้อมูลภายหลัง
    def set(self, data: Optional[bytes], mode_id: Optional[int] = None,
            w: Optional[int] = None, h: Optional[int] = None) -> None:
        if data is not None:
            self.data = data
        if mode_id is not None:
            self.mode_id = mode_id
        if w is not None:
            self.w = w
        if h is not None:
            self.h = h

    # เมธอดหลัก: ตัดสินใจตาม mode แล้วแสดงผล
    def show(self) -> Tuple[bool, Optional[str]]:
        if self.data is None:
            return False, None  # ยังไม่มีเฟรม

        if self.mode_id not in self.SUPPORTED:
            return False, f"Unsupported mode_id={self.mode_id}. Supported: {sorted(self.SUPPORTED)}"

        try:
            if self.mode_id == 4:
                # ---------- JPEG ----------
                buf = np.frombuffer(self.data, dtype=np.uint8)
                img = cv2.imdecode(buf, cv2.IMREAD_COLOR)  # BGR
                if img is None:
                    return False, "cv2.imdecode returned None"
                cv2.imshow(self.window_name, img)
                cv2.waitKey(1)
                return True, None

            elif self.mode_id == 3:
                # ---------- GRAYSCALE ----------
                if not self._check_wh():
                    return False, "width/height required for GRAYSCALE"
                arr = np.frombuffer(self.data, dtype=np.uint8)
                expected = self.w * self.h
                if arr.size != expected:
                    return False, f"GRAY size mismatch: {arr.size} != {expected}"
                img = arr.reshape(self.h, self.w)  # 1-channel
                cv2.imshow(self.window_name, img)
                cv2.waitKey(1)
                return True, None

            elif self.mode_id == 1:
                # ---------- YUV422 (YUY2) ----------
                if not self._check_wh():
                    return False, "width/height required for YUV422"
                arr = np.frombuffer(self.data, dtype=np.uint8)
                expected = self.w * self.h * 2
                if arr.size != expected:
                    return False, f"YUV422 size mismatch: {arr.size} != {expected}"
                yuy2 = arr.reshape(self.h, self.w, 2)
                img = cv2.cvtColor(yuy2, cv2.COLOR_YUV2BGR_YUY2)
                cv2.imshow(self.window_name, img)
                cv2.waitKey(1)
                return True, None
            
            elif self.mode_id == 0:
                # ---------- RGB565 (little-endian) ----------
                if not self._check_wh():
                    return False, "width/height required for RGB565"
                arr = np.frombuffer(self.data, dtype=np.uint8)
                expected = self.w * self.h * 2
                if arr.size != expected:
                    return False, f"RGB565 size mismatch: {arr.size} != {expected}"

                # บังคับอ่านเป็น uint16 ลิตเติลเอนเดียน
                px = arr.view(dtype=np.dtype('<u2')).reshape(self.h, self.w)

                # bit layout: rrrrrggg gggbbbbb
                r5 = (px >> 11) & 0x1F
                g6 = (px >> 5)  & 0x3F
                b5 =  px        & 0x1F

                # scale 5/6-bit -> 8-bit
                r8 = ((r5 << 3) | (r5 >> 2)).astype(np.uint8)
                g8 = ((g6 << 2) | (g6 >> 4)).astype(np.uint8)
                b8 = ((b5 << 3) | (b5 >> 2)).astype(np.uint8)

                # OpenCV ใช้ BGR
                img = np.dstack((b8, g8, r8))
                cv2.imshow(self.window_name, img)
                cv2.waitKey(1)
                return True, None

            else:
                # ไม่ควรมาถึง เพราะเราจำกัด SUPPORTED แล้ว
                return False, f"Unhandled mode_id={self.mode_id}"

        except Exception as e:
            return False, f"decode/display error: {e}"

    # ---------- helpers ----------
    def _check_wh(self) -> bool:
        return (isinstance(self.w, int) and isinstance(self.h, int)
                and self.w > 0 and self.h > 0)