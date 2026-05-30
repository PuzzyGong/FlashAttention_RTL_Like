#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#ifndef HEAD_DIMENTION
#define HEAD_DIMENTION 4
#endif

#ifndef N
#define N 4
#endif

#ifndef TOKEN_COUNT
#define TOKEN_COUNT (N * HEAD_DIMENTION)
#endif

#define SRAM_SIZE (TOKEN_COUNT * HEAD_DIMENTION)

static_assert(HEAD_DIMENTION > 0, "HEAD_DIMENTION must be positive");
static_assert((HEAD_DIMENTION & (HEAD_DIMENTION - 1)) == 0, "HEAD_DIMENTION must be a power of two");
static_assert(N > 0, "N must be positive");
static_assert(TOKEN_COUNT > 0, "TOKEN_COUNT must be positive");
static_assert((TOKEN_COUNT % HEAD_DIMENTION) == 0, "TOKEN_COUNT must be divisible by HEAD_DIMENTION");

inline constexpr int TILE_SIZE = HEAD_DIMENTION;
inline constexpr uint8_t BYTE_INDEX_IDLE = static_cast<uint8_t>(TILE_SIZE + 1);

inline constexpr int SCORE_ROW_Y = HEAD_DIMENTION;
inline constexpr int MAX_OLD_X = HEAD_DIMENTION + 3;
inline constexpr int EXP_X = 2 * HEAD_DIMENTION + 5;
