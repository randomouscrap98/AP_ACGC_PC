# Animal Crossing GameCube client (0.7.0): connects the GC build running in Dolphin to an Archipelago server.
# The game keeps a RAM mailbox (gc/include/gc_ap_mailbox.h); this client finds it by its magic, writes slot_data
# (gc_slotdata.py), slot names, received items, checked locations, toasts and the connection state into it, and
# sends what the game queues.
# Run from an Archipelago checkout: python -m worlds.animal_crossing.gc_client [--connect host:port]
# Needs dolphin-memory-engine. Only DolphinRAM touches it, so another RAM bridge can replace that class.
# Real Wii (Nintendont fork "Animal Crossing AP"): wii_task finds its start screen and names the memory card, then
# game_sync_task reaches the game's RAM through the fork's kernel server (gc_wii.NintendontRAM).
import asyncio
import ipaddress
import json
import os
import struct
import time
import traceback
from typing import Any, Iterator, Optional

from . import gc_slotdata, gc_wii
from .gc_link import GameLink
from CommonClient import ClientCommandProcessor, CommonContext, get_base_parser, gui_enabled, logger, server_loop
from NetUtils import ClientStatus

GAME = "Animal Crossing"
MEM1 = 0x80000000
MEM1_SIZE = 0x1800000

MAGIC = struct.pack(">II", 0x41434150, 0x4D424F58)  # "ACAP" "MBOX"
TAIL = 0x454E4421  # "END!"
VERSION = 3  # GC_AP_MAILBOX_VERSION

# Header fields: u32 each, in the order of gc_ap_mailbox (index * 4 = offset); tests/test_gc_mailbox checks them
H_VERSION, H_SIZE = 2, 3
H_ITEMS_OFF, H_ITEMS_MAX, H_CHECKED_OFF, H_CHECKED_MAX, H_SEND_OFF, H_SEND_MAX = 4, 5, 6, 7, 8, 9
H_SLOTDATA_OFF, H_SLOTDATA_SIZE = 10, 11
H_SLOTDATA_CHECK, SD_CHECKS = 12, 8
H_TOASTS_OFF, H_TOAST_MAX, H_TOAST_LEN, H_NAMES_OFF, H_NAME_MAX, H_NAME_LEN = 20, 21, 22, 23, 24, 25
H_GAME_TICK, H_SEND_HEAD, H_GOAL, H_TOAST_TAIL = 26, 27, 28, 29
H_CLIENT_TICK, H_CLIENT_STATE, H_SEND_TAIL, H_ITEM_COUNT, H_CHECKED_COUNT = 30, 31, 32, 33, 34
H_SLOTDATA_READY, H_TOAST_HEAD, H_TEAM, H_SLOT = 35, 36, 37, 38
H_GSET_OFF, H_GSET_MAX, H_GSET_GAME_GEN, H_GSET_CLIENT_GEN = 39, 40, 41, 42
HEADER_WORDS = 43
H_SEED, SEED_LEN = HEADER_WORDS * 4, 64
H_STATUS, STATUS_LEN = H_SEED + SEED_LEN, 96
H_HOST, HOST_LEN = H_STATUS + STATUS_LEN, 64
H_SLOTNAME, SLOTNAME_LEN = H_HOST + HOST_LEN, 17
ITEM_SIZE = 8  # gc_ap_item: u32 id, s16 sender, u8 flags, u8 pad

# The game's global settings (Settings > Gameplay / Overlay; no settings file on a console), by mailbox index:
# GC_AP_GSET_* order, append-only. Stored by name in GSET_FILE.
GSET_NAMES = ["no_cockroaches", "no_fishing_trash", "easier_critter_catch", "hidden_bug_hints", "hidden_item_hints",
              "short_shop_talk", "ap_status_always", "ap_tracker_always", "ap_toast_seconds", "ap_toast_max",
              "disable_resetti", "instant_text", "bite_sound", "less_mail", "map_divination", "planting_marks",
              "screen_margin"]
GSET_FILE = "animal_crossing_gc_settings.json"  # also holds WII_ADDRESS_KEY
WII_ADDRESS_KEY = "wii_address"  # set with /wii: asked directly besides the broadcast
WII_LAST_KEY = "wii_last"  # the last Wii that took a memory card name (its game is reached there)

