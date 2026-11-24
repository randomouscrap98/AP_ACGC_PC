# The Wii side of the GameCube client (0.7.0): finds the "Animal Crossing AP" loader (the Nintendont fork in
# nintendont/) on the local network and names the per-seed memory card before the game boots.
# Protocol (nintendont/loader/source/ap_wait.c), both on port WII_PORT:
#   UDP: client -> broadcast "ACAP?\n"; Wii -> client "ACAP 1 <game> <card>\n" (<game> 1 = disc image chosen,
#        <card> the card name so far or "-")
#   TCP: Wii "ACAP 1\n"; client "CARD <name>\n"; Wii "OK\n" or "ERR <reason>\n", then closes
#   TCP: client "ISO <size>\n" + exactly <size> raw bytes: a disc image the Wii boots once (/wii_iso; it goes to
#        /games/ACAP_UPLOAD/game.iso on the Wii's boot device). Wii: "OK <details>\n" or "ERR <reason>\n", then closes
# Plain sockets, no Archipelago imports, so tests can run it against a fake Wii.
import os
import re
import select
import socket
import struct
import threading
from typing import Any, Callable, NamedTuple, Optional

WII_PORT = 38290  # AP_WAIT_PORT (ap_wait.h)
CARD_NAME_MAX = 40  # AP_CARD_NAME_MAX


class WiiReply(NamedTuple):
    address: str
    game_ok: bool
    card: str  # "" = none yet


def card_name(seed: str, team: int, slot: int) -> str:
    # The card file on the Wii's SD/USB: /saves/<name>.raw, named like the PC port's save folder ap_<seed>_<team>_<slot>.
    # Characters the loader refuses become "_"; a long seed name loses its end so team and slot stay.
    tail = f"_{team}_{slot}"
    seed = re.sub(r"[^A-Za-z0-9_-]", "_", seed)
    return "ap_" + seed[:CARD_NAME_MAX - 3 - len(tail)] + tail


def parse_reply(data: bytes, address: str) -> Optional[WiiReply]:
    parts = data.decode("ascii", "replace").split()
    if len(parts) != 4 or parts[0] != "ACAP" or parts[1] != "1":
        return None
    return WiiReply(address, parts[2] == "1", "" if parts[3] == "-" else parts[3])


def local_broadcasts() -> list[str]:
    # 255.255.255.255 leaves through one network adapter only (Windows picks one), so the x.y.z.255 of each local
    # address goes out too. /24 is a guess (most home networks); a wrong guess just goes nowhere.
    targets = ["255.255.255.255"]
    try:
        infos = socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET)
    except OSError:
        infos = []
    addrs = {info[4][0] for info in infos}
    try:
        # The adapter that routes outside (no packet is sent by a UDP connect)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("192.0.2.1", 9))
            addrs.add(s.getsockname()[0])
    except OSError:
        pass
    for a in sorted(addrs):
        if not a.startswith("127."):
            targets.append(a.rsplit(".", 1)[0] + ".255")
    return targets


class WiiFinder:
    # One non-blocking UDP socket: probe() sends the question, replies() collects the answers so far
    def __init__(self, port: int = WII_PORT) -> None:
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.bind(("", 0))
        self.sock.setblocking(False)

    def close(self) -> None:
        self.sock.close()

    def probe(self, extra: list[str]) -> None:
        # extra: addresses asked directly (the one set with /wii, for networks that drop broadcasts)
        for target in local_broadcasts() + extra:
            try:
                self.sock.sendto(b"ACAP?\n", (target, self.port))
            except OSError:
                pass  # no route on that adapter

    def replies(self) -> list[WiiReply]:
        found: list[WiiReply] = []
        while True:
            try:
                data, (address, _) = self.sock.recvfrom(256)
            except (BlockingIOError, InterruptedError):
                return found
            except OSError:
                return found  # Windows reports an earlier unreachable target here (ConnectionResetError)
            reply = parse_reply(data, address)
            if reply and all(r.address != address for r in found):
                found.append(reply)


def send_card(address: str, name: str, port: int = WII_PORT, timeout: float = 5) -> tuple[bool, str]:
    # Blocking (run it in a thread). Returns (accepted, the Wii's answer or the error).
    try:
        with socket.create_connection((address, port), timeout=timeout) as s:
            f = s.makefile("rwb")
            hello = f.readline().decode("ascii", "replace").strip()
            if hello != "ACAP 1":
                return False, f"unexpected greeting {hello!r}"
            f.write(f"CARD {name}\n".encode("ascii"))
            f.flush()
            answer = f.readline().decode("ascii", "replace").strip()
            return answer.split(" ")[0] == "OK", answer or "connection closed"
    except OSError as e:
        return False, str(e)


