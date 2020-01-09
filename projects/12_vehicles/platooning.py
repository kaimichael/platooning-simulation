#!/usr/bin/env python3
import os, sys
if 'SUMO_HOME' in os.environ:
    tools = os.path.join(os.environ['SUMO_HOME'], 'tools')
    sys.path.append(tools)
else:
    sys.exit("please declare environment variable 'SUMO_HOME'")

sumoBinary = "../../bin/sumo"
sumoCmd = [sumoBinary, "-c", "./circular.sumocfg", "--fcd-output", "./circularTrace.xml", "--begin", "0", "--end", "10000", "--log", "log.txt"]

import traci, simpla
traci.start(sumoCmd)
simpla.load("simpla.cfg")
step = 0
while step < 10000:

    traci.simulationStep()
    step += 1
simpla.stop()
traci.close()
