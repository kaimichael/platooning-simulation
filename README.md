# platooning-simulation

## ns3 simulation

In order to start the simulation in ns3 simply copy the 'platooning' folder into the 'scratch' directory of a working ns3 installation.
If the modified nckernel version for interflow-networkcoding is already installed on your system, simply let ns3 build, then you can run 
the simulation.

Example call to start the simulator from ns-3.xx directory:

> ./waf build --run 'platooning -packetSize=100 -numPackets=100 -numVehicles=20 -traceFile=/home/kai/repos/sumo/projects/ns2mobility_20_vehicles.tcl -maxTransmissionDelay=25000 -interval=0.1 -logFile=measurements/mobility.log -maxPacketLoss=5 -fwdLogFile=measurements/fwd1 -recvLogFile=measurements/recv1 -sendLogFile=measurements/send1 -phyMode=OfdmRate27MbpsBW10MHz'

Options:
- traceFile - required - path to the movement tracefile from sumo
- phyMode - optional - the MCS-rate used in Stringform (e.g. 'OfdmRate27MbpsBW10MHz'), default value 27 Mbps
- packetSize - optional - the length of the generated packets, default value 1000 bytes
- interval - optional - the interval between the generated packets in seconds, default 1 s
- numVehicles - optional - the number of vehicles, must match the number of simulated vehicles from the movement trace file, default 5
- maxTransmissionDelay - optional - the maximum time in us for a single transmission before forwarding is used, default -1 (inactive)
- maxPacketLoss - optional - the maximum loss ratio in % from a single source before forwarding is used, default 100 % (inactive)
- logFile - required - the output file for the logging of node mobility
- recvLogFile - required - template for the logging of received / decoded packets, will automatically be addended by node number and .log
- sendLogFile - required - template for the logging of sent packets, will automatically be addended by node number and .log
- fwdLogFile - required - template for the logging of forwarded packets, will automatically be addended by node number and .log
- forwardAll - optional - enables automatic forwarding on all nodes, wihtout special rules, default false
- networkCoding - optional - enables network coding for all devies, default true

## sumo and tracefiles
If you want to repeat the vehicle simulations in sumo copy the 'projects' folder into your sumo folder. The simulation files can be found in projects/xx_vehicles.
To run a simulation execute 'platooning.py' in the respective folder. If you want to see the visual representation go into the 'platooning.py' file and replace 
> sumoBinary = "../../bin/sumo"
with 
> sumoBinary = "../../bin/sumo-gui"

There are already trace files ready for use with ns3 in the projects folder. 
>projects/ns2mobility_xx_vehicles.tcl