CSTATE_CONNECTING, CSTATE_CONNECTED, CSTATE_SLOTREFUSED = 1, 3, -1  # AP_CSTATE_* (ap_archipelago.h)
CTRL_WHITE, CTRL_GREEN, CTRL_RED = "\x01", "\x02", "\x03"  # AP_CTRL_* overlay colours


class DolphinRAM:
    # The RAM bridge: Dolphin through dolphin_memory_engine
    def __init__(self) -> None:
        import dolphin_memory_engine
        self.dme = dolphin_memory_engine

    def hook(self) -> bool:
        if not self.dme.is_hooked():
            self.dme.hook()
        return self.dme.is_hooked()

    def unhook(self) -> None:
        self.dme.un_hook()

    def read(self, addr: int, size: int) -> bytes:
        return self.dme.read_bytes(addr, size)

    def write(self, addr: int, data: bytes) -> None:
        self.dme.write_bytes(addr, data)


class Mailbox:
    warned: set[int] = set()  # mailbox versions already warned about

    def __init__(self, ram: Any, addr: int, header: tuple[int, ...]) -> None:
        self.ram = ram
        self.addr = addr
        self.h = header

    @staticmethod
    def check(ram: Any, addr: int) -> Optional["Mailbox"]:
        # The mailbox at addr if its header is whole and this client's version
        h = struct.unpack(">%dI" % HEADER_WORDS, ram.read(addr, HEADER_WORDS * 4))
        size = h[H_SIZE]
        if h[H_VERSION] == VERSION and 0 < size <= MEM1 + MEM1_SIZE - addr and \
                struct.unpack(">I", ram.read(addr + size - 4, 4))[0] == TAIL:
            return Mailbox(ram, addr, h)
        if h[H_VERSION] != VERSION and h[H_VERSION] not in Mailbox.warned:
            Mailbox.warned.add(h[H_VERSION])
            logger.warning("Mailbox version %d, this client speaks %d: update the client or the game",
                           h[H_VERSION], VERSION)
        return None

    @staticmethod
    def find(ram: Any, hint: int = 0) -> Optional["Mailbox"]:
        # hint: where it was last time (a reconnect to the same game skips the search)
        if hint:
            try:
                found = Mailbox.check(ram, hint)
                if found:
                    return found
            except OSError:
                raise
            except Exception:
                pass
        if hasattr(ram, "find_magic"):
            # The Wii searches its own RAM (24 MB over Wi-Fi would take too long)
            addr = ram.find_magic(MEM1)
            while addr:
                found = Mailbox.check(ram, addr)
                if found:
                    return found
                addr = ram.find_magic(addr + 4)
            return None
        mem = ram.read(MEM1, MEM1_SIZE)
        pos = mem.find(MAGIC)
        while pos >= 0:
            if pos % 4 == 0 and pos + HEADER_WORDS * 4 <= len(mem):
                h = struct.unpack_from(">%dI" % HEADER_WORDS, mem, pos)
                size = h[H_SIZE]
                if h[H_VERSION] == VERSION and pos + size <= len(mem) and \
                        struct.unpack_from(">I", mem, pos + size - 4)[0] == TAIL:
                    return Mailbox(ram, MEM1 + pos, h)
                if h[H_VERSION] != VERSION and h[H_VERSION] not in Mailbox.warned:
                    Mailbox.warned.add(h[H_VERSION])
                    logger.warning("Mailbox version %d, this client speaks %d: update the client or the game",
                                   h[H_VERSION], VERSION)
            pos = mem.find(MAGIC, pos + 1)
        return None

    def read_header(self) -> Optional[tuple[int, ...]]:
        # The whole header in one read (each read is a network round trip on a Wii); None = the mailbox is gone
        data = self.ram.read(self.addr, HEADER_WORDS * 4)
        if data[:8] != MAGIC:
            return None
        return struct.unpack(">%dI" % HEADER_WORDS, data)

    def get(self, field: int) -> int:
        # One header field (test harnesses, tools); sync uses read_header
        return struct.unpack(">I", self.ram.read(self.addr + field * 4, 4))[0]

    def put(self, field: int, value: int, signed: bool = False) -> None:
        self.ram.write(self.addr + field * 4, struct.pack(">i" if signed else ">I", value))

    def put_text(self, offset: int, size: int, text: str) -> None:
        # Zero-terminated, cut to size - 1 bytes (absolute offset into the mailbox)
        self.ram.write(self.addr + offset, c_text(text, size))

    def put_seed(self, seed: str) -> None:
        self.put_text(H_SEED, SEED_LEN, seed)

    def slotdata_layout(self) -> tuple[int, list[int]]:
        return self.h[H_SLOTDATA_SIZE], list(self.h[H_SLOTDATA_CHECK:H_SLOTDATA_CHECK + SD_CHECKS])

    def put_slotdata(self, data: bytes) -> None:
        self.ram.write(self.addr + self.h[H_SLOTDATA_OFF], data)

    def put_names(self, names: dict[int, str]) -> None:
        n, size = self.h[H_NAME_MAX], self.h[H_NAME_LEN]
        data = b"".join(c_text(names.get(slot, ""), size) for slot in range(n))
        self.ram.write(self.addr + self.h[H_NAMES_OFF], data)

    def put_tick_state(self, tick: int, state: int) -> None:
        # client_tick and client_state are neighbours: one write
        self.ram.write(self.addr + H_CLIENT_TICK * 4, struct.pack(">Ii", tick, state))

    # The game shows up to toast_head; a full ring overwrites the oldest (the game skips ahead). head: the current
    # toast_head (only this client writes it)
    def push_toasts(self, texts: list[str], head: int) -> None:
        n, size = self.h[H_TOAST_MAX], self.h[H_TOAST_LEN]
        for text in texts[-n:]:
            self.ram.write(self.addr + self.h[H_TOASTS_OFF] + (head % n) * size, c_text(text, size))
            head = (head + 1) & 0xFFFFFFFF
        self.put(H_TOAST_HEAD, head)

    def read_send(self, head: int, tail: int) -> list[int]:
        n = self.h[H_SEND_MAX]
        ring = struct.unpack(">%dI" % n, self.ram.read(self.addr + self.h[H_SEND_OFF], n * 4))
        count = min((head - tail) & 0xFFFFFFFF, n)
        return [ring[(tail + i) % n] for i in range(count)]

    # Appends entries[have:] (the game already has the first `have`), then the new count
    def append_items(self, items: list[tuple[int, int, int]], have: int) -> None:
        items = items[:self.h[H_ITEMS_MAX]]
        if have >= len(items):
            return
        data = b"".join(struct.pack(">IhBx", i, s, f & 0xFF) for i, s, f in items[have:])
        self.ram.write(self.addr + self.h[H_ITEMS_OFF] + have * ITEM_SIZE, data)
        self.put(H_ITEM_COUNT, len(items))

    def get_gsettings(self) -> dict[str, int]:
        n = min(self.h[H_GSET_MAX], len(GSET_NAMES))
        values = struct.unpack(">%di" % n, self.ram.read(self.addr + self.h[H_GSET_OFF], n * 4))
        return dict(zip(GSET_NAMES, values))

    def put_gsettings(self, stored: dict[str, int]) -> None:
        # Names the game knows; the others keep the game's value
        n = min(self.h[H_GSET_MAX], len(GSET_NAMES))
        for i, name in enumerate(GSET_NAMES[:n]):
            if isinstance(stored.get(name), int):
                self.ram.write(self.addr + self.h[H_GSET_OFF] + i * 4, struct.pack(">i", stored[name]))

    def append_checked(self, ids: list[int], have: int) -> None:
        ids = ids[:self.h[H_CHECKED_MAX]]
        if have >= len(ids):
            return
        self.ram.write(self.addr + self.h[H_CHECKED_OFF] + have * 4, struct.pack(">%dI" % (len(ids) - have),
                                                                                  *ids[have:]))
        self.put(H_CHECKED_COUNT, len(ids))


