// slot_data JSON text -> ap_slotdata, in plain C (the future shared parser: PC and GC). A line-by-line copy of
// apclient/src/ap_slotdata.cpp's ap_slotdata_fill (same defaults, ranges, clamping, tables, string truncation),
// with jsmn as the tokenizer and small number/string helpers instead of nlohmann. Old seeds are upgraded before
// this (ap_slotdata_upgrade on the PC, the Python client on the GC), so it only ever sees the current format.
#define JSMN_STATIC // header-only: the implementation belongs to this unit
#define JSMN_STRICT // reject junk; nlohmann is a strict parser too
#include "../lib/jsmn/jsmn.h"

#include "pc_ap_slotdata_parse.h"

#include <limits.h>
#include <string.h>

typedef struct {
  const char * json;
  const jsmntok_t * tok;
} sd_doc;

// A warning's copy of a token: its raw JSON text (nlohmann's dump() would only differ in quoting/whitespace)
#define SD_TOK_STR(d, i) ((d)->tok[i].end - (d)->tok[i].start), ((d)->json + (d)->tok[i].start)

// ---------------------------------------------------------------------------------------------------------------
// tokens
// ---------------------------------------------------------------------------------------------------------------

// Index just past token i and everything inside it. A container's children are contiguous in jsmn and its size is
// the number of keys (object) or elements (array)
static int sd_tok_end(const jsmntok_t * tok, int i) {
  const jsmntok_t * t = &tok[i];
  int k;
  i++;
  if (t->type == JSMN_OBJECT) {
    for (k = 0; k < t->size; k++) {
      i = sd_tok_end(tok, i); // key
      i = sd_tok_end(tok, i); // value
    }
  } else if (t->type == JSMN_ARRAY) {
    for (k = 0; k < t->size; k++) { i = sd_tok_end(tok, i); }
  }
  return i;
}

// ---------------------------------------------------------------------------------------------------------------
// numbers, booleans, strings
// ---------------------------------------------------------------------------------------------------------------

// Four hex digits at js[*pos..end) -> value, *pos past them; -1 if they are not hex
static int sd_hex4(const char * js, int * pos, int end) {
  int i, v = 0;
  if (*pos + 4 > end) { return -1; }
  for (i = 0; i < 4; i++) {
    int c = js[(*pos)++];
    v <<= 4;
    if (c >= '0' && c <= '9') {
      v |= c - '0';
    } else if (c >= 'a' && c <= 'f') {
      v |= c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      v |= c - 'A' + 10;
    } else {
      return -1;
    }
  }
  return v;
}

