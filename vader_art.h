#pragma once
// Darth Vader Braille art — 11 lines (line 0 is blank), max 29 chars wide
// Render with dotW=2, dotH=2 → 116px wide × 88px tall
static const char* const VADER_BRAILLE[] = {
  "",
  "⠀⠀⠀⠀⠀⣠⣴⣶⣯⠪⣕⢶⣦⣔⢄⠀⠀",
  "⠀⠀⠀⢀⣼⣿⣿⣿⣿⣧⡙⣧⢹⣿⣷⣇⠀",
  "⠀⠀⠀⣸⣿⣿⣿⣿⡟⠛⢿⣾⢿⡟⠟⢛⡄⠀",
  "⠀⠀⠀⣿⣿⣿⣿⢟⣯⢖⣒⣚⣭⠀⣣⣈⡨⣢⠀⠀⠀⠀⠀⠀",
  "⠀⠀⠀⣿⣿⣿⢏⡛⠱⢿⣧⣿⢿⡂⠻⠭⠿⣴⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
  "⠀⠀⣰⣿⣿⡟⢼⣿⡶⡄⣴⣶⣶⠇⠀⢶⣶⡎⡗⠀",
  "⠀⢠⣿⣿⣿⢇⣷⣭⣃⠈⠙⠁⣠⢟⡟⡷⡙⢸⣷⠃",
  "⢀⣿⣿⠿⢟⣸⣷⠶⠯⠍⠀⡫⢬⣬⣤⣥⡅⣊⣿⣼",
  "⡜⣫⣴⣿⣿⣿⠁⢰⣿⣿⣿⣿⣞⠿⢛⣵⣾⡿⠛⠁",
  "⠙⠿⠿⠿⣿⣿⣼⣬⣿⣿⣿⣿⣿⣷⠟⠉⠁",
};
static const int VADER_BRAILLE_LINES  = 11;
static const int VADER_BRAILLE_DOT_W  = 2;
static const int VADER_BRAILLE_DOT_H  = 2;
static const int VADER_BRAILLE_MAX_W  = 29;  // max chars per line
