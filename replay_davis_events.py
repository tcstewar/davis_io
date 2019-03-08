import numpy as np
import matplotlib.pyplot as plt
import sys
import timeit
import os
import traceback

class Trace(object):
    def __init__(self, params):
        self.times = []
        self.params = {}
        for p in params:
            self.params[p] = []
    def frame(self, t, **params):
        self.times.append(t)
        for p in self.params.keys():
            if p not in params:
                self.params[p].append(self.params[p][-1])
            else:
                self.params[p].append(params[p])

    def get(self, t, p):
        if t < self.times[0]:
            return None
        elif t > self.times[-1]:
            return None
        else:
            return np.interp([t], self.times, self.params[p])[0]


def load_trace(fn):
    if not os.path.isfile(fn):
        return None
    else:
        with open(fn) as f:
            code = f.read()
        locals = dict()
        globals = dict(Trace=Trace)
        try:
            exec(code, globals, locals)
        except:
            traceback.print_exc()
            return None
        for k, v in locals.items():
            if isinstance(v, Trace):
                return v
        else:
            return None

            


packet_size = 8
display_mode = 'quick'

decay_time = 0.01   # seconds

filename = sys.argv[1]

with open(filename, 'rb') as f:
    data = f.read()
data = np.fromstring(data, np.uint8)

data = data[:packet_size*10000000]


# First, grab all the data from the file

for i in range(10):
    print(data[i*packet_size:(i+1)*packet_size])


# find x and y values for events
y = ((data[1::packet_size].astype('uint16')<<8) + data[::packet_size]) >> 2
x = ((data[3::packet_size].astype('uint16')<<8) + data[2::packet_size]) >> 1
print(y.max(), x.max())
# get the polarity (+1 for on events, -1 for off events)
p = np.where((data[::packet_size] & 0x02) == 0x02, 1, -1)
v = np.where((data[::packet_size] & 0x01) == 0x01, 1, -1)
print(p.min(), v.min())
print(p.max(), v.max())
# find the time stamp for each event, in seconds from the start of the file
t = data[7::packet_size].astype(np.uint32)
t = (t << 8) + data[6::packet_size]
t = (t << 8) + data[5::packet_size]
t = (t << 8) + data[4::packet_size]
#t = t - t[0]
t = t.astype(float) / 1000000   # convert microseconds to seconds

print(t[0], t[-1])

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
pltimg = plt.imshow(image, vmax=1, vmin=-1,
                               interpolation='none', cmap='binary', aspect='auto')
plt.xlim(0, 240)
plt.ylim(180, 0)

plt_title = plt.title('')

pts = plt.scatter([0], [0], s=100, c='yellow')

event_index = 0   # for keeping track of where we are in the file

last_real_time = timeit.default_timer()
last_event_time = 0

while True:

    trace = load_trace(filename+'.label')

    if trace is not None:
        xx = trace.get(t[event_index], 'x')
        yy = trace.get(t[event_index], 'y')
        rr = trace.get(t[event_index], 'r')
        if xx is not None and yy is not None and rr is not None:
            #ball.center = xx, yy
            #ball.set_radius(rr)
            #ball.set_alpha(0.2)
            pts.set_offsets(np.array([[xx],[yy]]).T)
            pts.set_color((1.0, 1.0, 0.0, 0.3))

            ax = plt.gca()
            ppd=72./ax.figure.dpi
            trans = ax.transData.transform
            s = ((trans((1,rr))-trans((0,0)))*ppd)[1]
            pts.set_sizes([s**2])
        else:
            pts.set_color((1.0, 1.0, 0.0, 0.0))
            #ball.set_alpha(0.0)
    else:
        pts.set_color((1.0, 1.0, 1.0, 0.0))
        #ball.set_alpha(0.0)


    real_now = timeit.default_timer()
    real_dt = real_now - last_real_time
    last_real_time = real_now

    rate = rates[rate_index]
    event_dt = real_dt * rate

    if event_dt != 0:
        decay_scale = 1-np.abs(event_dt)/(np.abs(event_dt)+decay_time)
        image *= decay_scale

    if event_dt > 0:
        count = np.searchsorted(t[event_index:], last_event_time + event_dt)
        s = slice(event_index, event_index+count)

        dts = event_dt-(t[s]-last_event_time)
        if decay_time > 0:
            image[y[s], x[s]] += p[s] * (1-dts / (dts+decay_time))
        else:
            image[y[s], x[s]] += p[s]        
        event_index += count

        last_event_time += event_dt
        if last_event_time >= t[-1]:
            last_event_time = t[-1]
            rate_index = len(rates) // 2

    if event_dt < 0:
        count = np.searchsorted(t[:event_index], last_event_time + event_dt)
        s = slice(count, event_index)
        dts = -event_dt-(last_event_time - t[s])
        if decay_time > 0:
            image[y[s], x[s]] += p[s] * (1-dts / (dts+decay_time))
        else:
            image[y[s], x[s]] += p[s]
        
        event_index = count

        last_event_time += event_dt
        if last_event_time <= 0:
            last_event_time = 0
            rate_index = len(rates) // 2

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


