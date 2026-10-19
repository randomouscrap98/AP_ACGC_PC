# Town Generation: new-town settings (fruit, grass, station, town day), the letters the client sends (paper,
# sender, loan letter text) and the villager blacklist + starting pool.
from .intro import clamp_name
from .options import personality_key, villager_key
from .villager_data import PERSONALITIES, VILLAGERS

LETTER_SENDER_MAX = 32  # MAIL_FOOTER_LEN in the game
DEFAULT_LETTER_SENDER = "Archipelago"
LETTER_TEXT_MAX = 192  # MAIL_BODY_LEN in the game
DEFAULT_LOAN_LETTER_TEXT = "Your loan is ready for|payoff at the post office!"
MIN_VILLAGERS = 6  # the game starts with 6 villagers


def letter_text(text: str) -> str:
    # | is the line break (FreeText is one line on the web options page)
    text = "".join(c for c in text if " " <= c <= "~")
    return text.strip().replace("|", "\n")[:LETTER_TEXT_MAX]


def villager_indices(chosen: set[str]) -> list[int]:
    # Expand personality shorthands into their villagers; the client gets npc indices
    return sorted(
        index for name, (index, looks) in VILLAGERS.items()
        if villager_key(name) in chosen or personality_key(PERSONALITIES[looks]) in chosen
    )


def setup(world) -> None:
    o = world.options
    world.ac_letter_sender = clamp_name(o.letter_sender.value, LETTER_SENDER_MAX) or DEFAULT_LETTER_SENDER
    world.ac_loan_letter_text = letter_text(o.loan_letter_text.value) or letter_text(DEFAULT_LOAN_LETTER_TEXT)
    # The 4th of July is the Fireworks Festival; the game never picks it either
    if o.town_day.value == 4:
        o.town_day.value = world.random.choice([d for d in range(1, 32) if d != 4])

    world.villager_blacklist = villager_indices(o.villager_blacklist.value)
    # The client skips blacklisted ones too; this keeps slot_data readable
    world.starting_villagers = [index for index in villager_indices(o.starting_villagers.value)
                                if index not in world.villager_blacklist]
    if len(VILLAGERS) - len(world.villager_blacklist) < MIN_VILLAGERS:
        raise world.option_error(f"Villager Blacklist must leave at least {MIN_VILLAGERS} villagers allowed.")


def slot_data(world) -> dict:
    o = world.options
    return {
        "town_fruit": o.town_fruit.value,
        "grass_shape": o.grass_shape.value,
        "train_station": o.train_station.value,
        "town_day": o.town_day.value,
        "villager_blacklist": world.villager_blacklist,
        "starting_villagers": world.starting_villagers,
        "letter_paper": o.letter_paper.value,
        "letter_sender": world.ac_letter_sender,
        "loan_letter_text": world.ac_loan_letter_text,
    }
