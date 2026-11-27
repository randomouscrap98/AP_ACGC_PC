# Diagnostics log for the GameCube game (mailbox v6 "diag", gc/gc_ap_diag.c): what the game was doing, for rare
# freezes nobody can watch happen. Writes logs/AnimalCrossingGCDiag_<date>.txt next to the client's own log:
# - the game's event lines (scene, acre, player action, pause menu, input hold, menus, weather, date, AP log);
# - a state line every 30 s;
# - FROZEN (game_tick stopped: the watchdog's record of where the game is, with its call stack), CRASH (the crash
#   record), STUCK? (stick held but the player doesn't move), each with the last two
#   minutes of state lines.
# Addresses are for the map file of the exact patched build (the game state, pc / lr / trace).
import collections
import logging
import os
import struct
import time
from typing import Any, Optional

TRACE = 16  # GC_AP_DIAG_TRACE


def _ctx(name: str) -> list[str]:
    return [name + "_pc", name + "_lr", name + "_sp"] + [f"{name}_trace{i}" for i in range(TRACE)]


# gc_ap_diag's fixed fields in order (gc_ap_mailbox.h), u32 each; tests/test_gc_mailbox checks the offsets
FIELDS = ["scene", "game", "paused", "hold", "reload", "pads", "pad0", "pad1", "pad2", "pad3", "weather",
          "save_weather", "date", "time", "heap", "cost_us", "cost_max_us", "cost_sum_us",
          "anchor_text", "anchor_data", "anchor_bss",
          "play_tick", "pos_x", "pos_y", "pos_z", "action", "block", "submenu", "demo", "wipe",
          "wd_ticks", "stall", "hang_seq", *_ctx("hang"),
          "crash_seq", "crash_error", "crash_dsisr", "crash_dar", "crash_thread", *_ctx("crash"),
          "test", "log_head"]
SIGNED = {"pos_x", "pos_y", "pos_z", "action"}
FMT = ">" + "".join("i" if f in SIGNED else "I" for f in FIELDS)
SIZE = struct.calcsize(FMT)

ERRORS = {2: "DSI (bad data address)", 3: "ISI (bad code address)", 5: "alignment", 6: "program (bad instruction)",
          15: "protection"}

STATE_EVERY = 30.0   # seconds between state lines
HISTORY = 240        # state samples kept for a dump (2 min at 0.5 s)
FROZEN_AFTER = 3.0   # seconds of game_tick standing still before FROZEN
STUCK_AFTER = 5.0    # seconds of stick held / input hold before STUCK?
STICK_HELD = 40      # stick deflection that should move the player


def pad_text(p: int) -> str:
    # pad word: button << 16 | stick_x << 8 | stick_y
    x, y = (p >> 8) & 0xFF, p & 0xFF
    return "%04x(%d,%d)" % (p >> 16, x - 256 if x > 127 else x, y - 256 if y > 127 else y)


def stick(p: int) -> int:
    x, y = (p >> 8) & 0xFF, p & 0xFF
    return max(abs(x - 256 if x > 127 else x), abs(y - 256 if y > 127 else y))


def state_text(tick: int, d: dict[str, int]) -> str:
    return ("tick %d play %d scene %d game %08x pause %d hold %d reload %d pads %d [%s] acre %d,%d pos %d,%d,%d "
            "action %d submenu %d demo %d wipe %d weather %d/%d save %02x date %04d-%02d-%02d %02d:%02d "
            "heap %d cost %d max %d us wd %d stall %d") % (
        tick, d["play_tick"], d["scene"], d["game"], d["paused"], d["hold"], d["reload"], d["pads"],
        " ".join(pad_text(d["pad%d" % i]) for i in range(4)), d["block"] >> 8, d["block"] & 0xFF,
        d["pos_x"], d["pos_y"], d["pos_z"], d["action"], d["submenu"], d["demo"], d["wipe"],
        d["weather"] >> 16, d["weather"] & 0xFFFF, d["save_weather"], d["date"] >> 16, (d["date"] >> 8) & 0xFF,
        d["date"] & 0xFF, d["time"] >> 16, (d["time"] >> 8) & 0xFF, d["heap"], d["cost_us"], d["cost_max_us"],
        d["wd_ticks"], d["stall"])


