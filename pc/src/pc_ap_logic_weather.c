// pc_ap_logic_weather: fixed weather per date: today's roll, the rain day, another date's forecast.
// Part of the pc_ap_logic.c facade; the feature's own logic is in pc_ap_weather.c.
#include "pc_ap_logic_internal.h"
#include "pc_ap_weather.h"
#include "m_common_data.h"
#include "m_kankyo.h"
#include "m_land.h"

int pc_ap_weather_roll_today(void) {
  lbRTC_time_c* now = Common_GetPointer(time.rtc_time);
  return pc_ap_weather_roll(Save_Get(land_info).id, now->year, now->month, now->day);
}

int pc_ap_weather_rain_day_today(void) {
  lbRTC_time_c* now = Common_GetPointer(time.rtc_time);
  return pc_ap_weather_rain_day(now->month, now->day);
}

int pc_ap_weather_of_date(const lbRTC_time_c* date, int* intensity_out) {
  lbRTC_time_c* now = Common_GetPointer(time.rtc_time);
  lbRTC_time_c saved = *now;
  s16 weather;
  s16 intensity;

  // Today: what was rolled (or set by an event, or before this hook existed)
  if(lbRTC_IsEqualDate(now->year, now->month, now->day, date->year, date->month, date->day) == lbRTC_EQUAL) {
    *intensity_out = mEnv_SAVE_GET_WEATHER_INTENSITY(Save_Get(weather));
    return mEnv_SAVE_GET_WEATHER_TYPE(Save_Get(weather));
  }
  // Another date: the game's own roll with that date in the clock, put back right after
  // (the roll reads the date from Common rtc_time)
  *now = *date;
  mEnv_RandomWeather(&weather, &intensity);
  *now = saved;
  *intensity_out = intensity;
  return weather;
}
