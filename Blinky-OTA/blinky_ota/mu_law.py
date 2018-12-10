import sys
import numpy as np
import matplotlib.pyplot as plt

def dB(x):
    return 10 * np.log10(np.abs(x))

def dB_inv(x):
    return 10. ** (x / 10.)

def map(t, mi, ma):
    t_m = (t - mi) / (ma - mi)
    I_p = t_m > 1.
    t_m[I_p] = 1.
    I_m = t_m < 0.
    t_m[I_m] = 0.
    return t_m

def mu_law(x, mu):
    return np.log(1 + mu * x) / np.log(mu + 1)

def sigmoid(x, loc, scale):
    return 1. / (1. + np.exp(- (x - loc) / scale))

if __name__ == '__main__':

    mu = float(sys.argv[1])
    
    min_lin, max_lin = 5e-9, 5e-2
    min_log, max_log = dB(min_lin), dB(max_lin)

    x = np.linspace(min_log, max_log, 1000)
    x_lin = dB_inv(x)

    y_map = map(x_lin, min_lin, max_lin)
    y_mu = mu_law(y_map, mu)
    y_sig = sigmoid(x, np.mean([min_log, max_log]), (max_log - min_log) / 2 / 4)

    PWM_MAX = 4095

    plt.plot(x, PWM_MAX * y_mu, label='mu')
    plt.plot(x, PWM_MAX * y_sig, label='sigmoid')
    plt.legend()
    plt.xlabel('Input power')
    plt.ylabel('Output')
    plt.title('$\mu = {}$'.format(mu))
    plt.show()
