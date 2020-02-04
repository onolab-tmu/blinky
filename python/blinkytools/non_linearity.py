# Copyright 2020 Robin Scheibler
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is furnished to do
# so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
import numpy as np

blinky_min_db = -80.
blinky_max_db = -10.

blinky_lut = np.array(
    [
        0.0, 0.008773874102714685, 0.017523573381825086, 0.02607612819282401, 0.034620136083068465,
        0.04331733831488338, 0.05230047386528325, 0.061675388827087674, 0.07152582470159385,
        0.08191960702011791, 0.09291522321689072, 0.10456801706542795, 0.11693543765260839,
        0.13008096516534118, 0.1440764950652642, 0.15900309789588218, 0.174950185372309,
        0.19201320590535764, 0.21029006568024444, 0.22987652620980514, 0.2508608672761513,
        0.2733181267338959, 0.29730423713112, 0.32285037488391344, 0.34995782217678406,
        0.37859361622309096, 0.4086872263716097, 0.44012845815276036, 0.4727667360874842,
        0.5064118652974956, 0.5408363170249617, 0.575779026456492, 0.610950634117771,
        0.64604004592588, 0.680722133122291, 0.7146663431266431, 0.747545947214615,
        0.7790476121989043, 0.8088809523456144, 0.8367876959550755, 0.8625500897419622,
        0.8859981647299889, 0.9070155011972668, 0.9255431586351102, 0.9415814810813828,
        0.9551895499249787, 0.9664821367163857, 0.9756241090261105, 0.982822364333689,
        0.9883155116706936, 0.9923616896472224, 0.9952251039284353, 0.9971620885614387,
        0.9984077451488259, 0.9991644930891165, 0.9995941743218074, 0.9998156985914106,
        0.9999105915463267, 0.9999392193803281, 0.999970911572436, 1.0,
    ]
)


def clip(x, lo=0.0, hi=1.0):

    y = x.copy()

    y[y > hi] = hi
    y[y < lo] = lo

    return y


def decibels(x):
    return 10. * np.log10(x)

def decibels_inv(y):
    return 10. ** (y / 10.)


def map_to_unit_interval(x, lo=0., hi=1.):
  """ Linearly map value in [lo_val, hi_val] to [0, 1] """
  return (x - lo) / (hi - lo)

def unmap_from_unit_interval(y, lo=0., hi=1.):
  """ Linearly map value in [0, 1] to [lo_val, hi_val] """
  return y * (hi - lo) + lo


def lut_interp(x, lut):

    x = clip(x, lo=0.0, hi=1.0)
    y = x.copy()

    # bin clipped at 1 should be left alone
    not_1 = y != 1

    # linear interpolation between bins
    p_f = x[not_1] * (len(lut) - 1)
    p = np.floor(p_f).astype(np.int)
    y[not_1] = (p_f - p) * (lut[p + 1] - lut[p]) + lut[p]

    return y


def lut_interp_inv(y, lut):

    y = clip(y, lo=0.0, hi=lut[-1])

    x = np.linspace(0, lut[-1], len(lut))

    y2 = y.flatten()
    output = np.zeros(y2.shape)

    for i in range(y2.shape[0]):
        k = np.where(y2[i] - lut[:-1] >= 0)[0][-1]

        a = (lut[k + 1] - lut[k]) / (x[k + 1] - x[k])
        b = lut[k]

        output[i] = (y2[i] - b) / a + x[k]

    return output.reshape(y.shape)


def blinky_non_linearity(x):
    """
    Non-linear function used in the Blinky

    Parameters
    ----------
    x: numpy.ndarray
        Array of values of signal power
    """
    global blinky_lut

    x = decibels(x)
    x = clip(x, lo=blinky_min_db, hi=blinky_max_db)
    x = map_to_unit_interval(x, lo=blinky_min_db, hi=blinky_max_db)
    x = lut_interp(x, blinky_lut)

    return x


def blinky_non_linearity_inv(y):
    """
    Inverse of the non-linear function used in Blinky

    Parameters
    ----------
    y: numpy.ndarray
        Array of values
    """
    global blinky_lut

    y = lut_interp_inv(y, blinky_lut)
    y = unmap_from_unit_interval(y, lo=blinky_min_db, hi=blinky_max_db)
    y = decibels_inv(y)

    return y
