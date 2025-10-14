import time
from typing import Optional, Callable


def fps_update(chunk_idx_b: bytes, chunk_total_b: bytes,
               *, one_based: bool = True, little_endian: bool = True) -> Optional[float]:
    """เรียกทุกแพ็กเก็ต -> คืน FPS เมื่อถึงชิ้นสุดท้ายของเฟรมนั้น, ไม่งั้นคืน None"""
    # สถานะภายในฟังก์ชัน (persist ระหว่างการเรียก)
    if not hasattr(fps_update, "_t0"):
        fps_update._t0 = None

    if len(chunk_idx_b) < 2 or len(chunk_total_b) < 2:
        return None

    byteorder = "little" if little_endian else "big"
    idx = int.from_bytes(chunk_idx_b, byteorder)
    tot = int.from_bytes(chunk_total_b, byteorder)

    print(f"idx {idx} : tot {tot}")
    if tot <= 0:
        return None

    now = time.perf_counter()
    first = (idx == 1) if one_based else (idx == 0)
    done  = (idx == tot) if one_based else (idx + 1 == tot)

    # เริ่มจับเวลาเมื่อชิ้นแรกของเฟรม
    if fps_update._t0 is None or first:
        fps_update._t0 = now

    # ชิ้นสุดท้าย -> คำนวณ FPS แล้วรีเซ็ต
    if done and fps_update._t0 is not None:
        dt = now - fps_update._t0
        fps_update._t0 = None
        return (1.0 / dt) if dt > 0 else None

    return None

def fps_on_frame_done(frame_done: bool) -> Optional[float]:
    """เรียกทุกแพ็กเก็ต; คืน FPS เฉพาะตอน frame_done=True"""
    if not hasattr(fps_on_frame_done, "_t_prev"):
        fps_on_frame_done._t_prev = None

    if not frame_done:
        return None

    now = time.perf_counter()
    t_prev = fps_on_frame_done._t_prev
    fps_on_frame_done._t_prev = now

    if t_prev is None:
        return None  # เฟรมแรก ยังไม่มีค่าเทียบ
    dt = now - t_prev
    return (1.0 / dt) if dt > 0 else None
