# crc32_ieee.py
from typing import Iterable

_POLY = 0xEDB88320

def crc32_init() -> int:
    return 0xFFFFFFFF

def crc32_update(crc: int, data: bytes | bytearray | memoryview | Iterable[int]) -> int:
    if not isinstance(data, (bytes, bytearray, memoryview)):
        data = bytes(data)
    for b in data:
        crc ^= b
        for _ in range(8):
            m = -(crc & 1) & 0xFFFFFFFF
            crc = (crc >> 1) ^ (_POLY & m)
    return crc & 0xFFFFFFFF

def crc32_final(crc: int) -> int:
    return (~crc) & 0xFFFFFFFF

def crc32_calc(data: bytes) -> int:
    return crc32_final(crc32_update(crc32_init(), data))
