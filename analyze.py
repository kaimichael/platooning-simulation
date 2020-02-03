#!/usr/bin/env python3
import os, sys, argparse, pandas, matplotlib.pyplot as plt, math, re
parser = argparse.ArgumentParser(description='Analyze data from measurements')
parser.add_argument('-n', default=1, type=int, help='Number of files to analyze')
parser.add_argument('-t', default='Receive', choices=['Receive', 'Forward'], help='Select type of measurement data (Receive, Forward)')
parser.add_argument('-d', default=False, help='Analyze and visualize delay distribution')
parser.add_argument('-z', default=False, help='Analyze average delivery and decoding delays')
parser.add_argument('-x', default=False, help='Analyze and visualize the packet losses')
parser.add_argument('-f', default=False,  help='Analyze and visualize the number of forwarded packets')
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
    print("Enter filename {}:\n".format(i+1))
    filename=input()
    filelist.append(filename)
    dataframe=pandas.read_csv(filename,sep=', ', header=1, engine='python')
    dflist.append(dataframe)
print("Enter filename of the output\n")
output_name=input()
forward_parameters=True
network_coding=True
if filelist[0].find("fwdAll_true")!=-1 or filelist[0].find("noForward_true")!=-1:
    forward_parameters=False
if filelist[0].find("nc_true")==-1:
    network_coding=False
    print("No network coding\n")
