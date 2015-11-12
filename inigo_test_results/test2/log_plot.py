#!/usr/bin/python

import sys
import matplotlib.pyplot as plt
import numpy as np
import csv
import heapq


tcp = ['NewReno']#, 'Inigo', 'Westwood'] #['NewReno', 'Inigo', 'Westwood']#, 'WestwoodPlus']
    
for version in tcp:
    t = []
    met = []
    ssthresh = []
    i = 0
    with open('variants_Tcp' + version, 'rb') as f:
        reader = csv.reader(f, delimiter=' ')
        for row in reader:
            if (len(row) > 4):
                if row[4] == 'cwnd':
                    if row[5] != 'and':
                        t.append(i)
                        met.append(float(row[5]))
                        if (row[7] == '4194240'):
                            ssthresh.append(-1)
                        else:
                            ssthresh.append(float(row[7]))
                        i = i + 1
                
    tt = [float(x)/i for x in t]
    plt.plot(tt, ssthresh, label=version)
                
    print i, version,  min(met), max(met)

plt.xlabel('time (????)')
plt.ylabel('ssthresh')
plt.title('TCP Variants Metrics Comparison')
plt.legend()

plt.savefig("log2_TCPVariantsMetricsComparison.png")
plt.show()

