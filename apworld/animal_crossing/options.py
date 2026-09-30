from dataclasses import dataclass

from Options import Choice, DefaultOnToggle, FreeText, PerGameCommonOptions, Toggle


class SkipIntro(Choice):
    """
    How much of the new game intro to skip.
    off: play the train ride and Tom Nook's part-time job normally.
    train_and_job: skip the train ride and the job; you start next to your house.
    """
    display_name = "Skip Intro"
    option_off = 0
    # 1 is reserved for a future "train" (skip the train, keep the job) option
    option_train_and_job = 2
    default = option_train_and_job


class PlayerName(FreeText):
    """Your character's name (max 8 characters). Empty uses your slot name (truncated)."""
    display_name = "Player Name"
    default = ""


class TownName(FreeText):
    """Your town's name (max 8 characters). Empty uses "Archi"."""
    display_name = "Town Name"
    default = ""


class Gender(Choice):
    """Your character's gender."""
    display_name = "Gender"
    option_boy = 0
    option_girl = 1
    default = "random"


class Face(Choice):
    """Your character's face (the game normally picks this from Rover's questions)."""
    display_name = "Face"
    option_face_1 = 0
    option_face_2 = 1
    option_face_3 = 2
    option_face_4 = 3
    option_face_5 = 4
    option_face_6 = 5
    option_face_7 = 6
    option_face_8 = 7
    default = "random"


class House(Choice):
    """Which house you get when the job is skipped."""
    display_name = "House"
    option_top_left = 0
    option_top_right = 1
    option_bottom_left = 2
    option_bottom_right = 3
    default = "random"


class TownFruit(Choice):
    """Your town's native fruit (applies to every new town, with or without the intro skip)."""
    display_name = "Town Fruit"
    option_apple = 0
    option_cherry = 1
    option_pear = 2
    option_peach = 3
    option_orange = 4
    default = "random"


class NoCockroaches(DefaultOnToggle):
    """Cockroaches never appear in your house, even if you're away for a while."""
    display_name = "No Cockroaches"


class ShopsAlwaysOpen(Toggle):
    """Shops are open at any hour."""
    display_name = "Shops Always Open"


class NoWeeds(DefaultOnToggle):
    """Weeds never grow in your town."""
    display_name = "No Weeds"


@dataclass
class AnimalCrossingOptions(PerGameCommonOptions):
    skip_intro: SkipIntro
    player_name: PlayerName
    town_name: TownName
    gender: Gender
    face: Face
    house: House
    town_fruit: TownFruit
    no_cockroaches: NoCockroaches
    shops_always_open: ShopsAlwaysOpen
    no_weeds: NoWeeds