ISO_CHUNK = 1 << 20  # bytes per send during an upload
ISO_SEND_TIMEOUT = 60  # seconds the Wii may take to take a chunk (a stalled transfer, not a slow one)
ISO_ANSWER_TIMEOUT = 120  # the Wii writes the last chunk to its card before it answers


def _read_line(sock: socket.socket, limit: int = 256) -> str:
    # A line from a blocking socket, one byte at a time: the replies are short and nothing may be read ahead
    data = b""
    while len(data) < limit:
        c = sock.recv(1)
        if not c or c == b"\n":
            break
        data += c
    return data.decode("ascii", "replace").strip()


def send_iso(address: str, path: str, port: int = WII_PORT, timeout: float = 10,
             progress: Optional[Callable[[int, int], None]] = None) -> tuple[bool, str]:
    # Blocking (run it in a thread), like send_card. Uploads a disc image to the Wii's start screen: it writes it to
    # /games/ACAP_UPLOAD/game.iso on its boot device and boots it for this boot only (the saved image is untouched).
    # Returns (accepted, the Wii's answer or the error); progress(sent, size) after every chunk.
    size = os.path.getsize(path)
    sending = False
    try:
        with socket.create_connection((address, port), timeout=timeout) as s:
            hello = _read_line(s)
            if hello != "ACAP 1":
                return False, f"unexpected greeting {hello!r}"
            s.sendall(b"ISO %d\n" % size)
            sending = True
            sent = 0
            with open(path, "rb") as f:
                while True:
                    chunk = f.read(ISO_CHUNK)
                    if not chunk:
                        break
                    s.settimeout(ISO_SEND_TIMEOUT)
                    s.sendall(chunk)
                    sent += len(chunk)
                    if progress is not None:
                        progress(sent, size)
                    # The Wii can answer before the last byte (an ERR, then it closes): take it as it comes
                    if select.select([s], [], [], 0)[0]:
                        answer = _read_line(s)
                        return answer.split(" ")[0] == "OK", answer or "connection closed"
            if sent != size:
                return False, f"{path} shrank while it was read ({sent} of {size} bytes)"
            s.settimeout(ISO_ANSWER_TIMEOUT)
            answer = _read_line(s)
            return answer.split(" ")[0] == "OK", answer or "connection closed"
    except OSError as e:
        if sending:
            # The Wii gives up by sending ERR and closing; with data still in flight that arrives as a reset
            return False, f"connection dropped ({e}); the reason, if any, is on the Wii's screen"
        return False, str(e)


KERNEL_PORT = 38291  # AP_SERVER_PORT (nintendont/kernel/ap_server.h)
KERNEL_MAGIC, KERNEL_VERSION = 0x41434150, 1
KERNEL_MAX = 4096  # AP_SERVER_MAX: bytes per read / write request
FIND_TIMEOUT = 30  # seconds for the F request (a 24 MB search on the Wii; not measured, a guess with room)


