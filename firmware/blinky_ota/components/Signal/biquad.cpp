//
//  Created by Robin Scheibler
//  Copyright (c) 2018 Robin Scheibler. All rights reserved.
//

#include "biquad.h"

Biquad::Biquad(float *_b, float *_a)
{
  registers[0] = registers[1] = 0;
  for (int i = 0 ; i < 3 ; i++)
    b[i] = _b[i];
  for (int i = 0 ; i < 2 ; i++)
    a[i] = _a[i];
}

float Biquad::process(float sample)
{
  float output = b[0] * sample + registers[0];
  registers[0] = registers[1] + (double)(b[1] * sample - a[0] * output);
  registers[1] = (double)(b[2] * sample - a[1] * output);

  return output;
}
