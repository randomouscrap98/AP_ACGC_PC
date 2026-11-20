# Animal Crossing GameCube client (0.7.0, phase 2 probe): connects the GC build running in Dolphin to an
# Archipelago server. The game keeps a RAM mailbox (gc/include/gc_ap_mailbox.h); this client finds it by its
# magic, writes received items / checked locations / connection state into it, and sends what the game queues.
# Run from an Archipelago checkout: python -m worlds.animal_crossing.gc_client [--connect host:port]
# Needs dolphin-memory-engine. Only DolphinRAM touches it, so another RAM bridge can replace that class.
import asyncio
import struct
import traceback
from typing import Any, Optional

from CommonClient import ClientCommandProcessor, CommonContext, get_base_parser, gui_enabled, logger, server_loop
from NetUtils import ClientStatus

GAME = "Animal Crossing"
MEM1 = 0x80000000
MEM1_SIZE = 0x1800000

MAGIC = struct.pack(">II", 0x41434150, 0x4D424F58)  # "ACAP" "MBOX"
TAIL = 0x454E4421  # "END!"
VERSION = 1  # GC_AP_MAILBOX_VERSION

# Header fields: u32 each, in the order of gc_ap_mailbox (index * 4 = offset)
H_VERSION, H_SIZE = 2, 3
H_ITEMS_OFF, H_ITEMS_MAX, H_CHECKED_OFF, H_CHECKED_MAX, H_SEND_OFF, H_SEND_MAX = 4, 5, 6, 7, 8, 9
H_SLOTDATA_OFF, H_SLOTDATA_SIZE = 10, 11
H_GAME_TICK, H_SEND_HEAD, H_GOAL, H_PROBE_TRIGGER = 12, 13, 14, 15
H_CLIENT_TICK, H_CLIENT_STATE, H_SEND_TAIL, H_ITEM_COUNT, H_CHECKED_COUNT = 16, 17, 18, 19, 20
H_SLOTDATA_READY, H_TEAM, H_SLOT = 21, 22, 23
H_SEED, SEED_LEN = 24 * 4, 64
HEADER_WORDS = 24
ITEM_SIZE = 8  # gc_ap_item: u32 id, s16 sender, u8 flags, u8 pad

CSTATE_CONNECTING, CSTATE_CONNECTED = 1, 3  # AP_CSTATE_* (ap_archipelago.h)


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
    def __init__(self, ram: DolphinRAM, addr: int, header: tuple[int, ...]) -> None:
        self.ram = ram
        self.addr = addr
        self.h = header

    @staticmethod
    def find(ram: DolphinRAM) -> Optional["Mailbox"]:
        mem = ram.read(MEM1, MEM1_SIZE)
        pos = mem.find(MAGIC)
        while pos >= 0:
            if pos % 4 == 0 and pos + HEADER_WORDS * 4 <= len(mem):
                h = struct.unpack_from(">%dI" % HEADER_WORDS, mem, pos)
                size = h[H_SIZE]
                if h[H_VERSION] == VERSION and pos + size <= len(mem) and \
                        struct.unpack_from(">I", mem, pos + size - 4)[0] == TAIL:
                    return Mailbox(ram, MEM1 + pos, h)
                if h[H_VERSION] != VERSION:
                    logger.warning("Mailbox version %d, this client speaks %d: update the client or the game",
                                   h[H_VERSION], VERSION)
            pos = mem.find(MAGIC, pos + 1)
        return None

    def valid(self) -> bool:
        return self.ram.read(self.addr, 8) == MAGIC

    def get(self, field: int) -> int:
        return struct.unpack(">I", self.ram.read(self.addr + field * 4, 4))[0]

    def put(self, field: int, value: int, signed: bool = False) -> None:
        self.ram.write(self.addr + field * 4, struct.pack(">i" if signed else ">I", value))

    def put_seed(self, seed: str) -> None:
        self.ram.write(self.addr + H_SEED, seed.encode()[:SEED_LEN - 1].ljust(SEED_LEN, b"\0"))

    def read_send(self) -> tuple[list[int], int]:
        head, tail = self.get(H_SEND_HEAD), self.get(H_SEND_TAIL)
        n = self.h[H_SEND_MAX]
        ring = struct.unpack(">%dI" % n, self.ram.read(self.addr + self.h[H_SEND_OFF], n * 4))
        count = min((head - tail) & 0xFFFFFFFF, n)
        return [ring[(tail + i) % n] for i in range(count)], head

    # Appends entries[have:] (the game already has the first `have`), then the new count
    def append_items(self, items: list[tuple[int, int, int]], have: int) -> None:
        items = items[:self.h[H_ITEMS_MAX]]
        if have >= len(items):
            return
        data = b"".join(struct.pack(">IhBx", i, s, f & 0xFF) for i, s, f in items[have:])
        self.ram.write(self.addr + self.h[H_ITEMS_OFF] + have * ITEM_SIZE, data)
        self.put(H_ITEM_COUNT, len(items))

    def append_checked(self, ids: list[int], have: int) -> None:
        ids = ids[:self.h[H_CHECKED_MAX]]
        if have >= len(ids):
            return
        self.ram.write(self.addr + self.h[H_CHECKED_OFF] + have * 4, struct.pack(">%dI" % (len(ids) - have),
                                                                                  *ids[have:]))
        self.put(H_CHECKED_COUNT, len(ids))