def gsettings_path() -> str:
    return user_file(GSET_FILE)


def load_gsettings() -> dict[str, Any]:
    try:
        with open(gsettings_path(), encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}


def save_gsettings(values: dict[str, Any], what: str = "the game's settings") -> None:
    data = load_gsettings()
    data.update(values)  # names this game doesn't know (a newer client's) stay
    path = gsettings_path()
    tmp = path + ".tmp"
    try:
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=1, sort_keys=True)
        os.replace(tmp, path)
        logger.info("Saved %s to %s", what, path)
    except OSError as e:
        logger.error("Couldn't save %s to %s: %s", what, path, e)


def c_text(text: str, size: int) -> bytes:
    # A C char[size]: utf-8 cut to size - 1 bytes (never inside a character), zero-padded
    data = text.encode("utf-8")[:size - 1].decode("utf-8", "ignore").encode("utf-8")
    return data.ljust(size, b"\0")


def plain(text: str) -> str:
    # ap_plain (ap_archipelago.cpp): control characters to spaces, trailing spaces off
    return "".join(" " if ord(c) < 0x20 else c for c in text).rstrip(" ")


def user_file(name: str) -> str:
    try:
        import Utils
        return Utils.user_path(name)
    except ImportError:
        return name


class ACGCCommandProcessor(ClientCommandProcessor):
    def _cmd_dolphin(self) -> None:
        """Game connection status (Dolphin or Wii)"""
        self.output(self.ctx.dolphin_status)

    def _cmd_wii(self, address: str = "") -> None:
        """Wii status. /wii <address>: also ask that address (networks that drop broadcasts); /wii off: forget it"""
        if address == "off":
            save_gsettings({WII_ADDRESS_KEY: ""}, "the Wii address")
        elif address:
            try:
                ipaddress.IPv4Address(address)
            except ValueError:
                self.output(f"Not an IPv4 address: {address} (the Wii shows its address on its start screen)")
                return
            save_gsettings({WII_ADDRESS_KEY: address}, "the Wii address")
        saved = wii_saved_address()
        self.output(self.ctx.wii_status or "No Wii found (start Animal Crossing AP on the Wii)")
        if saved:
            self.output(f"Also asking {saved} directly (/wii off to stop)")


