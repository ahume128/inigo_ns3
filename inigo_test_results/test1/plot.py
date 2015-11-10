#!/usr/bin/python

import sys
import matplotlib.pyplot as plt
import numpy as np
import csv
import heapq


tcp = ['NewReno', 'Inigo', 'Westwood', 'WestwoodPlus']
str = ['cwnd', 'ssth', 'rtt', 'rto']

c = 1
for metric in str:

    rw_cnt = np.floor(np.sqrt(len(str)))
    plt.subplot(rw_cnt, len(str) - rw_cnt, c)    
    
    for version in tcp:
        t = []
        met = []
        with open('TcpVariantsComparison_Tcp' + version + '-' + metric + '.data', 'rb') as f:
            reader = csv.reader(f, delimiter=' ')
            for row in reader:
                t.append(float(row[0]))
                met.append(float(row[1]))
               
        plt.plot(t, met, label=version)

        print version, metric, min(met), max(met)

    plt.xlabel('time (????)')
    plt.ylabel(metric)
    plt.title('TCP Variants Metrics Comparison')
    plt.legend()

    c = c + 1

plt.savefig("TCPVariantsMetricsComparison.png")
plt.show()
