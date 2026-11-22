# The game link's history and numbers (0.7.0), shown in the GC client's connection tab (gc_client_kvui.py /
# gc_client_aplite.py) and written to the client's log file. No Archipelago imports, so the tab tests run without it.
import collections
import logging
import statistics
import time
from typing import Any, Optional

from . import gc_wii

LINK_REPORT_SECONDS = 30  # sync timings (and on a Wii its kernel log) this often
LINK_LINES = 500  # lines the connection tab keeps (the log file has them all)


class GameLink:
    # The game link's history for the connection tab (gc_client_kvui.py / gc_client_aplite.py) and the client's log
    # file (logs/AnimalCrossingGCClient_*.txt): status changes, link errors, sync timings every LINK_REPORT_SECONDS,
    # and on a Wii the new lines of its kernel log (nintendont/kernel/ap_server.c 'L', fetched over the game
    # connection: the kernel serves one client).
    def __init__(self) -> None:
        self.lines: collections.deque[str] = collections.deque(maxlen=LINK_LINES)  # "HH:MM:SS text"
        self.generation = 0  # +1 per line: the tabs redraw on a change
        self.kernel_prev = ""  # the kernel log as last fetched
        self.syncs: list[float] = []  # seconds per sync since the last report
        self.last_report = time.monotonic()
        self.timing = ""  # the last timing summary (info block)
        self.finds = 0  # times the game's mailbox was found
        self.errors = 0  # link errors
        self.connected_at: Optional[float] = None  # monotonic time the mailbox was found, None = not connected

    def add(self, text: str, main_log: bool = False) -> None:
        # main_log: also in the client's main log (status changes); else the tab and the log file only
        self.lines.append(f"{time.strftime('%H:%M:%S')} {text}")
        self.generation += 1
        if main_log:
            logging.getLogger("Client").info(text)  # CommonClient's logger
        else:
            # Archipelago's file-only logger (CommonClient logs server messages there the same way)
            logging.getLogger("FileLog").info("[Game link] %s", text, extra={"NoStream": True})

    def report_due(self) -> bool:
        return time.monotonic() - self.last_report >= LINK_REPORT_SECONDS

    def take_timing(self) -> str:
        # The sync timings since the last report, as one line; starts the next report
        seconds = time.monotonic() - self.last_report
        if self.syncs:
            ms = sorted(t * 1000 for t in self.syncs)
            self.timing = (f"{len(ms)} syncs in {seconds:.0f} s, median {statistics.median(ms):.0f} ms, "
                           f"worst {ms[-1]:.0f} ms")
        else:
            self.timing = f"no syncs in {seconds:.0f} s"
        self.syncs, self.last_report = [], time.monotonic()
        return self.timing

    def kernel_lines(self, ram: Any) -> list[str]:
        # Blocking (worker thread): the kernel log's lines since the last fetch
        now = ram.log()
        new = gc_wii.log_tail(self.kernel_prev, now)
        lines = []
        if new is None:
            lines.append("kernel: --- earlier lines missed (more than 4 KB, or the Wii restarted) ---")
            new = now
        self.kernel_prev = now
        return lines + ["kernel: " + line for line in new.replace("\r", "").split("\n") if line]
