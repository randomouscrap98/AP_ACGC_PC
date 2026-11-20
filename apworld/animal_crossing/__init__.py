from worlds.LauncherComponents import Component, Type, components, launch

from .world import AnimalCrossingWorld as AnimalCrossingWorld


def run_gc_client(*args: str) -> None:
    from .gc_client import main  # lazy: dolphin_memory_engine only loads when the client starts
    launch(main, name="AnimalCrossingGCClient", args=args)


def run_gc_patcher(*args: str) -> None:
    from .gc_patch import run_gui
    launch(run_gui, name="AnimalCrossingGCPatcher")


# The GameCube / Dolphin version's client (the PC version has its own, built into the game) and its disc patcher
components.append(Component("Animal Crossing GameCube Client", func=run_gc_client, component_type=Type.CLIENT))
components.append(Component("Animal Crossing GameCube Patcher", func=run_gc_patcher, component_type=Type.TOOL))
