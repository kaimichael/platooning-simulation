#!/usr/bin/env python3
import os, sys, argparse
parser = argparse.ArgumentParser(description='Make multiple measurements in ns3')
parser.add_argument('-p', default='rate', choices=['rate', 'interval', 'packet_length', 'num_vehicles', 'max_loss', 'max_delay'], help='Set the changing parameter (rate, interval, packet_length, num_vehicles, max_loss, max_delay)')
parser.add_argument('-c', default=1, type=int, choices=[0,1], help='Set network coding to true or false')
parser.add_argument('-f', default=0, type=int, choices=[0,1], help='Set forward all to true or false')
parser.add_argument('-z', default=0, type=int, choices=[0,1], help='Set no forward to true or false')
parser.add_argument('-r', default=6, choices=[3,4.5,6,9,12,18,24,27], type=float, help='Set the MCS rate')
parser.add_argument('-i', default=0.05, type=float, help='Set the packet interval')
parser.add_argument('-l', default=300, type=int, help='Set the packet length')
parser.add_argument('-n', default=10, type=int, help='Set the number of vehicles')
parser.add_argument('-x', default=10, type=int, help='Set the max loss in %')
parser.add_argument('-d', default=10000, type=int, help='Set the maximum delay in us')
args=parser.parse_args()
if (args.c==1):
    networkCoding="true"
else:
    networkCoding="false"
if (args.f==1):
    fwdAll="true"
else:
    fwdAll="false"
if (args.z==1):
    noForward="true"
else:
    noForward="false"
switcher = {
        3: "OfdmRate3MbpsBW10MHz",
        4.5: "OfdmRate4_5MbpsBW10MHz",
        6: "OfdmRate6MbpsBW10MHz",
        9: "OfdmRate9MbpsBW10MHz",
        12: "OfdmRate12MbpsBW10MHz",
        18: "OfdmRate18MbpsBW10MHz",
        24: "OfdmRate24MbpsBW10MHz",
        27: "OfdmRate27MbpsBW10MHz",
    }
switcher2 = {
        0: "OfdmRate3MbpsBW10MHz",
        1: "OfdmRate4_5MbpsBW10MHz",
        2: "OfdmRate6MbpsBW10MHz",
        3: "OfdmRate9MbpsBW10MHz",
        4: "OfdmRate12MbpsBW10MHz",
        5: "OfdmRate18MbpsBW10MHz",
        6: "OfdmRate24MbpsBW10MHz",
        7: "OfdmRate27MbpsBW10MHz",
    }
rateString = switcher.get(args.r, "Invalid rate")
if args.n>9:
    vehicles_number_string="{}".format(args.n)
else:
    vehicles_number_string="0{}".format(args.n)

if (args.p=='rate'):
    meas_count = 8
    i = 0
    for i in range(0,meas_count):
        rateString = switcher2.get(i, "Invalid rate")
        logFile="vehicles_{}_rate_{}_length_{}_interval_{}_loss_{}_delay_{}_nc_{}_fwdAll_{}_noForward_{}".format(args.n, rateString, args.l, args.i, args.x, args.d, networkCoding, fwdAll, noForward)

        fwdFile="fwd_{}".format(logFile)
        rcvFile="rcv_{}".format(logFile)
        sendFile="send_{}".format(logFile)

        cmd="./waf build --run 'platooning -packetSize={} -numPackets=10000 \
        -numVehicles={} \
        -traceFile=/home/kai/repos/sumo/projects/ns2mobility_{}_vehicles.tcl \
        -maxTransmissionDelay={} -interval={} \
        -maxPacketLoss={} -fwdLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -recvLogFile=/media/kai/4814A63F14A6303C/measurements/{} -sendLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -phyMode={} -networkCoding={} -forwardAll={} -noForward={}'".format(args.l, args.n, \
        vehicles_number_string, args.d, args.i, args.x, fwdFile, rcvFile, sendFile, \
        rateString, networkCoding, fwdAll, noForward)
        os.system(cmd)
        
elif (args.p=='interval'):
    meas_count = 9
    i = 0
    for i in range(0,meas_count):
        if i==0:
            interval=0.01
        elif i==1:
            interval=0.02
        elif i==2:
            interval=0.025
        elif i==3:
            interval=0.03
        elif i==4:
            interval=0.05
        elif i==5:
            interval=0.1
        elif i==6:
            interval=0.2
        elif i==7:
            interval=0.5
        elif i==8:
            interval=1.0
        logFile="vehicles_{}_rate_{}_length{}_interval{}_loss_{}_delay_{}_nc_{}_fwdAll_{}_noForward_{}".format(args.n, rateString, args.l, interval, args.x, args.d, networkCoding, fwdAll, noForward)

        fwdFile="fwd_{}".format(logFile)
        rcvFile="rcv_{}".format(logFile)
        sendFile="send_{}".format(logFile)

        cmd="./waf build --run 'platooning -packetSize={} -numPackets=10000 \
        -numVehicles={} \
        -traceFile=/home/kai/repos/sumo/projects/ns2mobility_{}_vehicles.tcl \
        -maxTransmissionDelay={} -interval={} \
        -maxPacketLoss={} -fwdLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -recvLogFile=/media/kai/4814A63F14A6303C/measurements/{} -sendLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -phyMode={} -networkCoding={} -forwardAll={} -noForward={}'".format(args.l, args.n, \
        vehicles_number_string, args.d, interval, args.x, fwdFile, rcvFile, sendFile, \
        rateString, networkCoding, fwdAll, noForward)
        os.system(cmd)