class ACGCContext(CommonContext):
    command_processor = ACGCCommandProcessor
    game = GAME
    items_handling = 0b111  # everything, own items and starting inventory too (as the PC DLL)

    def __init__(self, server_address: Optional[str], password: Optional[str]) -> None:
        super().__init__(server_address, password)
        self.ram: Any = None  # DolphinRAM or gc_wii.NintendontRAM, whichever has the game
        self.loop: Optional[asyncio.AbstractEventLoop] = None  # set by game_sync_task
        self.mbox: Optional[Mailbox] = None
        self.dolphin_status = ""  # set_dolphin_status
        self.checked_order: list[int] = []  # mailbox order of checked locations (append-only)
        self.checked_known: set[int] = set()  # the same ids, for lookups
        self.mbox_slot_key: Optional[tuple[str, int, int]] = None  # (seed, team, slot) the mailbox holds
        self.client_tick = 0
        self.slot_data: Optional[dict[str, Any]] = None
        self.toasts: list[str] = []  # waiting for the mailbox
        self.gset_client_gen = 0  # gset_client_gen we last wrote (the game zeroes it when it restarts)
        self.gset_game_gen: Optional[int] = None  # gset_game_gen last stored (None: not looked yet)
        self.mbox_texts: dict[int, str] = {}  # mailbox text fields as last written (rewritten only on change)
        self.wii_status = ""  # set_wii_status
        self.wii_address = ""  # the Wii last talked to ("" = none seen)
        self.wii_booting = False  # it had the card and a disc image when last seen
        self.wii_warned: set[tuple[str, ...]] = set()  # address sets already reported as "several Wiis"
        self.link = GameLink()  # history + numbers for the connection tab

    # Items / Checks tabs: kvui (make_gui) and aplite (aplite_tabs, which only aplite calls). Each imports its UI
    # library only here: aplite has no kivy, upstream no Textual.
    def make_gui(self) -> type:
        from .gc_client_kvui import manager_class
        return manager_class()

    def aplite_tabs(self) -> list[tuple[str, Any]]:
        from .gc_client_aplite import tabs
        return tabs(self)

    def set_dolphin_status(self, text: str) -> None:
        # /dolphin's answer; a change is logged once (waiting at the start, connected, lost), never repeated
        if text != self.dolphin_status:
            self.link.add(text, main_log=True)
        self.dolphin_status = text

    def set_wii_status(self, text: str) -> None:
        if text != self.wii_status:
            self.link.add(text, main_log=True)
        self.wii_status = text

    # One round of wii_task with the Wiis that answered: picks one and names its card once the slot is known
    async def wii_step(self, found: list[gc_wii.WiiReply]) -> None:
        saved = wii_saved_address()
        wii = next((r for r in found if r.address == saved), None) or next(
            (r for r in found if r.address == self.wii_address), None) or (found[0] if found else None)
        if wii is None:
            if self.wii_address:
                self.set_wii_status(f"The Wii at {self.wii_address} is starting the game" if self.wii_booting else
                                    f"Lost the Wii at {self.wii_address}")
            return
        addresses = tuple(sorted(r.address for r in found))
        if len(found) > 1 and addresses not in self.wii_warned:
            self.wii_warned.add(addresses)
            logger.warning("Several Wiis answer (%s): using %s; /wii <address> picks another",
                           ", ".join(addresses), wii.address)
        self.wii_address = wii.address
        self.wii_booting = False
        if self.slot is None or self.seed_name is None:
            self.set_wii_status(f"Found the Wii at {wii.address}: connect to the AP server to name its memory card")
            return
        name = gc_wii.card_name(self.seed_name, self.team or 0, self.slot)
        if wii.card != name:
            ok, answer = await asyncio.to_thread(gc_wii.send_card, wii.address, name)
            if not ok:
                self.set_wii_status(f"The Wii at {wii.address} didn't take the memory card name: {answer}")
                return
            if wii_last_address() != wii.address:
                save_gsettings({WII_LAST_KEY: wii.address}, "the Wii's address")
        if wii.game_ok:
            self.wii_booting = True
            self.set_wii_status(f"The Wii at {wii.address} has memory card {name} and starts the game")
        else:
            self.set_wii_status(f"The Wii at {wii.address} has memory card {name}: pick the disc image on the Wii (A)")

    async def server_auth(self, password_requested: bool = False) -> None:
        if password_requested and not self.password:
            await super().server_auth(password_requested)
        await self.get_username()
        await self.send_connect()

    def on_package(self, cmd: str, args: dict[str, Any]) -> None:
        if cmd == "RoomInfo":
            self.seed_name = args["seed_name"]  # CommonContext leaves this to the game client
        if cmd == "Connected":
            self.slot_data = args.get("slot_data") or {}
            self.mbox_slot_key = None  # rewrite slot_data and names
        if cmd in ("Connected", "RoomUpdate"):
            self.watcher_event.set()

    def on_print_json(self, args: dict[str, Any]) -> None:
        super().on_print_json(args)
        text = self.toast_text(args)
        if text:
            self.toasts.append(text)
            self.watcher_event.set()

    # The DLL's toast lines (ap_archipelago.cpp print_json handler): items and hints to or from this slot, and
    # everyone's goals
    def toast_text(self, args: dict[str, Any]) -> Optional[str]:
        kind = args.get("type")
        if self.slot is None or kind not in ("ItemSend", "Hint", "Goal"):
            return None

        def player(slot: int) -> str:
            return CTRL_GREEN + plain(self.player_names.get(slot, f"Player {slot}")) + CTRL_WHITE

        if kind == "Goal":
            return player(args["slot"]) + " completed their goal!" if "slot" in args else None
        item, receiver = args.get("item"), args.get("receiving")
        if item is None or receiver is None:
            return None
        finder = item.player
        if receiver != self.slot and finder != self.slot:
            return None
        game = self.slot_info[receiver].game if receiver in self.slot_info else self.game
        name = CTRL_RED + plain(self.item_names.lookup_in_game(item.item, game)) + CTRL_WHITE
        if kind == "Hint":
            return f"Hint: {player(receiver)}'s {name} is in {player(finder)}'s world"
        if finder == receiver:
            return f"{player(finder)} found their {name}"
        return f"{player(finder)} sent {name} to {player(receiver)}"

    # slot_data into the mailbox; False (with the reason in the mailbox status) if the game can't take it
    def write_slotdata(self, m: Mailbox) -> bool:
        size, offsets = m.slotdata_layout()
        if not gc_slotdata.layout_matches(size, offsets):
            m.put_text(H_STATUS, STATUS_LEN, "Client and game versions don't match: update both")
            logger.error("The game's slot_data layout (size %d, offsets %s) isn't this client's: update both",
                         size, offsets)
            return False
        try:
            data = gc_slotdata.build(self.slot_data or {})
        except ValueError as e:
            m.put_text(H_STATUS, STATUS_LEN, str(e))
            logger.error("%s", e)
            return False
        m.put_slotdata(data)
        m.put_text(H_STATUS, STATUS_LEN, "")
        return True

    # The game's global settings: stored here, written back at every game start; no server needed.
    # fresh: this client just came, or the game started again (sync)
    def sync_gsettings(self, m: Mailbox, h: tuple[int, ...], fresh: bool) -> None:
        game_gen = h[H_GSET_GAME_GEN]
        if fresh:
            # What the player applied in the game before wins, else the stored values go in
            if game_gen != 0:
                save_gsettings(m.get_gsettings())
            m.put_gsettings(load_gsettings())
            self.gset_client_gen = (h[H_GSET_CLIENT_GEN] + 1) & 0xFFFFFFFF or 1
            m.put(H_GSET_CLIENT_GEN, self.gset_client_gen)
        elif game_gen != self.gset_game_gen:
            save_gsettings(m.get_gsettings())  # Apply in the game
        self.gset_game_gen = game_gen

    def put_text_changed(self, m: Mailbox, offset: int, size: int, text: str) -> None:
        if self.mbox_texts.get(offset) != text:
            m.put_text(offset, size, text)
            self.mbox_texts[offset] = text

    def run_soon(self, coro: Any) -> None:
        # sync() runs in a worker thread (the Wii's RAM is blocking network calls): hand coroutines to the loop
        asyncio.run_coroutine_threadsafe(coro, self.loop)

    # One round with the game. Each RAM access is a network round trip on a Wii (~44 ms), so an idle round is two:
    # the header read and the tick + state write; the rest only when something changed. False = the mailbox is gone.
    def sync(self) -> bool:
        m = self.mbox
        h = m.read_header()
        if h is None:
            return False
        # gset_client_gen only changes through this client, and the game zeroes it when it starts (a savestate
        # brings an old one back): a mismatch = the game started over, so everything is written again
        fresh = self.gset_game_gen is None or h[H_GSET_CLIENT_GEN] != self.gset_client_gen
        if fresh:
            self.mbox_slot_key = None
            self.mbox_texts = {}
        self.sync_gsettings(m, h, fresh)
        self.client_tick = (self.client_tick + 1) & 0xFFFFFFFF or 1
        connected = self.slot is not None and self.seed_name is not None and self.slot_data is not None
        host = self.server_address or ""
        self.put_text_changed(m, H_HOST, HOST_LEN, host.split("://")[-1])
        self.put_text_changed(m, H_SLOTNAME, SLOTNAME_LEN, self.auth or "")
        if not connected:
            m.put_tick_state(self.client_tick, CSTATE_CONNECTING)
            return True
        key = (self.seed_name, self.team or 0, self.slot)
        item_have, checked_have = h[H_ITEM_COUNT], h[H_CHECKED_COUNT]
        if key != self.mbox_slot_key:
            # Other slot (or first sync): slot_data and names first, then the lists start over
            m.put(H_SLOTDATA_READY, 0)
            if not self.write_slotdata(m):
                m.put_tick_state(self.client_tick, CSTATE_SLOTREFUSED)
                return True
            m.put_names(self.player_names)
            m.put(H_ITEM_COUNT, 0)
            m.put(H_CHECKED_COUNT, 0)
            m.put(H_TEAM, key[1], signed=True)
            m.put(H_SLOT, key[2], signed=True)
            m.put_seed(key[0])
            m.put(H_SLOTDATA_READY, 1)
            self.mbox_slot_key = key
            self.checked_order, self.checked_known = [], set()
            item_have = checked_have = 0
        m.put_tick_state(self.client_tick, CSTATE_CONNECTED)
        if self.toasts:
            m.push_toasts(self.toasts, h[H_TOAST_HEAD])
            self.toasts.clear()
        for loc in sorted(self.checked_locations - self.checked_known):
            self.checked_order.append(loc)
            self.checked_known.add(loc)
        # The game side is the truth for what it has (a game restart zeroes it)
        m.append_items([(i.item, i.player, i.flags) for i in self.items_received], item_have)
        m.append_checked(self.checked_order, checked_have)
        head, tail = h[H_SEND_HEAD], h[H_SEND_TAIL]
        if head != tail:
            ids = m.read_send(head, tail)
            known = [i for i in ids if i in self.missing_locations or i in self.checked_locations]
            if len(known) != len(ids):
                logger.warning("Game sent unknown location ids: %s", [i for i in ids if i not in known])
            # The game queues a check again every tick until the server confirms it: send each id once
            # (CommonClient sends all of locations_checked again after a reconnect)
            new = [i for i in known if i not in self.locations_checked]
            if new:
                self.locations_checked.update(new)
                self.run_soon(self.check_locations(new))
            m.put(H_SEND_TAIL, head)
        if h[H_GOAL] and not self.finished_game:
            self.run_soon(self.send_msgs([{"cmd": "StatusUpdate", "status": ClientStatus.CLIENT_GOAL}]))
            self.finished_game = True
        return True


