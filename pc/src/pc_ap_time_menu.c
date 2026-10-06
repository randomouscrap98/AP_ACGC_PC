#include "pc_ap_time_menu.h"
#include "pc_ap_logic.h"
#include "pc_ap_timesanity.h" // slot_of_hour
#include "pc_menu_util.h"

#include "lb_rtc.h"
#include "game.h"
#include "graph.h"

#include <stdio.h>

enum { ROW_YEAR, ROW_MONTH, ROW_DAY, ROW_HOUR, ROW_CHANGE, ROW_BACK, ROW_COUNT };

static lbRTC_time_c s_now;     // clock when the page opened (minutes dropped)
static lbRTC_time_c s_pending; // date and hour being picked
static int s_sel;
static int s_confirm;          // on the "the game will save" page
static int s_confirm_sel;      // 0 = No, 1 = Yes

static int pc_ap_time_menu_same_date(const lbRTC_time_c* a, const lbRTC_time_c* b) {
  return lbRTC_IsEqualDate(a->year, a->month, a->day, b->year, b->month, b->day) == lbRTC_EQUAL;
}

void pc_ap_time_menu_enter(void) {
  lbRTC_GetTime(&s_now);
  s_now.min = 0;
  s_now.sec = PC_AP_TIME_SEC;
  s_pending = s_now;
  s_sel = ROW_YEAR;
  s_confirm = 0;
}

void pc_ap_time_menu_up(void) {
  if(s_confirm) {
    s_confirm_sel = !s_confirm_sel;
  } else {
    s_sel = (s_sel + ROW_COUNT - 1) % ROW_COUNT;
  }
}

void pc_ap_time_menu_down(void) {
  if(s_confirm) {
    s_confirm_sel = !s_confirm_sel;
  } else {
    s_sel = (s_sel + 1) % ROW_COUNT;
  }
}

static void pc_ap_time_menu_step(int dir) {
  if(s_confirm) {
    s_confirm_sel = !s_confirm_sel;
    return;
  }
  switch(s_sel) {
    case ROW_YEAR:  pc_ap_time_step_year(&s_pending, dir);  break;
    case ROW_MONTH: pc_ap_time_step_month(&s_pending, dir); break;
    case ROW_DAY:   pc_ap_time_step_day(&s_pending, dir);   break;
    case ROW_HOUR:  pc_ap_time_step_hour(&s_pending, dir);  break;
  }
}

void pc_ap_time_menu_left(void) {
  pc_ap_time_menu_step(-1);
}

void pc_ap_time_menu_right(void) {
  pc_ap_time_menu_step(1);
}

int pc_ap_time_menu_confirm(void) {
  if(s_confirm) {
    if(s_confirm_sel == 1) {
      pc_ap_time_request(&s_pending);
      return PC_AP_TIME_MENU_RESUME;
    }
    s_confirm = 0;
    return PC_AP_TIME_MENU_STAY;
  }
  if(s_sel == ROW_BACK) {
    return PC_AP_TIME_MENU_BACK;
  }
  if(s_sel != ROW_CHANGE) {
    s_sel = ROW_CHANGE;
    return PC_AP_TIME_MENU_STAY;
  }
  // New date: reload, which saves, so ask first. Same date: hour set live.
  if(!pc_ap_time_menu_same_date(&s_pending, &s_now)) {
    s_confirm = 1;
    s_confirm_sel = 0;
    return PC_AP_TIME_MENU_STAY;
  }
  if(s_pending.hour != s_now.hour) {
    pc_ap_time_request(&s_pending);
  }
  return PC_AP_TIME_MENU_RESUME;
}

int pc_ap_time_menu_cancel(void) {
  if(s_confirm) {
    s_confirm = 0;
    return PC_AP_TIME_MENU_STAY;
  }
  return PC_AP_TIME_MENU_BACK;
}

static const char* l_month_names[12] = {
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December",
};
static const char* l_weekday_names[lbRTC_WEEK] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
};
static const char* l_slot_names[PC_AP_SLOT_NUM] = { "Morning", "Day", "Evening", "Night" };

static void pc_ap_time_menu_format(int row, char* out, int size) {
  const lbRTC_time_c* t = &s_pending;
  int hour12 = t->hour % 12 == 0 ? 12 : t->hour % 12;

  switch(row) {
    case ROW_YEAR:
      snprintf(out, size, "< %d >", t->year);
      break;
    case ROW_MONTH:
      snprintf(out, size, "< %s >", l_month_names[(t->month - 1) % 12]);
      break;
    case ROW_DAY:
      snprintf(out, size, "< %d >  %s", t->day, l_weekday_names[t->weekday % lbRTC_WEEK]);
      break;
    case ROW_HOUR:
      snprintf(out, size, "< %d %s >  %s", hour12, t->hour < 12 ? "AM" : "PM",
               l_slot_names[pc_ap_timesanity_slot_of_hour(t->hour)]);
      break;
    default:
      out[0] = 0;
      break;
  }
}

static void pc_ap_time_menu_draw_page(struct game_s* game) {
  static const char* labels[ROW_HOUR + 1] = { "Year", "Month", "Day", "Hour" };
  int r, g, b, a;
  f32 lx = 70.0f;
  f32 vx = 140.0f;
  f32 y0 = 70.0f;
  f32 line_h = 15.0f;
  char value[64];

  pc_menu_draw_centered(game, "- Date & Time -", 40.0f, 255, 255, 255, 255, 1.0f);
  for(int i = 0; i <= ROW_HOUR; i++) {
    int selected = s_sel == i;
    f32 s = selected ? PC_MENU_SCALE_SELECTED : 1.0f;
    pc_menu_row_colors(selected, &r, &g, &b, &a);
    pc_menu_draw_left(game, labels[i], lx, y0 + i * line_h, r, g, b, a, s);
    pc_ap_time_menu_format(i, value, sizeof(value));
    pc_menu_draw_left(game, value, vx, y0 + i * line_h, r, g, b, a, s);
  }

  f32 y = y0 + 4 * line_h + 10.0f;
  pc_menu_row_colors(s_sel == ROW_CHANGE, &r, &g, &b, &a);
  pc_menu_draw_centered(game, "Change", y, r, g, b, a, s_sel == ROW_CHANGE ? PC_MENU_SCALE_SELECTED : 1.0f);
  pc_menu_row_colors(s_sel == ROW_BACK, &r, &g, &b, &a);
  pc_menu_draw_centered(game, "Back", y + line_h, r, g, b, a, s_sel == ROW_BACK ? PC_MENU_SCALE_SELECTED : 1.0f);
}

static void pc_ap_time_menu_draw_confirm(struct game_s* game) {
  char date[64];

  snprintf(date, sizeof(date), "%s %d, %d", l_month_names[(s_pending.month - 1) % 12], s_pending.day,
           s_pending.year);
  pc_menu_draw_centered(game, "- Change the date? -", 80.0f, 255, 255, 255, 255, 1.0f);
  pc_menu_draw_centered(game, date, 105.0f, 230, 230, 230, 255, 1.0f);
  pc_menu_draw_centered(game, "The game will save.", 125.0f, 230, 230, 230, 255, 1.0f);
  pc_menu_draw_two_choice(game, "No", "Yes", s_confirm_sel, 160.0f);
}

void pc_ap_time_menu_draw(struct game_s* game) {
  pc_menu_dim_rect(game->graph, 180);
  if(s_confirm) {
    pc_ap_time_menu_draw_confirm(game);
  } else {
    pc_ap_time_menu_draw_page(game);
  }
}
