#!/usr/bin/python

import sys
import matplotlib.pyplot as plt
import numpy as np
import csv
import heapq

clr = ['red', 'blue', 'black', 'green']

# get tcp types
tcp = ["NewReno"]#,"Inigo"]

# get metrics to plot
str = ['cwnd','ssth']

i = 0
for version in tcp:
    for metric in str:
        t = []
        met = []
        with open('TcpVariantsComparison_Tcp' + version + '-' + metric + '.data', 'rb') as f:
            reader = csv.reader(f, delimiter=' ')
            for row in reader:
                time = float(row[0])
                if 200 <= time <= 240:
                    t.append(time)
                    met.append(float(row[1])/1466)
                
        plt.plot(t, met, label=version + '-' + metric, color=clr[i])
        print version, metric, min(met), max(met)
        i += 1

plt.xlabel('Time (seconds)')
plt.ylabel('Segments')
plt.title('TCP Variants Metrics Comparison')
plt.legend(loc=0,prop={'size':6})


plt.savefig("TCPVariantsMetricsComparison.png")
plt.show()
