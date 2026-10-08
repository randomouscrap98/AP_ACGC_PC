// Weather from the date. The daily weather roll (mEnv_RandomWeather, once per new date) takes
// its 0-9 from a hash of the town id and the date instead of the game's RNG, so a date always
// gets the same weather and the Date & Time page can show it ahead. The odds stay vanilla.
#ifndef PC_AP_WEATHER_H
#define PC_AP_WEATHER_H

#ifdef __cplusplus
extern "C" {
#endif

// The roll (0-9) for a town and date
int pc_ap_weather_roll(int town_id, int year, int month, int day);

// Nonzero on the rain day: the 13th of February to November (the months vanilla can rain in), so
// rain-only critters (Coelacanth, Snail) can always be caught in their months. Event days that
// force clear weather still win (Sep 13 is the harvest moon in 2019, 2038, 2057, 2095).
int pc_ap_weather_rain_day(int month, int day);

// Display name of a weather type + intensity (mEnv_WEATHER_*, mEnv_WEATHER_INTENSITY_*)
const char* pc_ap_weather_name(int weather, int intensity);

#ifdef __cplusplus
}
#endif

#endif