def where_text(d: dict[str, int], name: str) -> str:
    trace = [d[f"{name}_trace{i}"] for i in range(TRACE)]
    while trace and not trace[-1]:
        trace.pop()
    return "pc %08x lr %08x sp %08x trace %s" % (d[name + "_pc"], d[name + "_lr"], d[name + "_sp"],
                                                 " ".join("%08x" % a for a in trace))


def open_log() -> tuple[logging.Logger, str]:
    try:
        import Utils
        folder = Utils.user_path("logs")
    except ImportError:
        folder = "logs"
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, time.strftime("AnimalCrossingGCDiag_%Y_%m_%d__%H_%M_%S.txt"))
    log = logging.getLogger("AnimalCrossingGCDiag")
    log.propagate = False  # not in the client's own log / console
    log.setLevel(logging.INFO)
    for h in list(log.handlers):
        log.removeHandler(h)
        h.close()
    handler = logging.FileHandler(path, encoding="utf-8")
    handler.setFormatter(logging.Formatter("%(asctime)s %(message)s"))
    log.addHandler(handler)
    return log, path


class DiagLog:
    def __init__(self, log: Optional[logging.Logger] = None) -> None:
        self.path = ""
        if log is None:
            log, self.path = open_log()
        self.log = log
        self.history: collections.deque[str] = collections.deque(maxlen=HISTORY)
        # A game restart shows as game_tick going back (update), so finding the mailbox again changes nothing here
        self.tick: Optional[int] = None
        self.tick_since = 0.0     # when game_tick last moved
        self.moving_hang_seq = -1  # hang_seq when game_tick last moved
        self.wd_ticks = -1         # the watchdog's run count, and when it last changed (it runs once a second)
        self.wd_changed_at = 0.0
        self.frozen = False
        self.hang_seq: Optional[int] = None
        self.hang_pc = 0
        self.crash_seq: Optional[int] = None
        self.log_seen: Optional[int] = None
        self.state_at = 0.0
        self.cost_mark = (0, 0)  # (game_tick, cost_sum_us) at the last state line
        self.play_tick = -1
        self.stuck_since: Optional[float] = None
        self.stuck_pos: Optional[tuple[int, int, int]] = None
        self.stuck_said = False
        self.hold_since: Optional[float] = None
        self.hold_said = False

    def write(self, text: str) -> None:
        self.log.info(text)

    def dump(self, why: str) -> None:
        self.write(f"the last {len(self.history)} states before {why}:")
        for line in self.history:
            self.write("  " + line)

    def wants_ring(self, head: Optional[int]) -> bool:
        # Read the log ring this round: the last diag read showed lines not written here yet
        return head is not None and head != self.log_seen

    def update(self, game_tick: int, diag: bytes, ring: Optional[bytes], log_max: int, log_len: int,
               now: Optional[float] = None) -> int:
        # One sync's reads. Returns log_head (the next round reads the ring when it moved).
        now = time.monotonic() if now is None else now
        d = dict(zip(FIELDS, struct.unpack(FMT, diag[:SIZE])))
        state = state_text(game_tick, d)
        if self.tick is None or game_tick < self.tick:
            self.write("game found (game_tick %d): %s" % (game_tick, state))
            self.write("addresses: text %08x data %08x bss %08x (gc_ap_diag_frame gc_ap_diag_trap gc_ap_mbox_mem)"
                       % (d["anchor_text"], d["anchor_data"], d["anchor_bss"]))
            self.cost_mark = (game_tick, d["cost_sum_us"])
            self.log_seen = None if self.tick is None else 0
            self.hang_seq, self.crash_seq = d["hang_seq"], d["crash_seq"]
            self.tick, self.tick_since = game_tick, now
            self.moving_hang_seq = d["hang_seq"]
            self.state_at = now
        if ring is not None:
            self.lines(ring, d["log_head"], log_max, log_len)
        elif self.log_seen is None:
            self.log_seen = max(d["log_head"] - log_max, 0)  # the first round reads the ring next time
        self.history.append(state)
        if now - self.state_at >= STATE_EVERY:
            self.state_at = now
            frames = game_tick - self.cost_mark[0]
            avg = (d["cost_sum_us"] - self.cost_mark[1]) / frames if frames > 0 else 0.0
            self.cost_mark = (game_tick, d["cost_sum_us"])
            self.write("state %s (diagnostics %.1f us a frame on average)" % (state, avg))

        # Crash: the record is written before the game's handler restarts it (it waits 3 s for us)
        if d["crash_seq"] != self.crash_seq:
            self.crash_seq = d["crash_seq"]
            self.write("CRASH: %s thread %08x dar %08x dsisr %08x %s" % (
                ERRORS.get(d["crash_error"], "error %d" % d["crash_error"]), d["crash_thread"], d["crash_dar"],
                d["crash_dsisr"], where_text(d, "crash")))
            self.dump("CRASH")

        if d["wd_ticks"] != self.wd_ticks:
            self.wd_ticks, self.wd_changed_at = d["wd_ticks"], now
        # Frozen: game_tick stands still; the watchdog says where the game is (a record from this stop only)
        if game_tick != self.tick:
            if self.frozen:
                self.write("game moving again after %.1f s" % (now - self.tick_since))
            self.tick, self.tick_since, self.frozen = game_tick, now, False
            self.moving_hang_seq = d["hang_seq"]
        elif not self.frozen and now - self.tick_since >= FROZEN_AFTER:
            self.frozen = True
            if d["hang_seq"] != self.moving_hang_seq:
                where = "the game is at " + where_text(d, "hang")
            elif now - self.wd_changed_at >= 2.0:
                where = "watchdog silent too (interrupts off: a crash being handled, or a hard hang)"
            else:
                where = "no watchdog record yet"
            self.write("FROZEN: game_tick %d stood still %.1f s; %s" % (game_tick, now - self.tick_since, where))
            self.hang_seq, self.hang_pc = d["hang_seq"], d["hang_pc"]
            self.dump("FROZEN")
        elif self.frozen and d["hang_seq"] != self.hang_seq and d["hang_pc"] != self.hang_pc:
            self.write("still frozen, now at " + where_text(d, "hang"))
            self.hang_pc = d["hang_pc"]
        self.hang_seq = d["hang_seq"]

        self.check_stuck(d, now)
        return d["log_head"]

    def lines(self, ring: bytes, head: int, log_max: int, log_len: int) -> None:
        start = self.log_seen if self.log_seen is not None else max(head - log_max, 0)
        if head - start > log_max:
            self.write("(%d event lines lost: more than %d between two reads)" % (head - start - log_max, log_max))
            start = head - log_max
        for i in range(start, head):
            off = (i % log_max) * log_len
            tick = struct.unpack_from(">I", ring, off)[0]
            text = ring[off + 4:off + log_len].split(b"\0", 1)[0].decode("utf-8", "replace")
            self.write("[%d] %s" % (tick, text))
        self.log_seen = head

    def check_stuck(self, d: dict[str, int], now: float) -> None:
        playing = d["play_tick"] != self.play_tick
        self.play_tick = d["play_tick"]
        # Buttons down when the pause menu closed stay hidden from the game until let go (the rest works): only a
        # note, e.g. an analog trigger resting half pressed
        if d["hold"] and not d["paused"]:
            if self.hold_since is None:
                self.hold_since = now
            elif not self.hold_said and now - self.hold_since >= STUCK_AFTER:
                self.hold_said = True
                self.write("note: buttons %04x hidden from the game for %.0f s since the pause menu (pads %s)" % (
                    d["hold"], now - self.hold_since, " ".join(pad_text(d["pad%d" % i]) for i in range(4))))
        else:
            if self.hold_said:
                self.write("hidden buttons let go")
            self.hold_since, self.hold_said = None, False
        # Stick held in free play, but the player stays put
        free = playing and not d["paused"] and not d["submenu"] and not d["demo"] and not d["wipe"] and \
            d["action"] >= 0
        pushed = any(stick(d["pad%d" % i]) >= STICK_HELD for i in range(min(d["pads"], 4)))
        pos = (d["pos_x"], d["pos_y"], d["pos_z"])
        if free and pushed and pos == self.stuck_pos:
            if self.stuck_since is None:
                self.stuck_since = now
            elif not self.stuck_said and now - self.stuck_since >= STUCK_AFTER:
                self.stuck_said = True
                self.write("STUCK? stick held %.0f s, the player didn't move (action %d, acre %d,%d)" % (
                    now - self.stuck_since, d["action"], d["block"] >> 8, d["block"] & 0xFF))
                self.dump("STUCK? (player doesn't move)")
        else:
            if self.stuck_said:
                self.write("player moving again")
            self.stuck_since, self.stuck_said = None, False
        self.stuck_pos = pos
