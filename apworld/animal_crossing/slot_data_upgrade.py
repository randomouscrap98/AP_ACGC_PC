# Old seeds' slot_data -> the current format, for Universal Tracker (interpret_slot_data). The steps are data in
# slot_data_upgrades.json (its _readme), shared with the client DLL (ap_slotdata.cpp applies the same file).
import copy
import json
import pkgutil
from typing import Any

OPS = ("renamed", "remapped", "added", "removed")  # the order they're applied in within one step
STEPS: dict[str, Any] = json.loads(pkgutil.get_data(__name__, "slot_data_upgrades.json"))


def apply(slot_data: dict[str, Any], steps: dict[str, Any], current: int) -> dict[str, Any]:
    # A copy of slot_data with every step after its version up to current applied. A seed newer than current is
    # returned as is (the client refuses it; the tracker can't do better).
    out = copy.deepcopy(slot_data)
    version = out.get("slot_data_version", 1)
    if version > current:
        return out
    for v in range(version + 1, current + 1):
        step = steps.get(str(v), {})
        unknown = set(step) - set(OPS) - {"note"}
        if unknown:
            raise ValueError(f"slot_data upgrade step {v}: unknown entries {sorted(unknown)}")
        for old, new in step.get("renamed", {}).items():
            if old in out:
                out[new] = out.pop(old)
        for key, pairs in step.get("remapped", {}).items():
            for old, new in pairs:
                if key in out and out[key] == old:
                    out[key] = new
                    break
        for key, value in step.get("added", {}).items():
            out.setdefault(key, copy.deepcopy(value))
        for key in step.get("removed", []):
            out.pop(key, None)
    out["slot_data_version"] = current
    return out


def upgrade(slot_data: dict[str, Any], current: int) -> dict[str, Any]:
    return apply(slot_data, STEPS, current)
