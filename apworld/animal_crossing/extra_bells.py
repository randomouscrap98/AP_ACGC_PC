# Extra Bells: the Extra Bells option (in 10,000 Bell steps) as the game's own 10,000 and 30,000 Bell bags
# (game_items.py), mailed by the client with the sender's name. They take filler slots before the Bell Credits.
TEN_K, THIRTY_K = "10,000 Bells", "30,000 Bells"
THIRTY_K_CHANCE = 0.25  # 10,000 three times as likely


def items(world, free: int) -> list[str]:
    # Random 10k/30k bags adding up to exactly the option; 10k only once less than 30k is left
    units = world.options.extra_bells.value
    names = []
    while units > 0 and len(names) < free:
        if units >= 3 and world.random.random() < THIRTY_K_CHANCE:
            names.append(THIRTY_K)
            units -= 3
        else:
            names.append(TEN_K)
            units -= 1
    if units <= 0:
        return names
    # Not enough slots: every slot a bag, with enough 30k ones to get as close as possible without going over
    target = world.options.extra_bells.value
    thirty = min(free, max(0, (target - free) // 2))
    return [THIRTY_K] * thirty + [TEN_K] * (free - thirty)


def slot_data(world) -> dict:
    # Universal Tracker only (the client mails whatever bags it gets): Fruit Filler takes the slots left after these
    return {"extra_bells": world.options.extra_bells.value}
