#include "pc_ap_weather.h"
#include "m_kankyo.h"

#include <stdint.h>

int pc_ap_weather_roll(int town_id, int year, int month, int day) {
  uint32_t h = (uint32_t)town_id * 0x9E3779B1u ^ (uint32_t)(year * 10000 + month * 100 + day);

  // Integer hash finalizer (lowbias32), so neighboring dates don't roll alike
  h ^= h >> 16;
  h *= 0x7FEB352Du;
  h ^= h >> 15;
  h *= 0x846CA68Bu;
  h ^= h >> 16;
  return (int)(h % 10);
}

int pc_ap_weather_rain_day(int month, int day) {
  return day == 13 && month >= 2 && month <= 11;
}

const char* pc_ap_weather_name(int weather, int intensity) {
  switch(weather) {
    case mEnv_WEATHER_RAIN:
      return intensity == mEnv_WEATHER_INTENSITY_HEAVY ? "Thunderstorm" : "Rain";
    case mEnv_WEATHER_SNOW:
      return intensity == mEnv_WEATHER_INTENSITY_HEAVY ? "Blizzard" : "Snow";
    case mEnv_WEATHER_SAKURA:
      return "Cherry blossoms";
    case mEnv_WEATHER_LEAVES:
      return "Falling leaves";
    default:
      return "Clear";
  }
}