if args.t =='Receive':
    if args.b=='Node':
        if args.z:
            if args.n > 1:
                print("Average delays can only be analyzed for one node\n")
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
            dflist[0]['Packet Delivery Delay']=dflist[0]['Packet Delivery Delay'].astype(float)
            dflist[0]['Packet Delivery Delay']=dflist[0]['Packet Delivery Delay'].multiply(0.001)
            dflist[0]['Decoding Delay']=dflist[0]['Decoding Delay'].astype(float)
            dflist[0]['Decoding Delay']=dflist[0]['Decoding Delay'].multiply(0.001)
            if not network_coding:
                dflist[0].drop_duplicates(subset=['Sequence Number', 'Source ID'], keep='first', inplace=True)
            df2 = dflist[0].groupby('Source ID').mean()
            ax= df2[['Packet Delivery Delay','Decoding Delay']].plot(kind='bar', stacked=True)
            ax.set_ylabel("Average delay in ms")
            ax.autoscale(enable=True)

            fig = ax.get_figure()
            mid = (fig.subplotpars.right + fig.subplotpars.left)/2
            fig.suptitle('Average delay at node {} by source node \n'.format(node_id), fontsize=12, x= mid)
            if forward_parameters:
                ax.set_title('data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms, maximum delay: {} ms, maximum loss: {} %'.format(rate, length, n_vehicles, interval, d_max, loss_max), fontsize=10, y=1.2 )
            else:
                ax.set_title('data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms'.format(rate, length, n_vehicles, interval), fontsize=10, y=1.2 )

            fig.tight_layout()
            fig.savefig(output_name)
        if args.d:
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
            if not network_coding:
                dflist[0].drop_duplicates(subset=['Sequence Number', 'Source ID'], keep='first', inplace=True)
            flierprops = dict(marker='x', markerfacecolor='black', markersize=2.5,
                      linestyle='none')
            ax=dflist[0].boxplot(column='Total Delay', by='Source ID', flierprops=flierprops, whis=[5, 95])
            ax.set_ylabel("Total delay in ms")
            ax.set_yscale('log')
            ax.autoscale(enable=True)

            fig = ax.get_figure()
            mid = (fig.subplotpars.right + fig.subplotpars.left)/2
            fig.suptitle('Total delay at node {} by source node \n'.format(node_id), fontsize=12, x= mid)
            if forward_parameters:
                ax.set_title('Boxplot whiskers at 5% and 95% \n data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms, maximum delay: {} ms, maximum loss: {} %'.format(rate, length, n_vehicles, interval, d_max, loss_max), fontsize=10, y=1.2 )
            else:
                ax.set_title('Boxplot whiskers at 5% and 95% \n data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms'.format(rate, length, n_vehicles, interval), fontsize=10, y=1.2 )

            fig.tight_layout()
            fig.savefig(output_name)
        if args.x:
            if args.n > 1:
                print("Packet loss per source node can only be analyzed for one node\n")
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
            losslist=[]
            if not network_coding:
                dflist[0].drop_duplicates(subset=['Sequence Number', 'Source ID'], keep='first', inplace=True)
            for i in (list(range(0,node_id))+list(range(node_id+1,n_vehicles))):
                losslist.append((10000-len(dflist[0][dflist[0]['Source ID'] == i]))/10000*100)
            plt.bar(list(range(0,node_id))+list(range(node_id+1,n_vehicles)),losslist)
            plt.xlabel('Node ID')
            plt.ylabel('Loss ratio in %')
            plt.autoscale(enable=True)
            plt.suptitle('Loss ratio per source node at node {} \n'.format(node_id), fontsize=12)
            if forward_parameters:
                plt.title('data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms, maximum delay: {} ms, maximum loss: {} %'.format(rate, length, n_vehicles, interval, d_max, loss_max), fontsize=10, y=1.2)
            else:
                plt.title('data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms'.format(rate, length, n_vehicles, interval), fontsize=10, y=1.2)
            plt.xticks(list(range(0,node_id))+list(range(node_id+1,n_vehicles)), list(range(0,node_id))+list(range(node_id+1,n_vehicles)))
            plt.tight_layout()
            plt.savefig(output_name)
    if args.b=='MCS':
        with open(filelist[0]) as f:
            firstline=f.readline().strip()
            res = rx.findall(firstline)
            node_id =int(float( res[0]))
            ratelist=[]
            if len(res) == 8:
                n_vehicles = int(float(res[3]))
                length = int(float(res[4]))
                d_max = float(res[5])/1000
                loss_max = int(float(res[6]))
                interval = float(res[7])*1000
            if len(res) == 9:
                n_vehicles = float(res[4])
                length = int(float(res[5]))
                d_max = float(res[6])/1000
                loss_max = int(float(res[7]))
                interval = float(res[8])*1000
            for i in range(0,args.n):
                with open(filelist[i]) as g:
                    firstline=g.readline().strip()
                    res2 = rx.findall(firstline)
                    if len(res2) == 8:
                        ratelist.append(float(res2[1]))
                    if len(res2) == 9:
                        ratelist.append(float(res2[1]) + float(res2[2]) / 10.0)
        if args.x:
            losslist=[]
            if not network_coding:
                for dataframe in dflist:
                    dataframe.drop_duplicates(subset=['Sequence Number', 'Source ID'], keep='first', inplace=True)
            for i in range(0,args.n):
                losslist.append((10000-len(dflist[i][dflist[i]['Source ID'] == n_vehicles-1]))/10000*100)
            plt.plot(ratelist,losslist,'ro')
            plt.xlabel('Data rate in Mbps')
            plt.ylabel('Loss ratio in %')
            plt.autoscale(enable=True)
            plt.suptitle('Loss ratio for packets from last vehicles at node {} \n'.format(node_id), fontsize=12)
            if forward_parameters:
                plt.title('packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms, maximum delay: {} ms, maximum loss: {} %'.format(length, n_vehicles, interval, d_max, loss_max), fontsize=10, y=1.2)
            else:
                plt.title('packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms'.format(length, n_vehicles, interval), fontsize=10, y=1.2)
            plt.tight_layout()
            plt.savefig(output_name)
        if args.z:
            delaylist=[]
            if not network_coding:
                for dataframe in dflist:
                    dataframe.drop_duplicates(subset=['Sequence Number', 'Source ID'], keep='first', inplace=True)
            for i in range(0,args.n):
                delaylist.append((float(dflist[i][dflist[i]['Source ID'] == n_vehicles-1]['Total Delay'].mean())/1000))
            plt.plot(ratelist,delaylist,'ro')
            plt.xlabel('Data rate in Mbps')
            plt.ylabel('Average delay in ms')
            plt.autoscale(enable=True)
            plt.suptitle('Average delay for packets from last vehicles at node {} \n'.format(node_id), fontsize=12)
            if forward_parameters:
                plt.title('packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms, maximum delay: {} ms, maximum loss: {} %'.format(length, n_vehicles, interval, d_max, loss_max), fontsize=10, y=1.2)
            else:
                plt.title('packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms'.format(length, n_vehicles, interval), fontsize=10, y=1.2)
            plt.tight_layout()
            plt.savefig(output_name)
    if args.b=='Interval':
        with open(filelist[0]) as f:
            firstline=f.readline().strip()
            res = rx.findall(firstline)
            node_id =int(float( res[0]))
            intervallist=[]
            if len(res) == 8:
                rate = float(res[1])
                n_vehicles = int(float(res[3]))
                length = int(float(res[4]))
                d_max = float(res[5])/1000
                loss_max = int(float(res[6]))
            if len(res) == 9:
                rate = float(res[1]) + float(res[2]) / 10.0
                n_vehicles = float(res[4])
                length = int(float(res[5]))
                d_max = float(res[6])/1000
                loss_max = int(float(res[7]))
            for i in range(0,args.n):
                with open(filelist[i]) as g:
                    firstline=g.readline().strip()
                    res2 = rx.findall(firstline)
                    if len(res2) == 8:
                        intervallist.append(float(res2[7])*1000)
                    if len(res2) == 9:
                        intervallist.append(float(res2[8])*1000)
        if args.x:
            losslist=[]
            if not network_coding:
                for dataframe in dflist:
                    dataframe.drop_duplicates(subset=['Sequence Number', 'Source ID'], keep='first', inplace=True)
            for i in range(0,args.n):
                losslist.append((10000-len(dflist[i][dflist[i]['Source ID'] == n_vehicles-1]))/10000*100)
            plt.plot(intervallist,losslist,'ro')
            plt.xlabel('Packet interval in ms')
            plt.ylabel('Loss ratio in %')
            plt.autoscale(enable=True)
            plt.xscale('log')
            plt.suptitle('Loss ratio for packets from last vehicles at node {} \n'.format(node_id), fontsize=12)
            if forward_parameters:
                plt.title('data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n maximum delay: {} ms, maximum loss: {} %'.format(rate, length, n_vehicles, d_max, loss_max), fontsize=10, y=1.2)
            else:
                plt.title('data rate: {} Mbps, packet length: {} bytes, number of vehicles: {}'.format(rate, length, n_vehicles), fontsize=10, y=1.2)
            plt.tight_layout()
            plt.savefig(output_name)
        if args.z:
            delaylist=[]
            if not network_coding:
                for dataframe in dflist:
                    dataframe.drop_duplicates(subset=['Sequence Number', 'Source ID'], keep='first', inplace=True)
            for i in range(0,args.n):
                delaylist.append((float(dflist[i][dflist[i]['Source ID'] == n_vehicles-1]['Total Delay'].mean())/1000))
            plt.plot(intervallist, delaylist,'ro')
            plt.xlabel('Packet interval in ms')
            plt.ylabel('Average delay in ms')
            plt.autoscale(enable=True)
            plt.suptitle('Average delay for packets from last vehicles at node {} \n'.format(node_id), fontsize=12)
            if forward_parameters:
                plt.title('data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n maximum delay: {} ms, maximum loss: {} %'.format(rate, length, n_vehicles, d_max, loss_max), fontsize=10, y=1.2)
            else:
                plt.title('data rate: {} Mbps, packet length: {} bytes, number of vehicles: {}'.format(rate, length, n_vehicles), fontsize=10, y=1.2)
            plt.tight_layout()
            plt.savefig(output_name)
    if args.b=='Length':
        print('Length analysis')
    if args.b=='NumVehicles':
        print('Number of vehicles analysis')
    if args.b=='MaxDelay':
        print('Maximum delay analysis')
    if args.b=='MaxSourceLoss':
        print('Maximum source loss anaylisis')

elif args.t == 'Forward':
    nodelist=[]
    for dataframe in dflist:
        nodelist.append(len(dataframe.index))
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
    plt.bar(range(0, n_vehicles),nodelist)
    plt.xlabel('Node ID')
    plt.ylabel('Number of forwards')
    plt.autoscale(enable=True)
    plt.suptitle('Number of forwards per node\n', fontsize=12)
    if forward_parameters:
        plt.title('data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms, maximum delay: {} ms, maximum loss: {} %'.format(rate, length, n_vehicles, interval, d_max, loss_max), fontsize=10, y=1.2)
    else:
        plt.title('data rate: {} Mbps, packet length: {} bytes, number of vehicles: {} \n packet interval: {} ms'.format(rate, length, n_vehicles, interval), fontsize=10, y=1.2)

    plt.xticks(range(0, n_vehicles), range(0, n_vehicles))
    plt.tight_layout()
    plt.savefig(output_name)

else:
    print("Unsupported measurement data type. Choose 'Receive' or 'Forward'\n")
