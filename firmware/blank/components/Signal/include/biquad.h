//
//  Created by Robin Scheibler
//  Copyright (c) 2018 Robin Scheibler. All rights reserved.
//
//  This is a class implementing a second-order section IIR filter
//

#ifndef __BIQUAD_H__
#define __BIQUAD_H__

class Biquad
{
  private:
    double registers[2];
    float a[2];
    float b[3];

  public:
    Biquad(float *b, float *a);
    float process(float sample);
};


#endif // __BIQUAD_H__
