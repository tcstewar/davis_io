import numpy as np
import matplotlib.pyplot as plt
import sys
import timeit
import os
import traceback



packet_size = 4+180*240*2
display_mode = 'quick'

filename = sys.argv[1]

with open(filename, 'rb') as f:
    data = f.read()
data = np.fromstring(data, np.uint8)


t = data[3::packet_size].astype(np.uint32)
t = (t << 8) + data[2::packet_size]
t = (t << 8) + data[1::packet_size]
t = (t << 8) + data[0::packet_size]
t = t.astype(float) / 1000000   # convert microseconds to seconds


rates = [-8, -4, -2, -1, -0.5, -0.1, -0.01, -0.001, 0, 0.001, 0.01, 0.1, 0.5, 1, 2, 4, 8]
rate_index = len(rates) // 2


def press(event):
    global rate_index

    if event.key == 'right':
        if rate_index < len(rates)-1:
            rate_index += 1
    elif event.key == 'left':
        if rate_index > 0:
            rate_index -= 1


# Now create the visualization

image = np.zeros((180, 240), dtype=float)
fig, ax = plt.subplots()
fig.canvas.mpl_connect('key_press_event', press)
# so quick mode can run on ubuntu
plt.show(block=False)
plt.ion()
pltimg = plt.imshow(image, vmax=1, vmin=0,
                               interpolation='none',
                               cmap='binary_r',
                               aspect='auto')
plt.xlim(0, 240)
plt.ylim(180, 0)

plt_title = plt.title('')

event_index = 0   # for keeping track of where we are in the file

last_real_time = timeit.default_timer()
last_event_time = 0

while True:

    real_now = timeit.default_timer()
    real_dt = real_now - last_real_time
    last_real_time = real_now

    rate = rates[rate_index]
    event_dt = real_dt * rate
    last_event_time += event_dt

    index = np.searchsorted(t, last_event_time)
    if index >= len(t):
        last_event_time = t[-1]
        rate_index = len(rates) // 2
        index = len(t) - 1

    

    d = data[index*packet_size+4:(index+1)*packet_size]
    high = d[1::2]
    low = d[0::2]
    v = high.astype(int)<<8 + low
    v = v.astype(float).reshape(180,240)
    print(v.shape, v.max(), v.min())



    if v.max() > 0:
        image[:] = v / v.max()


    plt_title.set_text('rate:%g  time:%1.3f' % (rate, last_event_time))

    # update the display
    pltimg.set_data(image)

    if display_mode == 'quick':
        # this is faster, but doesn't work on all systems
        fig.canvas.draw()
        fig.canvas.flush_events()

    elif display_mode == 'ubuntu_quick':
        # this is even faster, but doesn't work on all systems
        ax.draw_artist(ax.patch)
        ax.draw_artist(img)
        ax.draw_artist(scatter)
        fig.canvas.update()

        fig.canvas.flush_events()
    else:
        # this works on all systems, but is kinda slow
        plt.pause(1e-8)


