#!/usr/bin/env python3
import os, sys, argparse, pandas, matplotlib.pyplot as plt, math, re
parser = argparse.ArgumentParser(description='Analyze data from measurements')
parser.add_argument('-n', default=1, type=int, help='Number of files to analyze')
parser.add_argument('-t', default='Receive', choices=['Receive', 'Forward'], help='Select type of measurement data (Receive, Forward)')
parser.add_argument('-d', default=False, help='Analyze and visualize average delay over all nodes')
parser.add_argument('-g', default=False, help='Analyze and visualize delay distribution over all nodes')
parser.add_argument('-w', default=False, help='Analyze and visualize average delay per source node')
parser.add_argument('-z', default=False, help='Analyze and visualize delay distribution per source node')
parser.add_argument('-l', default=False, help='Analyze and visualize the total packet losses')
parser.add_argument('-x', default=False, help='Analyze and visualize the packet loss per source node')
parser.add_argument('-f', default=False,  help='Analyze and visualize the number of forwarded packets per node')
parser.add_argument('-b', default='Node', choices=['Node', 'MCS', 'Interval', 'Length', 'NumVehicles', 'MaxDelay', 'MaxSourceLoss'], help='Set the x-Axis for Analysis')
args=parser.parse_args()


numeric_const_pattern = r"""[-+]?
(?:
(?: \d* \. \d+ ) # .1 .12 .123 etc 9.1 etc 98.1 etc
|
(?: \d+ \.? ) # 1. 12. 123. etc 1 12 123 etc
)
(?: [Ee] [+-]? \d+ ) ?
"""
rx = re.compile(numeric_const_pattern, re.VERBOSE)
filelist=[]
dflist=[]
i=0
for i in range(0,args.n):
    print("Enter filename {}:!\n".format(i+1))
    filename=input()
    filelist.append(filename)
    df=pandas.read_csv(filename,sep=', ', header=1, engine='python')
    dflist.append(df)

if args.t =='Receive':
    if args.g:
        if args.n > 1:
            print("Delay distribution per source node can only be analyzed for one node\n")
        with open(filelist[0]) as f:
            firstline=f.readline().strip()
        res = rx.findall(firstline)
        node_id =int(float( res[0]))
        if len(res) == 8:
            rate = float(res[1])
            n_vehicles = int(float(res[3]))
            length = int(float(res[4]))
            d_max = float(res[5])/1000
            loss_max = int(float(res[6]))
            interval = float(res[7])*1000
        if len(res) == 9:
            rate = float(res[1]) + float(res[2]) / 10.0
            n_vehicles = float(res[4])
            length = int(float(res[5]))
            d_max = float(res[6])/1000
            loss_max = int(float(res[7]))
            interval = float(res[8])*1000
        dflist[0]['Total Delay']=dflist[0]['Total Delay'].astype(float)
        dflist[0]['Total Delay']=dflist[0]['Total Delay'].multiply(0.001)
        ax=dflist[0].boxplot(column='Total Delay', by='Source ID', sym='+', whis=[5, 95])
        ax.set_ylabel("Total delay in ms")
        ax.set_yscale('log')
        ax.autoscale(enable=True)
        fig = ax.get_figure()
        mid = (fig.subplotpars.right + fig.subplotpars.left)/2
        fig.suptitle('Total delay at node {} by source node \n'.format(node_id), fontsize=12, x= mid)
        ax.set_title('Boxplot whiskers at 5% and 95% \n data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms, maximum delay: {} ms, maximum loss: {} %'.format(rate, length, n_vehicles, interval, d_max, loss_max), fontsize=10, y=1.2 )

        fig.tight_layout()
        fig.savefig("output2.png")
    if args.x:
        if args.n > 1:
            print("Packet loss per source node can only be analyzed for one node\n")
        node_id = 9
        rate = 6
        length = 300
        n_vehicles = 10
        interval = 50
        d_max = 10
        loss_max = 10
        losslist=[]
        for i in range(0,node_id+1):
            losslist.append((10000-len(df[dflist[0]['Source ID'] == i]))/100)
        plt.bar([0,1,2,3,4,5,6,7,8,9],losslist)
        plt.xlabel('Node ID')
        plt.ylabel('Loss ratio in %')
        plt.title('Loss ratio per source node at node 0 \n data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms, maximum delay: {} ms, maximum loss: {} %'.format(rate, length, n_vehicles, interval, d_max, loss_max))
        plt.xticks([0,1,2,3,4,5,6,7,8,9], [0,1,2,3,4,5,6,7,8,9])
        plt.savefig("output4.png")

elif args.t == 'Forward':
    nodelist=[]
    for dataframe in dflist:
        nodelist.append(len(dataframe.index))
    node_id = 9
    rate = 6
    length = 300
    n_vehicles = 10
    interval = 50
    d_max = 10
    loss_max = 10
    plt.bar([0,1,2,3,4,5,6,7,8,9],nodelist)
    plt.xlabel('Node ID')
    plt.ylabel('Number of forwards')
    plt.title('Number of forwards per node\n data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms, maximum delay: {} ms, maximum loss: {} %'.format(rate, length, n_vehicles, interval, d_max, loss_max))
    plt.xticks([0,1,2,3,4,5,6,7,8,9], [0,1,2,3,4,5,6,7,8,9])
    plt.savefig("output5.png")

else:
    print("Unsupported measurement data type. Choose 'Receive' or 'Forward'\n")
