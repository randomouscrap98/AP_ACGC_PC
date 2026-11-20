from worlds.LauncherComponents import Component, Type, components, launch

from .world import AnimalCrossingWorld as AnimalCrossingWorld


def run_gc_client(*args: str) -> None:
    from .gc_client import main  # lazy: dolphin_memory_engine only loads when the client starts
    launch(main, name="AnimalCrossingGCClient", args=args)


# The GameCube / Dolphin version's client (the PC version has its own, built into the game)
components.append(Component("Animal Crossing GameCube Client", func=run_gc_client, component_type=Type.CLIENT))
