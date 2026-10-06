# Location names and ids. The table is static (ids never depend on options); each seed uses a subset.
from .items import MUSEUM_NAMES, MUSEUM_PAINTING, museum_slot

# Loans in the order they happen (house upgrades go Medium -> Basement -> Large -> Upper).
LOANS = ["Starting", "Medium", "Basement", "Large", "Upper"]
LOAN_CHECKS_MIN = 1    # every loan sends at least one check
LOAN_CHECKS_MAX = 196  # total_loan_checks max (200) minus the other four loans' minimum
FAVORS_MAX = 100       # favorsanity max

LOAN_BASE_ID = 0x10000  # loan k, check j (1-based): LOAN_BASE_ID + k * 0x1000 + j
FAVOR_BASE_ID = 0x20000  # favor n (1-based): FAVOR_BASE_ID + n
MUSEUM_DONATE_BASE_ID = 0x30000  # + museum slot: donating it
MUSEUM_FIND_BASE_ID = 0x30100    # + museum slot: catching a bug/fish, digging up a fossil (buying a painting later)
# Per museum category; None = no find locations yet (paintings: room left for "Buy")
MUSEUM_FIND_VERBS = ["Dig Up", None, "Catch", "Catch"]


def loan_location_name(loan: int, check: int) -> str:
    return f"{LOANS[loan]} Loan {check}"


def favor_location_name(favor: int) -> str:
    return f"Favor {favor}"


def museum_donate_name(name: str) -> str:
    return f"Donate {name}"


def museum_find_name(category: int, name: str) -> str:
    return f"{MUSEUM_FIND_VERBS[category]} {name}"


LOCATION_NAME_TO_ID = {}
for _k in range(len(LOANS)):
    for _j in range(1, LOAN_CHECKS_MAX + 1):
        LOCATION_NAME_TO_ID[loan_location_name(_k, _j)] = LOAN_BASE_ID + _k * 0x1000 + _j
for _n in range(1, FAVORS_MAX + 1):
    LOCATION_NAME_TO_ID[favor_location_name(_n)] = FAVOR_BASE_ID + _n
for _c, _names in enumerate(MUSEUM_NAMES):
    for _i, _name in enumerate(_names):
        LOCATION_NAME_TO_ID[museum_donate_name(_name)] = MUSEUM_DONATE_BASE_ID + museum_slot(_c, _i)
        if MUSEUM_FIND_VERBS[_c]:
            LOCATION_NAME_TO_ID[museum_find_name(_c, _name)] = MUSEUM_FIND_BASE_ID + museum_slot(_c, _i)


def split_loan_checks(loans: list[int], total: int) -> list[int]:
    """
    Split `total` checks over the loans: one each, the rest by each loan's share of the total debt.
    Largest remainder, so the result always sums to `total`. Ties go to the earlier loan.
    """
    extra = total - LOAN_CHECKS_MIN * len(loans)
    debt = sum(loans)
    shares = [extra * amount / debt for amount in loans]
    counts = [int(share) for share in shares]
    left = extra - sum(counts)
    by_remainder = sorted(range(len(loans)), key=lambda i: counts[i] - shares[i])
    for i in by_remainder[:left]:
        counts[i] += 1
    return [LOAN_CHECKS_MIN + c for c in counts]
