import sys
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy
#import matplotlib
from collections import OrderedDict

tableau20 = None

COLORZ = True

NAME_MAP = {'PQ\n': 'TPriorityQueue', 'It\n': 'Iterators using standard algorithms'}
PERM = [0, 1]


LINE = True

def settings():
    # These are the "Tableau 20" colors as RGB.  
    global tableau20
    tableau20 = [(31, 119, 180),(214, 39, 40), (255, 152, 150),
             (148, 103, 189), (197, 176, 213), (140, 86, 75), (196, 156, 148),  
             (227, 119, 194), (247, 182, 210), (127, 127, 127), (199, 199, 199),  
             (188, 189, 34), (219, 219, 141), (23, 190, 207), (158, 218, 229)]  
  
    # Scale the RGB values to the [0, 1] range, which is the format matplotlib accepts.  
    for i in range(len(tableau20)):  
        r, g, b = tableau20[i]  
        tableau20[i] = (r / 255., g / 255., b / 255.)

def median(l):
    return sorted(l)[len(l)/2]

def permute(perm, l):
  l2 = list(l)
  newl = []
  for p in perm:
    newl.append(l2[p])
  return newl

def get_loc():
    return 'upper right'

def line_graph(data, labels, title, colors):
    fig = plt.figure(figsize = (8, 4))
    ax = fig.add_subplot(111)
    x = [int(l[:2]) for l in labels]
    for ((k, d), color) in zip(permute(PERM,data.iteritems()), colors):
        plt.plot(x, d.values(), marker='s', color=color, label=NAME_MAP[k])
    plt.xlabel('Threads')
    plt.title(title)
    plt.legend(loc=get_loc(), prop={'size': 10})
    ax.yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, pos: str(int(x/1000))))
    plt.ylabel('Thousands of transactions per second', rotation=90)
    ax.set_ylim(ymin=0)
    ax.set_title(title)

    return plt



def parse(dat):
    f = open(dat, 'r')
    first = True
    labels = []
    lines = f.readlines()
    data = OrderedDict()
    min = OrderedDict()
    max = OrderedDict()
    scale_by = None
    cur_name = None
    it = iter(xrange(len(lines)))
    for i in it:
        if first:
            cur_name = lines[i]
            labels.append(lines[i])
            first = False
        elif lines[i] == '\n':
            for d in data.values():
                if not d.has_key(cur_name):
                    d[cur_name] = 0
            for d in min.values():
                if not d.has_key(cur_name):
                    d[cur_name] = 0
            for d in max.values():
                if not d.has_key(cur_name):
                    d[cur_name] = 0

            first = True
            scale_by = None
        else:
            print lines[i], lines[i+1]
            if not scale_by: 
                scale_by = 1
#float(lines[i+1])
            if not data.has_key(lines[i]):
                data[lines[i]] = OrderedDict()
                min[lines[i]] = OrderedDict()
                max[lines[i]] = OrderedDict()
                # initialize previously missed values to 0
                for l in labels:
                    data[lines[i]][l] = 0
                    min[lines[i]][l] = 0
                    max[lines[i]][l] = 0
            raw_dat = eval(lines[i+1]) # very safe lol
            throughputs = raw_dat
            print "XXXXX"
            print throughputs
            print median(throughputs)
            print numpy.amin(throughputs)
            print numpy.amax(throughputs)
            data[lines[i]][cur_name] = median(throughputs) / scale_by
            min[lines[i]][cur_name] = numpy.amin(throughputs)/scale_by
            max[lines[i]][cur_name] = numpy.amax(throughputs)/scale_by
            next(it)

    print data

    return data, min, max, labels


def plot(data, min, max, labels, title):
    fig, ax = plt.subplots()
    
    cur_width = 0
    if COLORZ:
        colors = tableau20
    else:
        colors = [(0,0,0), (.2,.2,.2), (.4,.4,.4), (.6,.6,.6), (.8,.8,.8)]

    if LINE:
        return line_graph(data, labels, title, colors)

    n_datapoints = len(data.values()[0].values())
    inds = numpy.arange(n_datapoints)
    n_types = len(data.values())
    width = .7 / n_types
    bars = []
    for (d, a, b, c) in zip(permute(PERM, data.values()), permute(PERM, min.values()), permute(PERM, max.values()), colors):
        pts = d.values()
        minerr = numpy.subtract(pts, a.values())
        maxerr = numpy.subtract(b.values(), pts)
        bars.append(ax.bar(inds + cur_width + .1, pts, width, color=c, yerr = [minerr, maxerr], ecolor = 'k'))
        cur_width += width

    ax.set_xticks(inds + width * n_types / 2.0)
    ax.set_xticklabels(labels, size=10)
    ax.set_ylabel('Thousands of transactions per second' + ' per core' if SCALE_CORE else '', rotation=90)
    ax.yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, pos: str(int(x/1000))))
    if not SCALE_CORE:
        ax.set_aspect(1/900000. * 2)
    # why does this work but not set_ylabel...
    plt.ylabel('Thousands of transactions per second', rotation=90)
    plt.xlim()
    ax.legend([x[0] for x in bars], [NAME_MAP[x] for x in permute(PERM,data.keys())], ncol=4, loc=get_loc(), prop={'size': 10})
    #loc='lower center', bbox_to_anchor=(.9, .05) )

    ax.set_title(title)

    plt.tight_layout()

    return plt

try:
    title = sys.argv[3]
except:
    title = ''

try:
    COLORZ = sys.argv[4] != 'bw'
except:
    pass

try:
    SCALE_CORE = sys.argv[5] == 'core'
except:
    pass

try:
    LINE = sys.argv[6] == 'line'
except:
    pass

settings()
data, min, max, labels = parse(sys.argv[1])
plot(data, min, max, labels, title).savefig(sys.argv[2])
