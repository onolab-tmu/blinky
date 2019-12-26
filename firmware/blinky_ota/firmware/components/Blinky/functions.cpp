/*
 * This file contains the definitions of some of the functions
 * used in the blinky
 */
#include "blinky_functions.h"

float clip(float x, float floor, float ceil)
{
  if (x < floor)
    return floor;
  else if (x > ceil)
    return ceil;
  else
    return x;
}

float mu_law(float d, float mu)
{
  static float c = 1.f / logf(1.f + mu);
  return logf(1 + mu * d) * c;
}

float sigmoid(float d, float min, float max, float margin)
{
  static float x_h = logf( (1 - margin) / (1 - (1 - margin)) );
  static float loc = (max + min) / 2.;
  static float scale = (max - min) / 2. / x_h;
  return 1. / (1. + expf(- (d - loc) / scale));
}
