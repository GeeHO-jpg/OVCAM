import socket, struct
from typing import Optional, Tuple

class UdpMulticast:
    """Multicast UDP แบบง่าย: เปิดครั้งเดียว ใช้ทั้งส่ง/รับได้"""

    def __init__(
        self,
        group: str,
        port: int,
        *,
        iface_ip: Optional[str] = None,
        ttl: int = 1,
        loopback: bool = True,
        recv_buf: Optional[int] = 4 * 1024 * 1024,
        bind_for_recv: bool = True,
        timeout: Optional[float] = None,
    ) -> None:
        self._ensure_multicast(group)
        self.group, self.port = group, port
        self.iface_ip = iface_ip or self._default_iface_ip()
        self.addr = (group, port)
        self.timeout = timeout

        # Create UDP/IPv4 socket
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        # Allow quick rebinding (useful during rapid restarts)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        # Enlarge receive buffer for high-rate streams (best-effort; OS may clamp)
        if recv_buf:
            try: s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, recv_buf)
            except OSError: pass
        if bind_for_recv:
            s.bind(("", port))


        # Outbound multicast settings (sending)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)  # hop limit
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, int(loopback))  # receive own sends
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(self.iface_ip))  # choose egress NIC

        # Join the multicast group on the chosen interface (for receiving)
        self._mreq = struct.pack("=4s4s", socket.inet_aton(group), socket.inet_aton(self.iface_ip))
        s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, self._mreq)

        if timeout is not None:
            s.settimeout(timeout)

        self.sock = s
        self._closed = False

    # ---------- API ----------
    def send(self, data: bytes, dest: Optional[Tuple[str, int]] = None) -> int:
        return self.sock.sendto(data, dest or self.addr)

    def recv(self, bufsize: int = 65535, timeout: Optional[float] = None):
        if timeout is not None:
            self.sock.settimeout(timeout)
        elif self.timeout is not None:
            self.sock.settimeout(self.timeout)
        else:
            self.sock.settimeout(None)
        return self.sock.recvfrom(bufsize)

    def close(self) -> None:
        if self._closed: return
        try: self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_DROP_MEMBERSHIP, self._mreq)
        except OSError: pass
        try: self.sock.close()
        finally: self._closed = True

    # ---------- Utils ----------
    @staticmethod
    def _ensure_multicast(addr: str) -> None:
        try:
            first = int(addr.split(".")[0])
            assert 224 <= first <= 239
        except Exception:
            raise ValueError(f"{addr} ไม่ใช่ multicast address (ต้องอยู่ 224.0.0.0–239.255.255.255)")

    @staticmethod
    def _default_iface_ip() -> str:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.connect(("8.8.8.8", 80))
            return s.getsockname()[0]
        finally:
            s.close()

    # ---------- Context manager ----------
    def __enter__(self): return self
    def __exit__(self, *exc): self.close()
