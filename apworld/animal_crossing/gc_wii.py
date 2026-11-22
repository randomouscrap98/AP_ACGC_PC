# The Wii side of the GameCube client (0.7.0): finds the "Animal Crossing AP" loader (the Nintendont fork in
# nintendont/) on the local network and names the per-seed memory card before the game boots.
# Protocol (nintendont/loader/source/ap_wait.c), both on port WII_PORT:
#   UDP: client -> broadcast "ACAP?\n"; Wii -> client "ACAP 1 <game> <card>\n" (<game> 1 = disc image chosen,
#        <card> the card name so far or "-")
#   TCP: Wii "ACAP 1\n"; client "CARD <name>\n"; Wii "OK\n" or "ERR <reason>\n", then closes
# Plain sockets, no Archipelago imports, so tests can run it against a fake Wii.
import re
import socket
from typing import NamedTuple, Optional

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
            return answer == "OK", answer or "connection closed"
    except OSError as e:
        return False, str(e)