// Appends one UTF-8 code point, cut off at size - 1 bytes like snprintf does
static int sd_put_utf8(char * out, int size, int n, unsigned int cp) {
  char b[4];
  int len, k;
  if (cp < 0x80) {
    b[0] = (char)cp;
    len = 1;
  } else if (cp < 0x800) {
    b[0] = (char)(0xC0 | (cp >> 6));
    b[1] = (char)(0x80 | (cp & 0x3F));
    len = 2;
  } else if (cp < 0x10000) {
    b[0] = (char)(0xE0 | (cp >> 12));
    b[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    b[2] = (char)(0x80 | (cp & 0x3F));
    len = 3;
  } else {
    b[0] = (char)(0xF0 | (cp >> 18));
    b[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    b[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    b[3] = (char)(0x80 | (cp & 0x3F));
    len = 4;
  }
  for (k = 0; k < len; k++) {
    if (n < size - 1) { out[n++] = b[k]; }
  }
  return n;
}

// A jsmn string token into out: NUL terminated, cut to size - 1 bytes (what snprintf("%s") does with the C++
// std::string), but with nlohmann's escapes decoded: \" \\ \/ \b \f \n \r \t and \uXXXX, a UTF-16 surrogate pair
// becoming the 4 byte UTF-8 sequence. Returns the number of bytes written.
static int sd_decode(char * out, int size, const char * js, const jsmntok_t * t) {
  int pos = t->start;
  int end = t->end;
  int n = 0;
  if (size < 1) { return 0; }
  while (pos < end) {
    int c = (unsigned char)js[pos++];
    if (c == '\\' && pos < end) {
      c = (unsigned char)js[pos++];
      switch (c) {
        case 'b': c = '\b'; break;
        case 'f': c = '\f'; break;
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
        case 'u': {
          int cp = sd_hex4(js, &pos, end);
          if (cp >= 0xD800 && cp <= 0xDBFF) { // high surrogate: the low one has to follow
            int lo = -1;
            if (pos + 1 < end && js[pos] == '\\' && js[pos + 1] == 'u') {
              pos += 2;
              lo = sd_hex4(js, &pos, end);
            }
            cp = (lo >= 0xDC00 && lo <= 0xDFFF) ? 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00) : 0xFFFD;
          } else if (cp < 0 || (cp >= 0xDC00 && cp <= 0xDFFF)) {
            cp = 0xFFFD; // nlohmann refuses such a document outright; a replacement char is good enough here
          }
          n = sd_put_utf8(out, size, n, (unsigned int)cp);
          continue;
        }
        default: break; // \" \\ and \/ are themselves; jsmn already rejects every other escape
      }
    }
    if (c == 0) { break; } // snprintf("%s") stops at the first NUL byte
    if (n < size - 1) { out[n++] = (char)c; }
  }
  out[n] = 0;
  return n;
}

// strlen(s) cut to size - 1 bytes: snprintf(out, size, "%s", s)
static void sd_put_str(char * out, int size, const char * s) {
  int n = 0;
  while (s[n] != 0 && n < size - 1) { n++; }
  memcpy(out, s, (size_t)n);
  out[n] = 0;
}

// ---------------------------------------------------------------------------------------------------------------
// top-level key index
// ---------------------------------------------------------------------------------------------------------------

// Every key the fill reads from the top-level object, alphabetically (the lookup is a linear scan, so the order
// is only for reading). A key missing here still works: sd_obj_get then walks the object for it like before, just
// slowly, so a forgotten entry can't make a value disappear.
typedef struct {
  const char * name;
  int len; // = strlen(name), for the cheap reject
} sd_key;

#define SD_KEY(s) { s, sizeof(s) - 1 }
static const sd_key sd_top_keys[] = {
  SD_KEY("beach_checks_per_month"),
  SD_KEY("beach_hint_count"),
  SD_KEY("beach_hint_letter"),
  SD_KEY("bell_credits"),
  SD_KEY("bug_availability"),
  SD_KEY("bug_checks"),
  SD_KEY("bug_passes"),
  SD_KEY("catalog_goal_count"),
  SD_KEY("catalog_pools"),
  SD_KEY("catalog_sources"),
  SD_KEY("catalogsanity"),
  SD_KEY("coconuts_wash_up"),
  SD_KEY("critter_odds"),
  SD_KEY("face"),
  SD_KEY("favor_pools"),
  SD_KEY("favors_per_pool"),
  SD_KEY("favorsanity"),
  SD_KEY("fish_availability"),
  SD_KEY("fish_checks"),
  SD_KEY("fish_passes"),
  SD_KEY("fossil_availability"),
  SD_KEY("fossil_checks"),
  SD_KEY("fossil_odds"),
  SD_KEY("fossil_seasons"),
  SD_KEY("fossil_series"),
  SD_KEY("furniture_set_items"),
  SD_KEY("gender"),
  SD_KEY("global_storage"),
  SD_KEY("goal"),
  SD_KEY("grass_shape"),
  SD_KEY("hourly_music"),
  SD_KEY("house"),
  SD_KEY("letter_paper"),
  SD_KEY("letter_sender"),
  SD_KEY("loan_checks"),
  SD_KEY("loan_letter_text"),
  SD_KEY("loans"),
  SD_KEY("loansanity"),
  SD_KEY("more_favors"),
  SD_KEY("museum_goal_count"),
  SD_KEY("museumsanity"),
  SD_KEY("new_item_chance"),
  SD_KEY("no_falling_stalks"),
  SD_KEY("no_weeds"),
  SD_KEY("nook_store_thresholds"),
  SD_KEY("nook_store_upgrades"),
  SD_KEY("normalized_time_travel"),
  SD_KEY("painting_checks"),
  SD_KEY("player_name"),
  SD_KEY("radio_goal_count"),
  SD_KEY("radiosanity"),
  SD_KEY("radiosanity_aircheck"),
  SD_KEY("radiosanity_song_count"),
  SD_KEY("radiosanity_songs"),
  SD_KEY("shellsanity"),
  SD_KEY("shellsanity_item_chance"),
  SD_KEY("shop_never_sells_out"),
  SD_KEY("shops_always_open"),
  SD_KEY("skip_hra_talk"),
  SD_KEY("skip_intro"),
  SD_KEY("start_year"),
  SD_KEY("starting_month"),
  SD_KEY("starting_shirt"),
  SD_KEY("starting_time"),
  SD_KEY("starting_villagers"),
  SD_KEY("timesanity"),
  SD_KEY("tools_in_pool"),
  SD_KEY("town_day"),
  SD_KEY("town_fruit"),
  SD_KEY("town_name"),
  SD_KEY("train_station"),
  SD_KEY("tree_checks_per_fruit"),
  SD_KEY("treesanity"),
  SD_KEY("turnips_never_spoil"),
  SD_KEY("villager_blacklist"),
  SD_KEY("villagers_dont_leave"),
  SD_KEY("villagers_never_sleep"),
  SD_KEY("world_version"),
};
#define SD_TOP_KEY_NUM ((int)(sizeof(sd_top_keys) / sizeof(sd_top_keys[0])))

// Value token of each sd_top_keys[] entry in the top-level object, -1 = the key is missing (76 x 4 = 304 bytes,
// static like the token array: one parse at a time). Filled by sd_top_index.
static int sd_top_value[SD_TOP_KEY_NUM];

// Index of a decoded key name in sd_top_keys, -1 = not one of the keys the fill reads
static int sd_top_key_index(const char * name, int len) {
  int i;
  for (i = 0; i < SD_TOP_KEY_NUM; i++) {
    if (sd_top_keys[i].len == len && memcmp(sd_top_keys[i].name, name, (size_t)len) == 0) { return i; }
  }
  return -1;
}

// One pass over the top-level object's keys to fill sd_top_value, so the fill's ~70 lookups read a table instead
// of each walking the whole object (nested objects, like bell_credits, still use sd_obj_get's scan). A duplicate
// key keeps overwriting: the last one wins, exactly like the scan's `found` does.
static void sd_top_index(const sd_doc * d) {
  int i, k, idx;
  char name[40]; // the longest key we read is shellsanity_item_chance (24), sd_decode cuts at 39 bytes
  for (i = 0; i < SD_TOP_KEY_NUM; i++) { sd_top_value[i] = -1; }
  i = 1;
  for (k = 0; k < d->tok[0].size; k++) {
    if (d->tok[i].type == JSMN_STRING) { // the key token; jsmn's strict mode guarantees strings anyway
      sd_decode(name, sizeof(name), d->json, &d->tok[i]);
      idx = sd_top_key_index(name, (int)strlen(name)); // strlen, not the decoded length: a \u0000 ends the name (strcmp did)
      if (idx >= 0) { sd_top_value[idx] = i + 1; }
    }
    i = sd_tok_end(d->tok, i + 1); // past the value = the next key
  }
}

// The value token of key in the object at obj, -1 if there is none. A duplicate key gives the last one (nlohmann).
// obj 0 is the top-level object, which sd_top_index has indexed: the keys in sd_top_keys come straight from there.
static int sd_obj_get(const sd_doc * d, int obj, const char * key) {
  int i, k, found = -1;
  char name[40]; // the longest key we read is shellsanity_item_chance (24)
  if (obj < 0 || d->tok[obj].type != JSMN_OBJECT) { return -1; }
  if (obj == 0) {
    int idx = sd_top_key_index(key, (int)strlen(key));
    if (idx >= 0) { return sd_top_value[idx]; }
  }
  i = obj + 1;
  for (k = 0; k < d->tok[obj].size; k++) {
    if (d->tok[i].type == JSMN_STRING) {
      sd_decode(name, sizeof(name), d->json, &d->tok[i]);
      if (strcmp(name, key) == 0) { found = i + 1; }
    }
    i = sd_tok_end(d->tok, i + 1); // past the value = the next key
  }
  return found;
}

// A JSON integer token as nlohmann types it (is_number_integer: no '.', 'e', 'E'):
//   *trunc = the value as ap_slotdata_fill's get<int>() reads it (low 32 bits: 3000000000 -> -1294967296),
//   *exact = 1 when the whole value fits an int (where the range checks use the get<long long>() value).
// 0 = not an integer: a float, true/false, null, a string, or 20+ digits (nlohmann makes those a float).
static int sd_tok_int(const sd_doc * d, int i, int * trunc, int * exact) {
  const jsmntok_t * t = &d->tok[i];
  const char * js = d->json;
  int pos, end, neg = 0, digits = 0;
  unsigned int mag = 0;
  if (t->type != JSMN_PRIMITIVE) { return 0; }
  pos = t->start;
  end = t->end;
  if (pos < end && js[pos] == '-') {
    neg = 1;
    pos++;
  }
  if (pos >= end || js[pos] < '0' || js[pos] > '9') { return 0; }
  if (js[pos] == '0' && pos + 1 < end) { return 0; } // leading zero: not a JSON number either
  for (; pos < end; pos++) {
    int c = js[pos];
    if (c < '0' || c > '9') { return 0; }
    digits++;
    mag = mag * 10u + (unsigned int)(c - '0'); // wraps: the low 32 bits are all get<int>() keeps
  }
  if (digits > 19) { return 0; } // past uint64/int64, where nlohmann falls back to a float
  *trunc = neg ? (int)(0u - mag) : (int)mag;
  *exact = mag <= 2147483647u;
  return 1;
}

// true/false -> 1/0 (read_int takes a boolean as an int for every key)
static int sd_tok_bool(const sd_doc * d, int i, int * out) {
  const jsmntok_t * t = &d->tok[i];
  int n;
  if (t->type != JSMN_PRIMITIVE) { return 0; }
  n = t->end - t->start;
  if (n == 4 && memcmp(d->json + t->start, "true", 4) == 0) {
    *out = 1;
    return 1;
  }
  if (n == 5 && memcmp(d->json + t->start, "false", 5) == 0) {
    *out = 0;
    return 1;
  }
  return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// key readers (ap_slotdata_fill's read_* helpers)
// ---------------------------------------------------------------------------------------------------------------

// An int key in [min, max]; missing = def, anything else (wrong type, out of range) = def + a warning.
// true/false count as 1/0
static int sd_int(const sd_doc * d, const char * key, int def, int min, int max) {
  int t = sd_obj_get(d, 0, key);
  int v = 0;
  int exact = 1;
  if (t < 0) { return def; }
  if (!sd_tok_bool(d, t, &v) && !sd_tok_int(d, t, &v, &exact)) {
    PC_AP_SD_WARN("%s: %.*s is not a number, using %d", key, SD_TOK_STR(d, t), def);
    return def;
  }
  if (exact && v >= min && v <= max) { return v; }
  PC_AP_SD_WARN("%s: %.*s is not in %d..%d, using %d", key, SD_TOK_STR(d, t), min, max, def);
  return def;
}

static int sd_bool(const sd_doc * d, const char * key, int def) {
  return sd_int(d, key, def, 0, 1);
}

// A string field: snprintf(out, size, "%s", value), def when the key is missing. A present value that is not a
// string makes the C++ throw (json::value); here it is the default + a warning.
static void sd_str(char * out, int size, const sd_doc * d, const char * key, const char * def) {
  int t = sd_obj_get(d, 0, key);
  if (t >= 0 && d->tok[t].type == JSMN_STRING) {
    sd_decode(out, size, d->json, &d->tok[t]);
    return;
  }
  if (t >= 0) { PC_AP_SD_WARN("%s: %.*s is not a string, using the default", key, SD_TOK_STR(d, t)); }
  sd_put_str(out, size, def);
}

// out[0..n) from a json int array; a missing key or bad entries keep the defaults (read_int_array: a float, a bool
// or a string entry leaves that slot alone, a big number is truncated like get<int>())
static void sd_int_array(const sd_doc * d, const char * key, int * out, const int * defaults, int n) {
  int a, i, k, v, exact;
  for (i = 0; i < n; i++) { out[i] = defaults[i]; }
  a = sd_obj_get(d, 0, key);
  if (a < 0 || d->tok[a].type != JSMN_ARRAY) { return; }
  k = a + 1;
  for (i = 0; i < n && i < d->tok[a].size; i++, k = sd_tok_end(d->tok, k)) {
    if (sd_tok_int(d, k, &v, &exact)) { out[i] = v; }
  }
}

// Per-thing item indices (bug_passes, fossil_series): out[i] in [0, num), -1 for a missing key, a short list or a
// bad entry (= nothing needed)
static void sd_pass_array(const sd_doc * d, const char * key, int * out, int n, int num) {
  int a, i, k, v, exact;
  for (i = 0; i < n; i++) { out[i] = -1; }
  a = sd_obj_get(d, 0, key);
  if (a < 0 || d->tok[a].type != JSMN_ARRAY) { return; }
  k = a + 1;
  for (i = 0; i < n && i < d->tok[a].size; i++, k = sd_tok_end(d->tok, k)) {
    if (sd_tok_int(d, k, &v, &exact) && exact && v >= 0 && v < num) {
      out[i] = v;
    } else {
      PC_AP_SD_WARN("%s[%d]: ignoring %.*s", key, i, SD_TOK_STR(d, k));
    }
  }
}

// fish_passes: per pass a list of [fish index, area] entries (area "" = every area) -> fish_area_passes bits.
// Missing key: nothing gated. Bad entries (and passes past AP_FISH_PASS_NUM) are skipped.
static void sd_fish_passes(const sd_doc * d, ap_slotdata * sd) {
  static const char * areas[AP_FISH_AREA_NUM] = { "river", "waterfall", "river_mouth", "lake", "pond", "sea" };
  int a, p, k, j, fish, area, v, exact, pass, entry;
  char name[16];
  memset(sd->fish_area_passes, 0, sizeof(sd->fish_area_passes));
  a = sd_obj_get(d, 0, "fish_passes");
  if (a < 0 || d->tok[a].type != JSMN_ARRAY) { return; }
  pass = a + 1;
  for (p = 0; p < AP_FISH_PASS_NUM && p < d->tok[a].size; p++, pass = sd_tok_end(d->tok, pass)) {
    if (d->tok[pass].type != JSMN_ARRAY) {
      PC_AP_SD_WARN("fish_passes[%d]: ignoring %.*s", p, SD_TOK_STR(d, pass));
      continue;
    }
    entry = pass + 1;
    for (k = 0; k < d->tok[pass].size; k++, entry = sd_tok_end(d->tok, entry)) {
      fish = -1;
      area = -2;
      if (d->tok[entry].type == JSMN_ARRAY && d->tok[entry].size == 2) {
        int fish_tok = entry + 1;
        int area_tok = sd_tok_end(d->tok, fish_tok);
        if (sd_tok_int(d, fish_tok, &v, &exact) && exact && v >= 0 && v < AP_CRITTER_NUM) { fish = v; }
        if (d->tok[area_tok].type == JSMN_STRING) {
          sd_decode(name, sizeof(name), d->json, &d->tok[area_tok]);
          if (name[0] == 0) { area = -1; } // "" = every area
          for (j = 0; j < AP_FISH_AREA_NUM; j++) {
            if (strcmp(name, areas[j]) == 0) { area = j; }
          }
        }
      }
      if (fish < 0 || area == -2) {
        PC_AP_SD_WARN("fish_passes[%d]: ignoring %.*s", p, SD_TOK_STR(d, entry));
        continue;
      }
      for (j = 0; j < AP_FISH_AREA_NUM; j++) {
        if (area == -1 || area == j) { sd->fish_area_passes[fish][j] |= 1 << p; }
      }
    }
  }
}

// npc indices from the apworld; bad entries are skipped (apworld/client mismatch)
static void sd_npc_list(const sd_doc * d, const char * key, unsigned char * out) {
  int a, i, k, v, exact;
  memset(out, 0, AP_NPC_NUM);
  a = sd_obj_get(d, 0, key);
  if (a < 0 || d->tok[a].type != JSMN_ARRAY) { return; }
  k = a + 1;
  for (i = 0; i < d->tok[a].size; i++, k = sd_tok_end(d->tok, k)) {
    if (sd_tok_int(d, k, &v, &exact) && v >= 0 && v < AP_NPC_NUM) {
      out[v] = 1;
    } else {
      PC_AP_SD_WARN("%s: ignoring bad entry %.*s", key, SD_TOK_STR(d, k));
    }
  }
}

// Radiosanity: the seed's song subset (indices 0-54). Bad entries and repeats are skipped, past AP_RADIO_SONG_NUM
// dropped. Not radiosanity_song_count: the client counts the list, so a short list can't ask for missing songs.
static void sd_radio_songs(const sd_doc * d, ap_slotdata * sd) {
  unsigned char seen[AP_RADIO_SONG_NUM];
  int a, i, k, v, exact;
  sd->radio_count = 0;
  memset(seen, 0, sizeof(seen));
  a = sd_obj_get(d, 0, "radiosanity_songs");
  if (a < 0 || d->tok[a].type != JSMN_ARRAY) { return; }
  k = a + 1;
  for (i = 0; i < d->tok[a].size; i++, k = sd_tok_end(d->tok, k)) {
    if (!sd_tok_int(d, k, &v, &exact) || v < 0 || v >= AP_RADIO_SONG_NUM || seen[v]) {
      PC_AP_SD_WARN("radiosanity_songs: ignoring %.*s", SD_TOK_STR(d, k));
    } else if (sd->radio_count >= AP_RADIO_SONG_NUM) {
      PC_AP_SD_WARN("radiosanity_songs: more than %d songs, ignoring %.*s", AP_RADIO_SONG_NUM, SD_TOK_STR(d, k));
    } else {
      seen[v] = 1;
      sd->radio_songs[sd->radio_count++] = (unsigned char)v;
    }
  }
}

// Shuffle Hourly Music: 24 songs, each of 1-24 once (song per hour). Anything else = all 0 (the game's own)
static void sd_hourly_music(const sd_doc * d, unsigned char * out) {
  unsigned char seen[AP_HOUR_NUM + 1];
  int a, h, k, v, exact;
  memset(out, 0, AP_HOUR_NUM);
  memset(seen, 0, sizeof(seen));
  a = sd_obj_get(d, 0, "hourly_music");
  if (a < 0 || d->tok[a].type != JSMN_ARRAY || d->tok[a].size == 0) { return; }
  if (d->tok[a].size != AP_HOUR_NUM) {
    PC_AP_SD_WARN("hourly_music: %d entries, not %d: ignored", d->tok[a].size, AP_HOUR_NUM);
    return;
  }
  k = a + 1;
  for (h = 0; h < AP_HOUR_NUM; h++, k = sd_tok_end(d->tok, k)) {
    if (!sd_tok_int(d, k, &v, &exact) || v < 1 || v > AP_HOUR_NUM || seen[v] != 0) {
      PC_AP_SD_WARN("hourly_music: bad entry %.*s: ignored", SD_TOK_STR(d, k));
      memset(out, 0, AP_HOUR_NUM);
      return;
    }
    seen[v] = 1;
    out[h] = (unsigned char)v;
  }
}

// Catalogsanity pools: a list of lists of game item ids. Bad entries are skipped (apworld/client mismatch),
// anything past AP_CATALOG_MAX is dropped
static void sd_catalog_pools(const sd_doc * d, ap_slotdata * sd) {
  int a, pool, k, item, v, exact;
  sd->catalog_count = 0;
  a = sd_obj_get(d, 0, "catalog_pools");
  if (a < 0 || d->tok[a].type != JSMN_ARRAY) { return; }
  item = a + 1;
  for (pool = 0; pool < d->tok[a].size; pool++, item = sd_tok_end(d->tok, item)) {
    if (d->tok[item].type == JSMN_ARRAY) {
      int items = item;
      int k2 = items + 1;
      for (k = 0; k < d->tok[items].size; k++, k2 = sd_tok_end(d->tok, k2)) {
        if (!sd_tok_int(d, k2, &v, &exact) || v <= 0 || v > 0xFFFF) {
          PC_AP_SD_WARN("catalog_pools: ignoring bad entry %.*s", SD_TOK_STR(d, k2));
        } else if (sd->catalog_count >= AP_CATALOG_MAX) {
          PC_AP_SD_WARN("catalog_pools: more than %d items, ignoring %.*s", AP_CATALOG_MAX, SD_TOK_STR(d, k2));
        } else {
          sd->catalog_items[sd->catalog_count] = (unsigned short)v;
          sd->catalog_pool[sd->catalog_count] = (unsigned char)(pool < 255 ? pool : 255);
          sd->catalog_count++;
        }
      }
    }
  }
}

// Furniture Set goal pieces: game item ids. Bad entries are skipped, past AP_FURNITURE_SET_MAX dropped
static void sd_furniture_set(const sd_doc * d, ap_slotdata * sd) {
  int a, i, k, v, exact;
  sd->furniture_set_count = 0;
  a = sd_obj_get(d, 0, "furniture_set_items");
  if (a < 0 || d->tok[a].type != JSMN_ARRAY) { return; }
  k = a + 1;
  for (i = 0; i < d->tok[a].size; i++, k = sd_tok_end(d->tok, k)) {
    if (!sd_tok_int(d, k, &v, &exact) || v <= 0 || v > 0xFFFF || sd->furniture_set_count >= AP_FURNITURE_SET_MAX) {
      PC_AP_SD_WARN("furniture_set_items: ignoring %.*s", SD_TOK_STR(d, k));
    } else {
      sd->furniture_set_items[sd->furniture_set_count++] = (unsigned short)v;
    }
  }
}

// Catalog Sources option set (names) -> AP_CATALOG_SOURCE_* bits; missing = none
static int sd_catalog_sources(const sd_doc * d) {
  // Same order as the bits (apworld options.CATALOG_SOURCES)
  static const char * names[] = { "Favors", "Trades", "Lost and Found", "Dump", "Letters", "Trees", "Presents",
                                  "Grab Bags", "Wisp" };
  char name[24];
  int a, i, k, n, bits = 0;
  a = sd_obj_get(d, 0, "catalog_sources");
  if (a < 0 || d->tok[a].type != JSMN_ARRAY) { return 0; }
  k = a + 1;
  for (i = 0; i < d->tok[a].size; i++, k = sd_tok_end(d->tok, k)) {
    if (d->tok[k].type != JSMN_STRING) { continue; }
    sd_decode(name, sizeof(name), d->json, &d->tok[k]);
    for (n = 0; n < (int)(sizeof(names) / sizeof(names[0])); n++) {
      if (strcmp(name, names[n]) == 0) { bits |= 1 << n; }
    }
  }
  return bits;
}

static int sd_goal(const sd_doc * d) {
  // Same names and bits as ap_slotdata_fill's read_goal
  static const char * names[] = { "Statue", "Museum", "Froggy Chair", "Catalog", "Music", "Furniture Set",
                                  "Chess Set" };
  static const int bits[] = { AP_GOAL_STATUE, AP_GOAL_MUSEUM, AP_GOAL_FROGGY, AP_GOAL_CATALOG, AP_GOAL_RADIO,
                              AP_GOAL_FURNITURE_SET, AP_GOAL_CHESS_SET };
  char name[16];
  int a, i, k, n, goal = 0;
  a = sd_obj_get(d, 0, "goal");
  if (a < 0 || d->tok[a].type != JSMN_ARRAY) { return AP_GOAL_STATUE; }
  k = a + 1;
  for (i = 0; i < d->tok[a].size; i++, k = sd_tok_end(d->tok, k)) {
    if (d->tok[k].type != JSMN_STRING) { continue; }
    sd_decode(name, sizeof(name), d->json, &d->tok[k]);
    for (n = 0; n < (int)(sizeof(names) / sizeof(names[0])); n++) {
      if (strcmp(name, names[n]) == 0) { goal |= bits[n]; }
    }
  }
  return goal;
}

// ---------------------------------------------------------------------------------------------------------------
// fill (the same order and defaults as ap_slotdata_fill)
// ---------------------------------------------------------------------------------------------------------------

// WARN: keep offline_example.json (apclient/) in sync with the keys that matter offline
// (new town, intro, QoL, loan amounts)
int pc_ap_slotdata_parse(const char * json, int len, ap_slotdata * out) {
  static jsmntok_t tokens[PC_AP_SD_MAX_TOKENS];
  static const int default_loans[AP_LOAN_NUM] = { 15000, 45000, 30000, 60000, 80000 };
  static const int default_loan_checks[AP_LOAN_NUM] = { 0, 0, 0, 0, 0 }; // missing = no loan checks
  static const int no_thresholds[AP_STORE_LEVELS] = { 0, 0, 0 };
  // WARN: copy of the apworld's FOSSIL_SEASONS (fossil_availability.py), the default for offline play
  static const int default_fossil_seasons[AP_FOSSIL_NUM] = {
    2, 2, 2, 2, 2, 2, // Tricera, T-rex: autumn
    1, 1, 1, 1, 1, 1, // Apato, Stego: summer
    0, 0, 0,          // Ptera: spring
    3, 3, 3, 3, 3,    // Plesio, Mammoth: winter
    3, 3,             // Amber, Dinosaur Track: winter
    0, 0, 0,          // Ammonite, Dinosaur Egg, Trilobite: spring
  };
  // Keyed by item name in the apworld (Small/Modest/Large Bell Credit)
  static const char * credit_names[AP_BELLCREDIT_NUM] = { "Small Bell Credit", "Modest Bell Credit",
                                                          "Large Bell Credit" };
  sd_doc d;
  jsmn_parser parser;
  int credits, favor_pools, i, n;
  if (json == NULL || out == NULL || len <= 0) { return 0; }
  jsmn_init(&parser);
  n = jsmn_parse(&parser, json, (size_t)len, tokens, PC_AP_SD_MAX_TOKENS);
  if (n < 1) {
    PC_AP_SD_WARN("slot_data: not JSON (%d) or more than %d tokens", n, PC_AP_SD_MAX_TOKENS);
    return 0;
  }
  if (tokens[0].type != JSMN_OBJECT) {
    PC_AP_SD_WARN("slot_data: not a JSON object");
    return 0;
  }
  d.json = json;
  d.tok = tokens;
  sd_top_index(&d); // one walk of the top-level keys: the fill's lookups are table reads from here on

  memset(out, 0, sizeof(*out)); // ap_slotdata_fill expects a zeroed struct (ap_slotdata_init)
  sd_str(out->town_name, sizeof(out->town_name), &d, "town_name", "pelago");
  sd_str(out->player_name, sizeof(out->player_name), &d, "player_name", "archi");
  out->skip_intro = sd_int(&d, "skip_intro", 2, 0, 2);
  out->gender = sd_int(&d, "gender", 0, 0, 1);
  out->face = sd_int(&d, "face", 0, 0, 7);
  out->starting_shirt = sd_int(&d, "starting_shirt", 16, 0, 16);
  out->house = sd_int(&d, "house", 0, 0, 3);
  out->town_fruit = sd_int(&d, "town_fruit", 0, 0, 4);
  out->grass_shape = sd_int(&d, "grass_shape", 0, 0, 2);
  out->train_station = sd_int(&d, "train_station", -1, -1, 14);
  out->town_day = sd_int(&d, "town_day", 0, 0, 31);
  out->letter_paper = sd_int(&d, "letter_paper", 0, 0, 63);
  sd_str(out->letter_sender, sizeof(out->letter_sender), &d, "letter_sender", "Archipelago");
  sd_str(out->loan_letter_text, sizeof(out->loan_letter_text), &d, "loan_letter_text",
      "Your loan is ready for\npayoff at the post office!");
  sd_npc_list(&d, "villager_blacklist", out->villager_blacklist);
  sd_npc_list(&d, "starting_villagers", out->starting_villagers);
  sd_hourly_music(&d, out->hourly_music);
  out->shops_always_open = sd_bool(&d, "shops_always_open", 0);
  out->global_storage = sd_int(&d, "global_storage", 0, 0, 2);
  out->no_weeds = sd_bool(&d, "no_weeds", 1);
  out->more_favors = sd_bool(&d, "more_favors", 1);
  out->villagers_dont_leave = sd_bool(&d, "villagers_dont_leave", 0);
  out->villagers_never_sleep = sd_bool(&d, "villagers_never_sleep", 0);
  out->turnips_never_spoil = sd_bool(&d, "turnips_never_spoil", 1);
  out->no_falling_stalks = sd_bool(&d, "no_falling_stalks", 0);
  out->skip_hra_talk = sd_bool(&d, "skip_hra_talk", 1);
  out->shop_never_sells_out = sd_bool(&d, "shop_never_sells_out", 0);
  out->coconuts_wash_up = sd_bool(&d, "coconuts_wash_up", 1);
  out->normalized_time_travel = sd_bool(&d, "normalized_time_travel", 1);

  // Missing keys: offline-relevant ones default to the apworld defaults, AP-only ones to off
  out->goal = sd_goal(&d);
  out->loansanity = sd_bool(&d, "loansanity", 1);
  sd_int_array(&d, "loans", out->loans, default_loans, AP_LOAN_NUM);
  sd_int_array(&d, "loan_checks", out->loan_checks, default_loan_checks, AP_LOAN_NUM);
  // Favor checks in total: Favor Pools x Favors per Pool, 0 with Favorsanity off
  favor_pools = sd_bool(&d, "favorsanity", 0) ? sd_int(&d, "favor_pools", 0, 0, 100) : 0;
  out->favors_per_pool = sd_int(&d, "favors_per_pool", 0, 0, 100); // 0: one pool, all open
  out->favorsanity = favor_pools * out->favors_per_pool;
  out->tools_in_pool = sd_bool(&d, "tools_in_pool", 0);
  out->store_upgrades = sd_int(&d, "nook_store_upgrades", AP_STORE_VANILLA, AP_STORE_VANILLA, AP_STORE_CATALOG);
  sd_int_array(&d, "nook_store_thresholds", out->store_thresholds, no_thresholds, AP_STORE_LEVELS);
  out->museumsanity = sd_bool(&d, "museumsanity", 0);
  out->bug_checks = sd_int(&d, "bug_checks", 0, 0, AP_MUSEUM_FIND | AP_MUSEUM_DONATE);
  out->fish_checks = sd_int(&d, "fish_checks", 0, 0, AP_MUSEUM_FIND | AP_MUSEUM_DONATE);
  out->fossil_checks = sd_int(&d, "fossil_checks", 0, 0, AP_MUSEUM_FIND | AP_MUSEUM_DONATE);
  out->painting_checks = sd_int(&d, "painting_checks", 0, 0, AP_MUSEUM_FIND | AP_MUSEUM_DONATE);
  out->museum_goal_count = sd_int(&d, "museum_goal_count", 0, 0, INT_MAX); // too high: goal never done
  out->critter_odds = sd_int(&d, "critter_odds", AP_CRITTER_ODDS_DYNAMIC, AP_CRITTER_ODDS_VANILLA,
      AP_CRITTER_ODDS_DYNAMIC);
  // Missing = all (older seeds have no bug / fish pass items)
  out->bug_availability = sd_int(&d, "bug_availability", AP_BUG_AVAIL_ALL, AP_BUG_AVAIL_ALL,
      AP_BUG_AVAIL_PROGRESSIVE);
  out->fish_availability = sd_int(&d, "fish_availability", AP_FISH_AVAIL_ALL, AP_FISH_AVAIL_ALL,
      AP_FISH_AVAIL_PROGRESSIVE);
  // Missing = all (0.3 seeds have no series items, progressive would never spawn dinosaurs)
  out->fossil_availability = sd_int(&d, "fossil_availability", AP_FOSSIL_AVAIL_ALL, AP_FOSSIL_AVAIL_ALL,
      AP_FOSSIL_AVAIL_PROGRESSIVE);
  out->fossil_odds = sd_int(&d, "fossil_odds", AP_FOSSIL_ODDS_DYNAMIC, AP_FOSSIL_ODDS_VANILLA,
      AP_FOSSIL_ODDS_DYNAMIC);
  sd_int_array(&d, "fossil_seasons", out->fossil_seasons, default_fossil_seasons, AP_FOSSIL_NUM);
  for (i = 0; i < AP_FOSSIL_NUM; i++) { out->fossil_seasons[i] &= 3; }
  sd_pass_array(&d, "bug_passes", out->bug_passes, AP_CRITTER_NUM, AP_BUG_PASS_NUM);
  sd_fish_passes(&d, out);
  sd_pass_array(&d, "fossil_series", out->fossil_series, AP_FOSSIL_NUM, AP_FOSSIL_SERIES_NUM);
  out->timesanity = sd_bool(&d, "timesanity", 0);
  out->starting_month = sd_int(&d, "starting_month", 0, 0, 11);
  out->starting_time = sd_int(&d, "starting_time", 0, 0, 3);
  // Same range as the game (mTM_MIN_YEAR / mTM_MAX_YEAR on PC)
  out->start_year = sd_int(&d, "start_year", 2001, 2001, 2100);
  out->catalogsanity = sd_bool(&d, "catalogsanity", 0);
  sd_catalog_pools(&d, out);
  out->new_item_chance = sd_int(&d, "new_item_chance", 0, 0, 100);
  out->catalog_sources = sd_catalog_sources(&d);
  out->catalog_goal_count = sd_int(&d, "catalog_goal_count", 0, 0, AP_CATALOG_MAX);
  sd_furniture_set(&d, out);
  out->beach_per_month = sd_bool(&d, "shellsanity", 0) ?
      sd_int(&d, "beach_checks_per_month", 0, 0, AP_BEACH_PER_MONTH_MAX) : 0;
  // Missing = off (older seeds)
  out->beach_hint_count = sd_int(&d, "beach_hint_count", 0, 0, INT_MAX);
  out->beach_hint_letter = sd_bool(&d, "beach_hint_letter", 0);
  out->shellsanity_item_chance = sd_int(&d, "shellsanity_item_chance", 20, 0, 100);
  out->tree_per_fruit = sd_bool(&d, "treesanity", 0) ?
      sd_int(&d, "tree_checks_per_fruit", 0, 0, AP_TREE_PER_FRUIT_MAX) : 0;
  out->radiosanity = sd_bool(&d, "radiosanity", 0);
  out->radiosanity_aircheck = sd_bool(&d, "radiosanity_aircheck", 0);
  if (out->radiosanity) {
    sd_radio_songs(&d, out);
  }
  out->radio_goal_count = sd_int(&d, "radio_goal_count", 0, 0, AP_RADIO_SONG_NUM);
  credits = sd_obj_get(&d, 0, "bell_credits");
  for (i = 0; i < AP_BELLCREDIT_NUM; i++) {
    int b = sd_obj_get(&d, credits, credit_names[i]);
    int v = 0;
    int exact = 1;
    out->bell_credits[i] = 0;
    if (b >= 0 && sd_tok_int(&d, b, &v, &exact)) { out->bell_credits[i] = v; }
  }
  sd_str(out->world_version, sizeof(out->world_version), &d, "world_version", "");
  out->valid = 1;
  return 1;
}
