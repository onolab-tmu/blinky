//
//  Created by Robin Scheibler
//  Copyright (c) 2018 Robin Scheibler. All rights reserved.
//
//  This is a class implementing a DC removal notch filter
//

#ifndef __NOTCH_H__
#define __NOTCH_H__

class DCRemoval
{
  private:
    float x_reg;
    float y_reg;
    float alpha;

  public:
    DCRemoval(double _alpha) : x_reg(0.), y_reg(0.), alpha(_alpha) {}
    float process(float sample)
    {
      float output = sample - x_reg + alpha * y_reg;
      x_reg = sample;
      y_reg = output;
      return output;
    }
};

#endif // __NOTCH_H__
