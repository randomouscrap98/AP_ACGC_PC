# Bell Credits: the filler. Each credit pays the loan down (to 100 owed), is held between loans, then goes to
# savings. All credits together are worth Loan Credit Percent of the total debt; the amounts are set after the
# fill from what was actually placed. Few fillers per Bell drop the Large, then the Modest tier.
# WARN: the client mirrors the ids (PC_AP_ITEM_BELL_CREDIT in pc_ap_credit.h, small = base, then modest, large)!
# Reads: world.loans (loansanity.setup), world.filler_tiers (set in world.create_items from tiers()).
SMALL_BELL_CREDIT = "Small Bell Credit"
MODEST_BELL_CREDIT = "Modest Bell Credit"
LARGE_BELL_CREDIT = "Large Bell Credit"
BELL_CREDIT_ID_BASE = 0x10300  # tier t = base + t; 64 ids reserved (0x10300-0x1033F)
ITEMS = {name: BELL_CREDIT_ID_BASE + t for t, name in enumerate([SMALL_BELL_CREDIT, MODEST_BELL_CREDIT, LARGE_BELL_CREDIT])}

# (value ratio, pool weight), small to large. The Bells per ratio unit are computed per seed.
BELL_CREDITS = {
    SMALL_BELL_CREDIT: (1, 30),
    MODEST_BELL_CREDIT: (3, 60),
    LARGE_BELL_CREDIT: (10, 10),
}
BELLS_ROUND = 100  # amounts are rounded to this


def _total_bells(world) -> float:
    return sum(world.loans) * world.options.loan_credit_percent.value / 100


def tiers(world, fillers: int) -> list[str]:
    # Not enough Bells for the big credits: drop the largest tier while a Small Bell Credit would be under
    # BELLS_ROUND (estimated from the filler count; the real amounts come later, amounts())
    names = list(BELL_CREDITS)
    while len(names) > 1 and fillers > 0:
        weights = sum(BELL_CREDITS[n][1] for n in names)
        units_per_filler = sum(BELL_CREDITS[n][0] * BELL_CREDITS[n][1] for n in names) / weights
        if _total_bells(world) / (fillers * units_per_filler) >= BELLS_ROUND:
            break
        names.pop()
    return names


def filler_name(world) -> str:
    names = world.filler_tiers
    return world.random.choices(names, weights=[BELL_CREDITS[n][1] for n in names])[0]


def amounts(world) -> dict[str, int]:
    # Count the credits this player will actually receive (placed anywhere, plus start inventory), then size them
    # so they add up to loan_credit_percent of the total debt.
    counts = {name: 0 for name in BELL_CREDITS}
    placed = [loc.item for loc in world.multiworld.get_locations() if loc.item]
    placed += world.multiworld.precollected_items[world.player]
    for item in placed:
        if item.player == world.player and item.name in counts:
            counts[item.name] += 1

    units = sum(counts[name] * BELL_CREDITS[name][0] for name in BELL_CREDITS)
    unit = _total_bells(world) / units if units else 0
    result = {}
    for name, (size, _) in BELL_CREDITS.items():
        bells = unit * size
        # Rounded to BELLS_ROUND; below that (only small credits left, very few Bells) the plain amount, at least 1
        if bells >= BELLS_ROUND:
            result[name] = round(bells / BELLS_ROUND) * BELLS_ROUND
        else:
            result[name] = max(1, round(bells))
    return result


def slot_data(world) -> dict:
    return {"bell_credits": amounts(world)}


# Unused hooks (world.py calls every hook of every feature)
LOCATIONS: dict[str, int] = {}


def setup(world) -> None:
    pass


def locations(world) -> list[str]:
    return []


def items(world) -> list[str]:
    return []


def precollected(world) -> list[str]:
    return []


def classify(world, name: str):
    return None


def rules(world) -> None:
    pass