# Between syncs. Server events (items, toasts, RoomUpdate) wake the loop at once (watcher_event), so this only paces
# game -> server news (checks, goal, settings Apply): a second is plenty, and on a Wii each sync is network traffic
SYNC_SECONDS = 1.0

WAITING_GAME = "Looking for game on Dolphin or Wii"


def wii_game_address(ctx: ACGCContext) -> str:
    # Where a Wii game's kernel server may be: the Wii this client found, else the one set with /wii, else the last
    # one that took a memory card name (the client may start after the game)
    return ctx.wii_address or wii_saved_address() or wii_last_address()


async def game_sync_task(ctx: ACGCContext) -> None:
    ctx.loop = asyncio.get_running_loop()
    try:
        dolphin: Optional[DolphinRAM] = DolphinRAM()
    except ImportError:
        dolphin = None
        logger.warning("dolphin_memory_engine missing (it comes with Archipelago 0.6.1 and newer): only a Wii "
                       "works. From source: pip install dolphin-memory-engine")
    wii: Optional[gc_wii.NintendontRAM] = None
    where = ""  # "Dolphin" / "the Wii at <address>": what ctx.ram is
    connected = False  # the mailbox was found since the game was last lost: "lost" instead of "waiting"
    last_addr = 0  # where the mailbox was last found

    def candidates() -> Iterator[tuple[Any, str]]:
        # The RAM bridges that answer, one at a time: Dolphin first (a local check), then the Wii's kernel server
        # (only asked when Dolphin didn't have the game: a Wii that's off costs a connect timeout)
        nonlocal wii
        if dolphin is not None and dolphin.hook():
            yield dolphin, "Dolphin"
        address = wii_game_address(ctx)
        if address:
            if wii is None or wii.address != address:
                if wii is not None:
                    wii.unhook()
                wii = gc_wii.NintendontRAM(address)
            if wii.hook():
                yield wii, f"the Wii at {address}"

    def find_game() -> tuple[Any, str, Optional[Mailbox]]:
        # The first bridge whose RAM holds the AP game's mailbox, so Dolphin running another game doesn't keep the
        # client from the Wii. None found: the first bridge that answered (for the "waiting" status), or nothing.
        first: tuple[Any, str, Optional[Mailbox]] = (None, "", None)
        for ram, name in candidates():
            try:
                mbox = Mailbox.find(ram, last_addr)
            except OSError:
                ram.unhook()  # went away while searching: the next round tries again
                continue
            if mbox:
                return ram, name, mbox
            if first[0] is None:
                first = (ram, name, None)
        return first

    while not ctx.exit_event.is_set():
        sleep = SYNC_SECONDS
        try:
            if ctx.ram is not None and not await asyncio.to_thread(ctx.ram.hook):
                ctx.ram = ctx.mbox = None
            found_now = False
            if ctx.mbox is None:
                ctx.ram, where, ctx.mbox = await asyncio.to_thread(find_game)
                found_now = ctx.mbox is not None
            if ctx.ram is None:
                lost = "Lost the game, waiting for it again"
                if connected or ctx.dolphin_status != lost:  # "lost" stays until something changes
                    ctx.set_dolphin_status(lost if connected else WAITING_GAME)
                connected = False
                sleep = 2
            elif found_now:
                ctx.link.finds += 1
                ctx.link.connected_at = time.monotonic()
                last_addr = ctx.mbox.addr
                logger.debug("Found the game's mailbox at %08X", ctx.mbox.addr)
                ctx.set_dolphin_status(f"Connected to the game in {where}")
                connected = True
                ctx.mbox_slot_key = None
                ctx.gset_game_gen = None
            elif ctx.mbox is None:
                ctx.set_dolphin_status(f"Lost the game in {where}, waiting for it again" if connected else
                                       f"{where[0].upper() + where[1:]} found, waiting for the AP game")
                sleep = 1
            else:
                start = time.monotonic()
                if not await asyncio.to_thread(ctx.sync):
                    ctx.mbox = None  # gone (the game restarted or quit): the next round looks for it again
                    sleep = 0
                ctx.link.syncs.append(time.monotonic() - start)
                if ctx.link.report_due():
                    timing = ctx.link.take_timing()
                    if isinstance(ctx.ram, gc_wii.NintendontRAM):  # Dolphin's timings: tab only, not every 30 s
                        ctx.link.add("timing: " + timing)
                        for line in await asyncio.to_thread(ctx.link.kernel_lines, ctx.ram):
                            ctx.link.add(line)
        except Exception as e:
            # The emulator or Wii went away mid-read: only "lost", no traceback; anything else is a bug
            ctx.mbox = None
            ram, ctx.ram = ctx.ram, None
            if ram is not None:
                await asyncio.to_thread(ram.unhook)
            if not isinstance(e, OSError) and ram is not None and await asyncio.to_thread(ram.hook):
                logger.error(traceback.format_exc())
            else:
                ctx.link.errors += 1
                ctx.link.add(f"link error: {e!r}")
                ctx.set_dolphin_status(f"Lost the game ({e or type(e).__name__}), waiting for it again")
                connected = False
            sleep = 2
        try:
            await asyncio.wait_for(ctx.watcher_event.wait(), sleep)
        except asyncio.TimeoutError:
            pass
        ctx.watcher_event.clear()


