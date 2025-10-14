
import struct
from dataclasses import dataclass

@dataclass(frozen=True)
class PacketOverhead:
    header: bytes   # 4 bytes
    id: int         # 0..65535
    cmd: int        # 0..255
    payload_len: int# 0..65535

    # little-endian: 4s H B H
    _FMT = "<4sHBH"
    SIZE = struct.calcsize(_FMT)  # 9 bytes

    @staticmethod
    def _ensure_header(h: str | bytes) -> bytes:
        if isinstance(h, str):
            h = h.encode("ascii", "strict")
        if len(h) != 4:
            raise ValueError("header must be exactly 4 bytes (e.g. b'OVJ1').")
        return h

    @staticmethod
    def _check_range(name: str, val: int, lo: int, hi: int) -> int:
        if not (lo <= int(val) <= hi):
            raise ValueError(f"{name} out of range: {val} (expected {lo}..{hi})")
        return int(val)

    @classmethod
    def create_overhead(cls, header: str | bytes, id: int, cmd: int, payload_len: int) -> "PacketOverhead":
        """
        Build overhead object with validation.
        Layout (little-endian): 4s | H | B | H  => total 9 bytes.
        """
        h = cls._ensure_header(header)
        i = cls._check_range("id", id, 0, 0xFFFF)
        c = cls._check_range("cmd", cmd, 0, 0xFF)
        p = cls._check_range("payload_len", payload_len, 0, 0xFFFF)
        return cls(h, i, c, p)

    def to_bytes(self) -> bytes:
        """Serialize to bytes: 4s H B H (little-endian)."""
        return struct.pack(self._FMT, self.header, self.id, self.cmd, self.payload_len)

    @classmethod
    def from_bytes(cls, buf: bytes) -> "PacketOverhead":
        """Parse from a 9-byte buffer into a PacketOverhead."""
        if len(buf) < cls.SIZE:
            raise ValueError(f"buffer too small: {len(buf)} < {cls.SIZE}")
        h, i, c, p = struct.unpack(cls._FMT, buf[:cls.SIZE])
        return cls(h, i, c, p)
