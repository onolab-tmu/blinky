/*
 * This class implements a simple ramp signal that counts
 * from 0 to 1 in a given amount of time
 */
#ifndef __RAMP_H__
#define __RAMP_H__

#include <cassert>

class Ramp
{
  // constants that define the signal
  float slope = 1.;

  // state
  float time = 0.;  // the time axis
  float value = 0.;

  public:
    Ramp(float _slope, float _now) : slope(_slope), time(_now) { }

    float get_value() const { return value; }

    void reset(float _now)
    {
      time = _now;
      value = 0.;
    }

    void update(float _now)
    {
      // time difference since last update
      float delta = _now - time;

      // update the value
      value += slope * delta;

      // udpate state
      time = _now;
    }

};

#endif  // __RAMP_H__
