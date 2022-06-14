/**
 * @file
 * @brief Macro helpers for the "do-while(false)" idiom
 */
#pragma once

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BREAK_ON_FALSE(cond) \
  if (!(cond))               \
  break

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BREAK_ON_TRUE(cond) BREAK_ON_FALSE(!(cond))
