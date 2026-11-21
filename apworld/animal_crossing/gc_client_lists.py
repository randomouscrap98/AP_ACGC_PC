# The GC client's Items and Checks tabs, without any UI: rows built from the client context, shown by
# gc_client_kvui.py (kvui) and gc_client_aplite.py (aplite). Same lists as the PC text client's tabs
# (apclient/src/ap_ui_lists.cpp).
from dataclasses import dataclass, field
from typing import Any

# Item class colours (hex, as Archipelago's text client): progression, useful, trap, filler
PROGRESSION, USEFUL, TRAP = 0b001, 0b010, 0b100
COLOR_PROGRESSION, COLOR_USEFUL, COLOR_TRAP, COLOR_FILLER = "AF99EF", "6D8BE8", "FA8072", "00EEEE"
COLOR_LOCATION, COLOR_DONE = "00FF7F", "808080"  # unchecked check / received item's location; checked / none


def item_color(flags: int) -> str:
    if flags & PROGRESSION:
        return COLOR_PROGRESSION
    if flags & USEFUL:
        return COLOR_USEFUL
    if flags & TRAP:
        return COLOR_TRAP
    return COLOR_FILLER


@dataclass
class ItemRow:
    number: int  # 1 = first received
    item: str
    sender: str
    location: str
    flags: int
    has_location: bool  # False: starting inventory / sent by the server


@dataclass
class Check:
    id: int
    name: str
    checked: bool


@dataclass
class CheckGroup:
    name: str
    checks: list[Check] = field(default_factory=list)  # by id (Loan 2 before Loan 10)
    missing: int = 0


def version(ctx: Any) -> tuple:
    # Changes whenever the lists would: views rebuild only then
    return (ctx.slot, len(ctx.items_received), len(ctx.checked_locations), len(ctx.missing_locations))


def item_rows(ctx: Any) -> list[ItemRow]:
    # Newest first. No slot (disconnected): lookup_in_slot would assert
    if ctx.slot is None:
        return []
    rows = []
    for i, it in enumerate(ctx.items_received):
        has_location = it.location >= 0 and it.player != 0
        rows.append(ItemRow(
            number=i + 1,
            item=ctx.item_names.lookup_in_slot(it.item, ctx.slot),
            sender=ctx.player_names.get(it.player, f"Player {it.player}") if it.player else "Archipelago",
            location=ctx.location_names.lookup_in_slot(it.location, it.player) if has_location else "-",
            flags=it.flags,
            has_location=has_location,
        ))
    rows.reverse()
    return rows


def item_matches(row: ItemRow, query: str) -> bool:
    q = query.lower()
    return not q or q in row.item.lower() or q in row.sender.lower() or q in row.location.lower()


# The group of a location name: the part before ":" ("Catalog: lamp"), else the name without a trailing number
# ("Starting Loan 12"), else its first word ("Donate tricera skull")
def group_of(name: str) -> str:
    if ":" in name:
        return name.split(":", 1)[0]
    stripped = name.rstrip("0123456789")
    if stripped != name and stripped.endswith(" ") and stripped.strip():
        return stripped[:-1]
    return name.split(" ", 1)[0]


def check_groups(ctx: Any) -> list[CheckGroup]:
    # Groups in the order of their first location id. No slot (disconnected): the server reset keeps the
    # location lists but lookup_in_slot would assert
    if ctx.slot is None:
        return []
    checks = [Check(i, ctx.location_names.lookup_in_slot(i, ctx.slot), True) for i in ctx.checked_locations]
    checks += [Check(i, ctx.location_names.lookup_in_slot(i, ctx.slot), False) for i in ctx.missing_locations]
    checks.sort(key=lambda c: c.id)
    groups: dict[str, CheckGroup] = {}
    for c in checks:
        g = groups.setdefault(group_of(c.name), CheckGroup(group_of(c.name)))
        g.checks.append(c)
        if not c.checked:
            g.missing += 1
    return list(groups.values())


def check_shown(c: Check, query: str, show_done: bool) -> bool:
    return (show_done or not c.checked) and (not query or query.lower() in c.name.lower())


def summary(groups: list[CheckGroup]) -> str:
    total = sum(len(g.checks) for g in groups)
    missing = sum(g.missing for g in groups)
    if total == 0:
        return "No locations yet (connect to the server)."
    return f"{missing} checks left of {total} ({100.0 * (total - missing) / total:.1f}%)"