dolphin_sync_task = game_sync_task  # older name (test harnesses)


def wii_last_address() -> str:
    value = load_gsettings().get(WII_LAST_KEY)
    return value if isinstance(value, str) else ""


def wii_saved_address() -> str:
    value = load_gsettings().get(WII_ADDRESS_KEY)
    return value if isinstance(value, str) else ""


async def wii_task(ctx: ACGCContext) -> None:
    # Asks for the Wii loader every 2 s while no game is connected through Dolphin
    try:
        finder = gc_wii.WiiFinder()
    except OSError as e:
        logger.error("Can't look for a Wii (UDP socket: %s)", e)
        return
    try:
        while not ctx.exit_event.is_set():
            if ctx.mbox is None:
                saved = wii_saved_address()
                finder.probe([saved] if saved else [])
                await asyncio.sleep(0.5)
                try:
                    await ctx.wii_step(finder.replies())
                except Exception:
                    logger.error(traceback.format_exc())
            try:
                await asyncio.wait_for(ctx.exit_event.wait(), 1.5)
            except asyncio.TimeoutError:
                pass
    finally:
        finder.close()


def main(*args: str) -> None:
    async def _main(connect: Optional[str], password: Optional[str]) -> None:
        ctx = ACGCContext(connect, password)
        ctx.server_task = asyncio.create_task(server_loop(ctx), name="ServerLoop")
        if gui_enabled:
            ctx.run_gui()
        ctx.run_cli()
        sync_task = asyncio.create_task(game_sync_task(ctx), name="GameSync")
        wii = asyncio.create_task(wii_task(ctx), name="WiiFinder")
        await ctx.exit_event.wait()
        ctx.watcher_event.set()
        ctx.server_address = None
        await ctx.shutdown()
        await sync_task
        await wii

    import Utils
    Utils.init_logging("AnimalCrossingGCClient", exception_logger="Client")  # logs/AnimalCrossingGCClient_*.txt
    parsed = get_base_parser().parse_args(args)
    asyncio.run(_main(parsed.connect, parsed.password))


if __name__ == "__main__":
    import sys
    main(*sys.argv[1:])