class NintendontRAM:
    # The RAM bridge to a real Wii: the Nintendont fork's kernel server (nintendont/kernel/ap_server.c), same
    # interface as gc_client.DolphinRAM. Blocking sockets: call it from a thread. Requests: u8 cmd, 3 pad, u32 a, u32 b.
    # timeout: per request. Shorter than the game's 10 s "client alive" window (GC_AP_CLIENT_TIMEOUT_FRAMES), so a
    # stalled connection is replaced (cheap: the mailbox address is remembered) before the game notices
    def __init__(self, address: str, port: int = KERNEL_PORT, timeout: float = 5) -> None:
        self.address = address
        self.port = port
        self.timeout = timeout
        self.sock: Optional[socket.socket] = None
        self.lock = threading.Lock()  # one request at a time on the connection (callers may be several threads)

    def hook(self) -> bool:
        if self.sock is not None:
            return True
        try:
            self.sock = socket.create_connection((self.address, self.port), timeout=self.timeout)
            self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            magic, version = struct.unpack(">II", self._request(b"P", 0, 0, 8))
            if magic != KERNEL_MAGIC or version != KERNEL_VERSION:
                raise OSError(f"kernel server speaks {magic:08X} v{version}, expected v{KERNEL_VERSION}")
            return True
        except OSError:
            self.unhook()
            return False

    def unhook(self) -> None:
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
        self.sock = None

    def _recv(self, n: int) -> bytes:
        data = b""
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if not chunk:
                raise OSError("the Wii closed the connection")
            data += chunk
        return data

    def _request(self, cmd: bytes, a: int, b: int, reply: int, payload: bytes = b"") -> bytes:
        # The fixed-size reply; read / log fetch their data inside _exchange
        return self._exchange(cmd, a, b, reply, payload)[0]

    def _exchange(self, cmd: bytes, a: int, b: int, reply: int, payload: bytes = b"",
                  data_len_at: Optional[int] = None) -> tuple[bytes, bytes]:
        # One request + its whole reply under the lock. data_len_at: offset of a u32 length in the fixed reply
        # that more bytes follow
        with self.lock:
            if self.sock is None:
                raise OSError("not connected to the Wii")
            self.sock.sendall(cmd + b"\0\0\0" + struct.pack(">II", a, b) + payload)
            head = self._recv(reply)
            data = b""
            if data_len_at is not None:
                n, = struct.unpack_from(">I", head, data_len_at)
                data = self._recv(n)
            return head, data

    def read(self, addr: int, size: int) -> bytes:
        out = b""
        while size > 0:
            n = min(size, KERNEL_MAX)
            head, data = self._exchange(b"R", addr, n, 4, data_len_at=0)
            if len(data) != n:
                raise OSError(f"the Wii refused to read {n} bytes at {addr:08X}")
            out += data
            addr += n
            size -= n
        return out

    def write(self, addr: int, data: bytes) -> None:
        while data:
            part = data[:KERNEL_MAX]
            done, = struct.unpack(">I", self._request(b"W", addr, len(part), 4, part))
            if done != len(part):
                raise OSError(f"the Wii refused to write {len(part)} bytes at {addr:08X}")
            addr += len(part)
            data = data[KERNEL_MAX:]

    def batch(self, ops: list[tuple[str, int, Any]]) -> list[bytes]:
        # gc_client.Mailbox's queue: ("W", addr, data) / ("R", addr, size), all sent before any reply is read (the
        # kernel answers in order), split at KERNEL_MAX like read / write; the reads' bytes in order. A batch costs
        # about two round trips whatever its size (research/wii_link_latency.md). Replies are 4 bytes per write and
        # 4 + size per read, so they can't fill our receive buffer while we're still sending.
        requests, replies = [], []  # replies: ("W", n) / ("R", n, index of the op's read)
        reads: list[bytes] = []
        for op, addr, arg in ops:
            if op == "W":
                for at in range(0, len(arg), KERNEL_MAX):
                    part = arg[at:at + KERNEL_MAX]
                    requests.append(b"W\0\0\0" + struct.pack(">II", addr + at, len(part)) + part)
                    replies.append(("W", len(part), 0))
            else:
                for at in range(0, arg, KERNEL_MAX):
                    n = min(arg - at, KERNEL_MAX)
                    requests.append(b"R\0\0\0" + struct.pack(">II", addr + at, n))
                    replies.append(("R", n, len(reads)))
                reads.append(b"")
        with self.lock:
            if self.sock is None:
                raise OSError("not connected to the Wii")
            self.sock.sendall(b"".join(requests))
            for kind, n, index in replies:
                got, = struct.unpack(">I", self._recv(4))
                if got != n:
                    raise OSError(f"the Wii refused a {kind} of {n} bytes in a batch")
                if kind == "R":
                    reads[index] += self._recv(n)
        return reads

    def find_magic(self, start: int) -> int:
        # Game address of the next "ACAPMBOX" at or after start, 0 = none (the kernel searches its own RAM: up to
        # 24 MB, so this request gets FIND_TIMEOUT)
        with self.lock:
            if self.sock is not None:
                self.sock.settimeout(FIND_TIMEOUT)
        try:
            return struct.unpack(">I", self._request(b"F", start, 0, 4))[0]
        finally:
            with self.lock:
                if self.sock is not None:
                    self.sock.settimeout(self.timeout)

    def quit(self) -> None:
        # The game exits to the loader at its next controller read (testing: no hands needed at the Wii)
        self._request(b"Q", 0, 0, 4)

    def log(self) -> str:
        return self._exchange(b"L", 0, 0, 4, data_len_at=0)[1].decode("ascii", "replace")


def log_tail(prev: str, now: str) -> Optional[str]:
    # The part of the kernel log that is new since prev. The kernel keeps a 4 KB ring and sends all of it, so now =
    # (prev minus some of its start) + new text. None = no overlap (more than 4 KB came, or the kernel restarted):
    # all of now is new, with a gap before it.
    if not prev:
        return now
    for k in range(len(prev)):
        if now.startswith(prev[k:]):
            return now[len(prev) - k:]
    return None
