#!/usr/bin/python

import sys
import matplotlib.pyplot as plt
import numpy as np
import csv
import heapq

clr = ['red', 'blue', 'black']
logplot = False
c = 1

str_ylim = [False, False, False, False]
str_xlim = [False, False, False, False]
str_ylow = [0.0, 0.0, 0.0, 0.0]
str_yhigh = [0.0, 0.0, 0.0, 0.0]
str_xlow = [0.0, 0.0, 0.0, 0.0]
str_yhigh = [0.0, 0.0, 0.0, 0.0]

# get tcp types
tcp = []
var = ""
while var != "end":
    if var != "":
        tcp.append(var)
    var = raw_input("Tcp type (end when done): ")

# get metrics to plot
str = []
var = ""
while var != "end":
    if var != "":
        str.append(var)
    var = raw_input("metrics to plot - default is cwnd, ssth, rtt, rto (end when done): ")
if not str:
    str = ['cwnd', 'ssth', 'rtt', 'rto']

# log or normal plot
var = raw_input("type yes for a logplot: ")
if var == "yes":
    logplot = True

# set any axis limits
i = 0
for item in str:
    var = raw_input("type yes to set y axis limits for " + item + ": ")
    if var == "yes":
        str_ylim[i] = True
        var = raw_input("Specify y low :")
        str_ylow[i] = float(var)
        var = raw_input("Specify y high :")
        str_yhigh[i] = float(var)

    var = raw_input("type yes to set y axis limits for " + item + ": ")
    if var == "yes":
        str_xlim[i] = True
        var = raw_input("Specify x low :")
        str_xlow[i] = float(var)
        var = raw_input("Specify x high :")
        str_xhigh[i] = float(var)
    i = i + 1

for metric in str:

    rw_cnt = np.floor(np.sqrt(len(str)))
    plt.subplot(rw_cnt, np.ceil(len(str)/rw_cnt), c)    
    
    plt.tight_layout()
    plt.figure(1,1)
    for i, version in enumerate(tcp):
        t = []
        met = []
        with open('TcpVariantsComparison_Tcp' + version + '-' + metric + '.data', 'rb') as f:
            reader = csv.reader(f, delimiter=' ')
            for row in reader:
                t.append(float(row[0]))
                foo = float(row[1])
                if logplot:
                    met.append(np.log(foo))
                else:
                    met.append(foo)
                
        plt.plot(t, met, label=version, color=clr[i])
        if str_ylim[c-1]:
                plt.ylim(str_ylow[c-1],str_yhigh[c-1])
        if str_xlim[c-1]:
                plt.xlim(str_xlow[c-1],str_xhigh[c-1])
        print version, metric, min(met), max(met)
    
    plt.xlabel('time (s)')
    plt.ylabel(metric)
    plt.title('TCP Variants Metrics Comparison')
    plt.legend(loc=0,prop={'size':6})

    c = c + 1

plt.savefig("TCPVariantsMetricsComparison.png")
plt.show()
