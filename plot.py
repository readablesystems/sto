import sys
import matplotlib.pyplot as plt

def settings():
    #plt.xlim(xmin=0)
    plt.ylim(ymin=0)
    plt.xlim(xmin=-1)
    plt.xlim(xmax=103)

def plot(dat,style,n):
    x = []
    for i in xrange(11):
        for j in xrange(5):
            x.append(i*10+j*.1+n*.5)
    f = open(dat, 'r')
    y = map(float, f.readlines())
    p, = plt.plot(x,y,style,linestyle='None')
    return p


colors = ['bs', 'go', 'r^']
dat = [sys.argv[1], sys.argv[2], sys.argv[3]]
plots = []
n=0
for i,j in zip(dat,colors):
    plots.append(plot(i,j,n))
    n+=1

plt.legend(plots, ['Read-my-writes', "Don't read-my-writes", 'Optimized read-my-writes'], loc='lower right')
plt.xlabel('Write percentage')
plt.ylabel('Time (s)')
settings()
plt.show()
