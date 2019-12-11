from scipy import signal
import numpy as np
import matplotlib.pyplot as plt

fs_target = 30
block_size = 64
f_cut = 35  # Hz

# 1) In-blinky filter characteristics
fs_in = 16000
fs_out = int(fs_in / block_size)
cut_off = 2 * f_cut / fs_in
attenuation_db = 40
#ftype = 'cheby2'
ftype = 'bessel'
N = 20

sos = signal.iirfilter(N, cut_off, rs=attenuation_db, btype='low',
                         analog=False, ftype=ftype, output='sos')

# print coeff for export to Cpp
for i, sec in enumerate(sos):
    print('// layer', i)
    print('float b{}[3] = {{{}, {}, {}}};'.format(i, *sec[:3]))
    print('float a{}[2] = {{{}, {}}};'.format(i, *sec[4:]))
print('// The vector of biquads')
print('vector<Biquad> biquads{ ', end='')
for i in range(len(sos)):
    final_comma = ',' if i != len(sos) - 1 else ''
    print('Biquad(b{}, a{}){} '.format(i, i, final_comma), end='')
print('};')

# impulse response
x = np.zeros(fs_in * 5)
x[0] = 1
T = np.arange(x.shape[0]) / fs_in
ir = signal.sosfilt(sos, x)

# frequency response
w, h = signal.sosfreqz(sos, 10000)
w = w / np.pi * fs_in * 0.5

# 2) Now estimate the impact of the box filter from the camera
fps = 30
box_filt_len = 1 / 30 / fs_in
h_sinc_camera = np.sinc(w / fps)

# 3) Just the block average filter
h_sinc_block = np.sinc(w / fs_out)


plt.figure()
plt.plot(T, ir)
plt.title('Impulse response of in-blinky filter')

fig = plt.figure()
ax = fig.add_subplot(111)
y_min = -100
l1 = 20 * np.log10(abs(h))
l2 = 20 * np.log10(abs(h * h_sinc_camera))
l3 = 20 * np.log10(abs(h_sinc_block * h_sinc_camera))
ax.plot(w, l1)
ax.plot(w, l2, 'r')
ax.plot(w, l3, 'g')
plt.ylim([y_min, 0])

f = fs_out
i = 0
while i * f < fs_in / 2:
    for j in [-1,1]:
        f0 = i * f + j * fs_target / 2
        if f0 < 0:
            continue
        plt.vlines(f0, y_min, 0, 'r')
    i += 1

ax.set_title('Chebyshev Type II bandpass frequency response')
ax.set_xlabel('Frequency [radians / second]')
ax.set_ylabel('Amplitude [dB]')
#ax.axis((10, 1000, -100, 10))
ax.grid(which='both', axis='both')
plt.show()