class ACGCCommandProcessor(ClientCommandProcessor):
    def _cmd_dolphin(self) -> None:
        """Dolphin / mailbox status"""
        self.output(self.ctx.dolphin_status)

    def _cmd_probe_favor(self) -> None:
        """Probe only: the game acts as if a favor were done (sends Favor 1)"""
        if self.ctx.mbox:
            self.ctx.mbox.put(H_PROBE_TRIGGER, 1)
            self.output("probe_trigger set")
        else:
            self.output("no mailbox yet")


class ACGCContext(CommonContext):
    command_processor = ACGCCommandProcessor
    game = GAME
    items_handling = 0b111  # everything, own items and starting inventory too (as the PC DLL)

    def __init__(self, server_address: Optional[str], password: Optional[str]) -> None:
        super().__init__(server_address, password)
        self.ram: Optional[DolphinRAM] = None
        self.mbox: Optional[Mailbox] = None
        self.dolphin_status = "Not connected to Dolphin yet"
        self.checked_order: list[int] = []  # mailbox order of checked locations (append-only)
        self.checked_known: set[int] = set()  # the same ids, for lookups
        self.mbox_slot_key: Optional[tuple[str, int, int]] = None  # (seed, team, slot) the mailbox holds
        self.client_tick = 0

    async def server_auth(self, password_requested: bool = False) -> None:
        if password_requested and not self.password:
            await super().server_auth(password_requested)
        await self.get_username()
        await self.send_connect()

    def on_package(self, cmd: str, args: dict[str, Any]) -> None:
        if cmd == "RoomInfo":
            self.seed_name = args["seed_name"]  # CommonContext leaves this to the game client
        if cmd in ("Connected", "RoomUpdate"):
            self.watcher_event.set()

    def sync(self) -> None:
        m = self.mbox
        self.client_tick = (self.client_tick + 1) & 0xFFFFFFFF or 1
        m.put(H_CLIENT_TICK, self.client_tick)
        connected = self.slot is not None and self.seed_name is not None
        m.put(H_CLIENT_STATE, CSTATE_CONNECTED if connected else CSTATE_CONNECTING, signed=True)
        if not connected:
            return
        key = (self.seed_name, self.team or 0, self.slot)
        if key != self.mbox_slot_key:
            # Other slot (or first sync): start the lists over
            m.put(H_ITEM_COUNT, 0)
            m.put(H_CHECKED_COUNT, 0)
            m.put(H_TEAM, key[1], signed=True)
            m.put(H_SLOT, key[2], signed=True)
            m.put_seed(key[0])
            self.mbox_slot_key = key
            self.checked_order, self.checked_known = [], set()
        for loc in sorted(self.checked_locations - self.checked_known):
            self.checked_order.append(loc)
            self.checked_known.add(loc)
        # The game side is the truth for what it has (a game restart zeroes it)
        m.append_items([(i.item, i.player, i.flags) for i in self.items_received], m.get(H_ITEM_COUNT))
        m.append_checked(self.checked_order, m.get(H_CHECKED_COUNT))
        ids, head = m.read_send()
        if ids:
            new = [i for i in ids if i in self.missing_locations or i in self.checked_locations]
            if len(new) != len(ids):
                logger.warning("Game sent unknown location ids: %s", [i for i in ids if i not in new])
            self.locations_checked.update(new)
            asyncio.create_task(self.check_locations(new))
            m.put(H_SEND_TAIL, head)
        if m.get(H_GOAL) and not self.finished_game:
            asyncio.create_task(self.send_msgs([{"cmd": "StatusUpdate", "status": ClientStatus.CLIENT_GOAL}]))
            self.finished_game = True


async def dolphin_sync_task(ctx: ACGCContext) -> None:
    ctx.ram = DolphinRAM()
    while not ctx.exit_event.is_set():
        sleep = 0.1
        try:
            if not ctx.ram.hook():
                ctx.mbox = None
                ctx.dolphin_status = "Dolphin not found (start it with the patched game)"
                sleep = 2
            elif ctx.mbox is None or not ctx.mbox.valid():
                ctx.mbox = Mailbox.find(ctx.ram)
                if ctx.mbox:
                    logger.info("Found the game's mailbox at %08X", ctx.mbox.addr)
                    ctx.dolphin_status = "Connected to the game in Dolphin"
                    ctx.mbox_slot_key = None
                else:
                    ctx.dolphin_status = "Dolphin is running, but no AP game found in it (yet)"
                    sleep = 1
            else:
                ctx.sync()
        except Exception:
            logger.error(traceback.format_exc())
            ctx.mbox = None
            ctx.ram.unhook()
            sleep = 2
        try:
            await asyncio.wait_for(ctx.watcher_event.wait(), sleep)
        except asyncio.TimeoutError:
            pass
        ctx.watcher_event.clear()


def main(*args: str) -> None:
    async def _main(connect: Optional[str], password: Optional[str]) -> None:
        ctx = ACGCContext(connect, password)
        ctx.server_task = asyncio.create_task(server_loop(ctx), name="ServerLoop")
        if gui_enabled:
            ctx.run_gui()
        ctx.run_cli()
        sync_task = asyncio.create_task(dolphin_sync_task(ctx), name="DolphinSync")
        await ctx.exit_event.wait()
        ctx.watcher_event.set()
        ctx.server_address = None
        await ctx.shutdown()
        await sync_task

    parsed = get_base_parser().parse_args(args)
    asyncio.run(_main(parsed.connect, parsed.password))


if __name__ == "__main__":
    import sys
    main(*sys.argv[1:])
