#!/usr/bin/python

import sys
import matplotlib.pyplot as plt
import numpy as np
import csv
import heapq

tcp_ylim = [False, False, False]
tcp_xlim = [False, False, False]
tcp_ylow = [0.0, 0.0, 0.0]
tcp_yhigh = [0.0, 0.0, 0.0]
tcp_xlow = [0.0, 0.0, 0.0]
tcp_yhigh = [0.0, 0.0, 0.0]

# get tcp types                                                                 
tcp = []
var = ""
while var != "end":
    if var != "":
        tcp.append(var)
    var = raw_input("Tcp type (end when done): ")

clr = ['red', 'blue', 'black']
str = ['q']

# set any axis limits                                                           
i = 0
for item in tcp:
    var = raw_input("type yes to set y axis limits for " + item + ": ")
    if var == "yes":
        tcp_ylim[i] = True
        var = raw_input("Specify y low :")
        tcp_ylow[i] = float(var)
        var = raw_input("Specify y high :")
        tcp_yhigh[i] = float(var)

    var = raw_input("type yes to set y axis limits for " + item + ": ")
    if var == "yes":
        tcp_xlim[i] = True
        var = raw_input("Specify x low :")
        tcp_xlow[i] = float(var)
        var = raw_input("Specify x high :")
        tcp_xhigh[i] = float(var)
    i = i + 1

c = 1
for i, version in enumerate(tcp):
    
    plt.subplot(3,2,c)
    
    plt.tight_layout()
    plt.figure(1,1)
    for metric in str:
        t = []
        met = []
        j = 0
        with open('TcpVariantsComparison_Tcp' + version + '-' + metric + '.data', 'rb') as f:
            reader = csv.reader(f, delimiter=' ')
            for row in reader:
                t.append(float(row[0]))
                met.append(float(row[1]))
               
        plt.plot(t, met, label=version, color=clr[i])
        if tcp_ylim[i]:
            plt.ylim(tcp_ylow[i],tcp_yhigh[i])
        if tcp_xlim[i]:
            plt.xlim(tcp_xlow[i],tcp_xhigh[i])
        print version, metric, min(met), max(met)

    plt.xlabel('time (s)')
    plt.ylabel(metric)
    plt.title('TCP Variants Metrics Comparison')
    plt.legend(loc=0,prop={'size':6})

    c = c + 1

plt.savefig("TCPVariantsMetricsComparison.png")
plt.show()
