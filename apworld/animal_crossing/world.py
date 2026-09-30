from collections.abc import Mapping
from typing import Any

from BaseClasses import Item, ItemClassification, Location, Region
from worlds.AutoWorld import World

from .items import ITEM_NAME_TO_ID
from .options import AnimalCrossingOptions

GAME_NAME = "Animal Crossing"
NAME_MAX = 8  # PLAYER_NAME_LEN / LAND_NAME_SIZE in the game
DEFAULT_TOWN = "Archi"

# Placeholder so the generator has something to place. Real checks come later.
LOCATION_NAME_TO_ID = {
    "Arrive in Town": 1,
}


class AnimalCrossingItem(Item):
    game = GAME_NAME


class AnimalCrossingLocation(Location):
    game = GAME_NAME


def clamp_name(text: str) -> str:
    # The client does the real charset conversion; keep printable ASCII and the length here.
    text = "".join(c for c in text if " " <= c <= "~")
    return text.strip()[:NAME_MAX]


class AnimalCrossingWorld(World):
    """
    Animal Crossing (GameCube), via the PC port.
    """

    game = GAME_NAME
    options_dataclass = AnimalCrossingOptions
    options: AnimalCrossingOptions

    item_name_to_id = ITEM_NAME_TO_ID
    location_name_to_id = LOCATION_NAME_TO_ID

    def generate_early(self) -> None:
        self.ac_player_name = clamp_name(self.options.player_name.value) or clamp_name(self.player_name)
        self.ac_town_name = clamp_name(self.options.town_name.value) or DEFAULT_TOWN

    def create_regions(self) -> None:
        menu = Region("Menu", self.player, self.multiworld)
        menu.add_locations(LOCATION_NAME_TO_ID, AnimalCrossingLocation)
        self.multiworld.regions.append(menu)

    def create_items(self) -> None:
        self.multiworld.itempool.append(self.create_item("Bells (1000)"))

    def create_item(self, name: str) -> AnimalCrossingItem:
        return AnimalCrossingItem(name, ItemClassification.filler, ITEM_NAME_TO_ID[name], self.player)

    def get_filler_item_name(self) -> str:
        return "Bells (1000)"

    def set_rules(self) -> None:
        self.multiworld.completion_condition[self.player] = lambda state: True

    def fill_slot_data(self) -> Mapping[str, Any]:
        # Random choices are already resolved here, so the client gets plain numbers.
        return {
            "skip_intro": self.options.skip_intro.value,
            "player_name": self.ac_player_name,
            "town_name": self.ac_town_name,
            "gender": self.options.gender.value,
            "face": self.options.face.value,
            "house": self.options.house.value,
            "town_fruit": self.options.town_fruit.value,
            "no_cockroaches": self.options.no_cockroaches.value,
            "shops_always_open": self.options.shops_always_open.value,
            "no_weeds": self.options.no_weeds.value,
        }