elif (args.p=='packet_length'):
    meas_count = 14
    i = 0
    for i in range(0,meas_count):
        length= 100 * (i+1)
        logFile="vehicles_{}_rate_{}_length{}_interval{}_loss_{}_delay_{}_nc_{}_fwdAll_{}_noForward_{}".format(args.n, rateString, length, args.i, args.x, args.d, networkCoding, fwdAll, noForward)

        fwdFile="fwd_{}".format(logFile)
        rcvFile="rcv_{}".format(logFile)
        sendFile="send_{}".format(logFile)

        cmd="./waf build --run 'platooning -packetSize={} -numPackets=10000 \
        -numVehicles={} \
        -traceFile=/home/kai/repos/sumo/projects/ns2mobility_{}_vehicles.tcl \
        -maxTransmissionDelay={} -interval={} \
        -maxPacketLoss={} -fwdLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -recvLogFile=/media/kai/4814A63F14A6303C/measurements/{} -sendLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -phyMode={} -networkCoding={} -forwardAll={} -noForward={}'".format(length, args.n, \
        vehicles_number_string, args.d, args.i, args.x, fwdFile, rcvFile, sendFile, \
        rateString, networkCoding, fwdAll, noForward)
        os.system(cmd)
elif (args.p=='num_vehicles'):
    i = 3
    for i in range(3,21):
        if i>9:
            vehicles_number_string="{}".format(i)
        else:
            vehicles_number_string="0{}".format(i)
        logFile="vehicles_{}_rate_{}_length{}_interval{}_loss_{}_delay_{}_nc_{}_fwdAll_{}_noForward{}".format(i, rateString, args.l, args.i, args.x, args.d, networkCoding, fwdAll, noForward)

        fwdFile="fwd_{}".format(logFile)
        rcvFile="rcv_{}".format(logFile)
        sendFile="send_{}".format(logFile)

        cmd="./waf build --run 'platooning -packetSize={} -numPackets=10000 \
        -numVehicles={} \
        -traceFile=/home/kai/repos/sumo/projects/ns2mobility_{}_vehicles.tcl \
        -maxTransmissionDelay={} -interval={} \
        -maxPacketLoss={} -fwdLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -recvLogFile=/media/kai/4814A63F14A6303C/measurements/{} -sendLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -phyMode={} -networkCoding={} -forwardAll={} -noForward={}'".format(args.l, i, \
        vehicles_number_string, args.d, args.i, args.x, fwdFile, rcvFile, sendFile, \
        rateString, networkCoding, fwdAll, noForward)
        os.system(cmd)

elif (args.p=='max_loss'):
    meas_count = 11
    i = 0
    for i in range(0,meas_count):
        max_loss = 10 * i
        logFile="vehicles_{}_rate_{}_length{}_interval{}_loss_{}_delay_{}_nc_{}_fwdAll_{}_noForward_{}".format(args.n, rateString, args.l, args.i, max_loss, args.d, networkCoding, fwdAll, noForward)

        fwdFile="fwd_{}".format(logFile)
        rcvFile="rcv_{}".format(logFile)
        sendFile="send_{}".format(logFile)

        cmd="./waf build --run 'platooning -packetSize={} -numPackets=10000 \
        -numVehicles={} \
        -traceFile=/home/kai/repos/sumo/projects/ns2mobility_{}_vehicles.tcl \
        -maxTransmissionDelay={} -interval={} \
        -maxPacketLoss={} -fwdLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -recvLogFile=/media/kai/4814A63F14A6303C/measurements/{} -sendLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -phyMode={} -networkCoding={} -forwardAll={} -noForward={}'".format(args.l, args.n, \
        vehicles_number_string, args.d, args.i, max_loss, fwdFile, rcvFile, sendFile, \
        rateString, networkCoding, fwdAll, noForward)
        os.system(cmd)

elif (args.p=='max_delay'):
    meas_count = 10
    i = 0
    for i in range(0,meas_count):
        max_delay = 5000 * (i+1)
        logFile="vehicles_{}_rate_{}_length{}_interval{}_loss_{}_delay_{}_nc_{}_fwdAll_{}_noForward_{}".format(args.n, rateString, args.l, args.i, args.x, max_delay, networkCoding, fwdAll, noForward)

        fwdFile="fwd_{}".format(logFile)
        rcvFile="rcv_{}".format(logFile)
        sendFile="send_{}".format(logFile)

        cmd="./waf build --run 'platooning -packetSize={} -numPackets=10000 \
        -numVehicles={} \
        -traceFile=/home/kai/repos/sumo/projects/ns2mobility_{}_vehicles.tcl \
        -maxTransmissionDelay={} -interval={} \
        -maxPacketLoss={} -fwdLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -recvLogFile=/media/kai/4814A63F14A6303C/measurements/{} -sendLogFile=/media/kai/4814A63F14A6303C/measurements/{} \
        -phyMode={} -networkCoding={} -forwardAll={} -noForward={}'".format(args.l, args.n, \
        vehicles_number_string, max_delay, args.i, args.x, fwdFile, rcvFile, sendFile, \
        rateString, networkCoding, fwdAll, noForward)
        os.system(cmd)
