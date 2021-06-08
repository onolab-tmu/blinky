/*
 * This file defines a few non-linear functions
 */
#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <cmath>
#include <array>

float blinky_non_linearity(float frac);
float clip(float x, float floor, float ceil);
float mu_law(float d, float mu);
float sigmoid(float d, float min, float max, float margin);

template<class T>
T map_to_unit_interval(T val, T lo_val, T hi_val)
{
  // Linearly map value in [lo_val, hi_val] to [0, 1]
  return (val - lo_val) / (hi_val - lo_val);
}

template<class T>
T decibels(T val)
{
  return 10. * log10(val);
}

template<class T, size_t N>
T lut_interp(T frac, const std::array<T, N> table)
{
  /*
   * This function implements linear interpolation between values of a look up table
   * The input parameter `frac` is clipped in the interval (0, 1).
   */
  frac = clip(frac, 0., 1.);

  T p_f = (frac * (N - 1));
  int p = (int)(p_f);

  return (p_f - p) * (table[p+1] - table[p]) + table[p];
}

#endif  // __FUNCTIONS_H__
