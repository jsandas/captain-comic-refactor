#ifndef PREF_PATHS_H
#define PREF_PATHS_H

// SDL pref-path identity for user-writable app data (KEYS.DEF, COMIC.HGH).
// Prefixed per platform by SDL_GetPrefPath(); the app name keeps the
// historical value "comic-modernization" so existing user files stay valid.
constexpr char PREF_PATH_ORG[] = "jsandas";
constexpr char PREF_PATH_APP[] = "comic-modernization";

#endif  // PREF_PATHS_H
