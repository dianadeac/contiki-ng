#!/usr/bin/env python3

import os
import sys
import time
import matplotlib.pyplot as pl
import pandas as pd

###########################################

# If set to true, all nodes are plotted, even those with no valid data
PLOT_ALL_NODES = True

###########################################

LOG_FILE = "COOJA.testlog"

COORDINATOR_ID = 1

# for charge calculations
CC2650_MHZ = 48
CC2650_RADIO_TX_CURRENT_MA = 9.100  # at 5 dBm, from CC2650 datasheet
CC2650_RADIO_RX_CURRENT_MA = 5.900  # from CC2650 datasheet
CC2650_RADIO_CPU_ON_CURRENT = 0.061 * CC2650_MHZ  # from CC2650 datasheet
CC2650_RADIO_CPU_SLEEP_CURRENT = 1.335  # empirical
CC2650_RADIO_CPU_DEEP_SLEEP_CURRENT = 0.010  # empirical

###########################################

# for testbed: mapping between the node ID (Contiki_NG) and device ID (testbed)
node_id_to_device_id = {}

###########################################


class NodeStats:
    def __init__(self, id):
        self.id = id

        # intermediate metrics
        self.dis_no = 0
        self.dao_no = 0
        self.dio_no = 0
        self.dao_ack_no = 0
        self.npdao_no = 0
        self.npdao_ack_no = 0
        self.dco_no = 0
        self.dco_ack_no = 0

        self.rpl_join_time_sec = None
        self.rpl_parent_changes = 0
        self.rpl_parent = None

        self.is_valid = False

        # final metrics (uninitialized)
        self.pdr = 0.0
        self.rpl_parent_changes = 0

        # self.root_packets_received_id7 = {}
        # self.root_packets_received_id8 = {}
        # self.root_packets_received_id9 = {}

        self.root_packets_sent_id7 = {}
        self.root_packets_sent_id8 = {}
        self.root_packets_sent_id9 = {}

        self.node_packets_sent_id7 = {}
        self.node_packets_sent_id8 = {}
        self.node_packets_sent_id9 = {}

        self.node_packets_received_root = {}
        self.node_packets_received_id3 = {}

        # self.root_delays_7 = {}
        # self.root_delays_8 = {}
        # self.root_delays_9 = {}

        self.pdr_root = 0.0
        self.pdr_id3 = 0.0

        self.avg_delay_root = 0.0
        self.avg_delay_id3 = 0.0

    # calculate the final metrics
    def calc(self):

        if self.rpl_join_time_sec is None:
            print("node {} never joined RPL DAG".format(self.id))
            return False
        self.is_valid = True


###########################################


def extract_macaddr(s):
    if "NULL" in s:
        return None
    return s


def extract_ipaddr(s):
    if "NULL" in s:
        return None
    return s


# (NULL IP addr) -> fe80::244:44:44:44
def extract_ipaddr_pair(fields):
    s = " ".join(fields)
    fields = s.split(" -> ")
    return extract_ipaddr(fields[0]), extract_ipaddr(fields[1])


def addr_to_id(addr):
    return int(addr.split(":")[-1], 16)


###########################################
# Parse a log file


def analyze_results(filename, is_testbed):
    nodes = {}

    in_initialization = True

    start_ts_unix = None

    with open(filename, "r") as f:
        for line in f:
            line = line.strip()
            fields = line.split()
            if len(fields) < 3:
                continue
            try:
                # in milliseconds
                ts = int(fields[0]) // 1000  # convert to ms
                node = int(fields[1])
                message = fields[2]
            except:
                # failed to extract timestamp
                continue

            if node not in nodes:
                nodes[node] = NodeStats(node)

            # 1348000 4 [INFO: RPL       ] Sending a DIS to ff02::1a
            if "Sending a DIS" in line:
                nodes[node].dis_no += 1
                continue

            # 3656000 1 [INFO: RPL       ] Sending a multicast-DIO with rank 128
            if "Sending a multicast-DIO" in line:
                nodes[node].dio_no += 1
                continue

            # 3656000 1 [INFO: RPL       ] Sending a multicast-DIO with rank 128
            if "Sending unicast-DIO" in line:
                nodes[node].dio_no += 1
                continue

            # 8733000 2 [INFO: RPL       ] Sending a DAO with sequence number 241, lifetime 30, prefix fd00::202:2:2:2 to fe80::201:1:1:1 , parent fe80::201:1:1:1
            if "Sending a DAO with" in line:
                nodes[node].dao_no += 1
                continue

            # 8764464 1 [INFO: RPL       ] Sending a DAO ACK with sequence number 241 to fe80::202:2:2:2 with status 0
            if "Sending a DAO ACK" in line:
                nodes[node].dao_ack_no += 1
                continue

            # 2497128 2 [INFO: RPL       ] rpl_set_preferred_parent fe80::201:1:1:1 used to be NULL
            if "rpl_set_preferred_parent" in line:
                nodes[node].rpl_parent_changes += 1
                nodes[node].rpl_parent = extract_ipaddr(fields[6])
                if nodes[node].rpl_join_time_sec is None:
                    nodes[node].rpl_join_time_sec = ts / 1000
                continue

            # 377018480 76 [INFO: RPL       ] parent switch: (NULL IP addr) -> fe80::244:44:44:44
            if " parent switch: " in line:
                nodes[node].rpl_parent_changes += 1
                nodes[node].rpl_parent = extract_ipaddr_pair(fields[7:])[1]
                if nodes[node].rpl_join_time_sec is None:
                    nodes[node].rpl_join_time_sec = ts / 1000
                continue

            # 308394464 2 [INFO: RPL       ] Sending a DCO with sequence number 241, lifetime 30, prefix fd00::207:7:7:7 to fe80::203:3:3:3
            if "Sending a DCO with" in line:
                nodes[node].dco_no += 1
                continue

            #308413464 3 [INFO: RPL       ] Sending a DCO ACK with sequence number 241 to fe80::202:2:2:2
            if "Sending a DCO ACK" in line:
                nodes[node].dco_ack_no += 1
                continue

            #361450000 7 [INFO: RPL       ] Sending a No-Path DAO with sequence number 244, lifetime 0, prefix fd00::207:7:7:7 to fe80::205:5:5:5 , parent fe80::205:5:5:5
            if "Sending a No-Path DAO with" in line:
                nodes[node].npdao_no += 1
                continue

            if "Root;Sending;" in message:
                addr_id = addr_to_id(message.split(";")[-2].strip())
                msg_id = message.split(";")[-1].strip()
                if addr_id == 7:
                    nodes[node].root_packets_sent_id7[msg_id] = ts
                elif addr_id == 8:
                    nodes[node].root_packets_sent_id8[msg_id] = ts
                elif addr_id == 9:
                    nodes[node].root_packets_sent_id9[msg_id] = ts
                continue
            if "Node;Sending;" in message:
                msg_id = message.split(";")[-1].strip()
                addr_id = addr_to_id(message.split(";")[-2].strip())
                if addr_id == 7:
                    nodes[node].node_packets_sent_id7[msg_id] = ts
                elif addr_id == 8:
                    nodes[node].node_packets_sent_id8[msg_id] = ts
                elif addr_id == 9:
                    nodes[node].node_packets_sent_id9[msg_id] = ts
                continue
            if "Node;Receiving;" in message:
                msg_id = message.split(";")[-1].strip()
                addr_id = addr_to_id(message.split(";")[-2].strip())
                if addr_id == 1:
                    nodes[node].node_packets_received_root[msg_id] = ts
                elif addr_id == 3:
                    nodes[node].node_packets_received_id3[msg_id] = ts
                continue

            # if "Root;Receiving;" in message:
            #     addr_id = addr_to_id(message.split(";")[-2].strip())
            #     msg_id = message.split(";")[-1].strip()
            #     if addr_id == 7:
            #         nodes[node].root_packets_received_id7[msg_id] = ts
            #     elif addr_id == 8:
            #         nodes[node].root_packets_received_id8[msg_id] = ts
            #     elif addr_id == 9:
            #         nodes[node].root_packets_received_id9[msg_id] = ts
            #     continue

    r = []
    sorted(nodes.keys())

    # compute pdr for root, 7,8, and 9
    # total_sent_by9 = len(nodes[9].node_packets_sent)
    # total_sent_by8 = len(nodes[8].node_packets_sent)
    # total_sent_by7 = len(nodes[7].node_packets_sent)
    # root_total_received_from7 = len(nodes[1].root_packets_received_id7)
    # root_total_received_from8 = len(nodes[1].root_packets_received_id8)
    # root_total_received_from9 = len(nodes[1].root_packets_received_id9)

    total_received_by9_from_root = len(nodes[9].node_packets_received_root)
    total_received_by8_from_root = len(nodes[8].node_packets_received_root)
    total_received_by7_from_root = len(nodes[7].node_packets_received_root)

    root_total_sent_to7 = len(nodes[1].root_packets_sent_id7)
    root_total_sent_to8 = len(nodes[1].root_packets_sent_id8)
    root_total_sent_to9 = len(nodes[1].root_packets_sent_id9)

    nodes[9].pdr_root = total_received_by9_from_root / root_total_sent_to9 * 100
    nodes[8].pdr_root = total_received_by8_from_root / root_total_sent_to8 * 100
    nodes[7].pdr_root = total_received_by7_from_root / root_total_sent_to7 * 100

    total_received_by9_from_id3 = len(nodes[9].node_packets_received_id3)
    total_received_by8_from_id3 = len(nodes[8].node_packets_received_id3)
    total_received_by7_from_id3 = len(nodes[7].node_packets_received_id3)

    node_total_sent_to7 = len(nodes[3].node_packets_sent_id7)
    node_total_sent_to8 = len(nodes[3].node_packets_sent_id8)
    node_total_sent_to9 = len(nodes[3].node_packets_sent_id9)

    nodes[9].pdr_id3 = total_received_by9_from_id3 / node_total_sent_to9 * 100
    nodes[8].pdr_id3 = total_received_by8_from_id3 / node_total_sent_to8 * 100
    nodes[7].pdr_id3 = total_received_by7_from_id3 / node_total_sent_to7 * 100
    # nodes[1].pdr = (
    #     (
    #         root_total_received_from7
    #         + root_total_received_from8
    #         + root_total_received_from9
    #     )
    #     / (total_sent_by9 + total_sent_by8 + total_sent_by7)
    #     * 100
    # )

    # compute average delay for root
    # root_avg_delay_from7 = 0
    # root_avg_delay_from8 = 0
    # root_avg_delay_from9 = 0

    # for key in nodes[1].root_packets_received_id7:
    #     if key in nodes[7].node_packets_sent and key in nodes[1].root_packets_received_id7:
    #         root_avg_delay_from7 += (
    #             nodes[1].root_packets_received_id7[key] - nodes[7].node_packets_sent[key]
    #         )
    # root_avg_delay_from7 = root_avg_delay_from7 / len(
    #     nodes[1].root_packets_received_id7
    # )

    # for key in nodes[1].root_packets_received_id8:
    #     if key in nodes[8].node_packets_sent and key in nodes[1].root_packets_received_id8:
    #         root_avg_delay_from8 += (
    #             nodes[1].root_packets_received_id8[key] - nodes[8].node_packets_sent[key]
    #         )
    # root_avg_delay_from8 = root_avg_delay_from8 / len(
    #     nodes[1].root_packets_received_id8
    # )

    # for key in nodes[1].root_packets_received_id9:
    #     if key in nodes[9].node_packets_sent and key in nodes[1].root_packets_received_id9:
    #         root_avg_delay_from9 += (
    #             nodes[1].root_packets_received_id9[key] - nodes[9].node_packets_sent[key]
    #         )
    # root_avg_delay_from9 = root_avg_delay_from9 / len(
    #     nodes[1].root_packets_received_id9
    # )
    # nodes[1].avg_delay = (
    #     root_avg_delay_from7 + root_avg_delay_from8 + root_avg_delay_from9
    # ) / 3

    #compute average delay for 7,8,9  packets received from root
    for key in nodes[7].node_packets_received_root:
        nodes[7].avg_delay_root += (
            nodes[7].node_packets_received_root[key] - nodes[1].root_packets_sent_id7[key]
        )
    nodes[7].avg_delay_root /= len(nodes[7].node_packets_received_root)

    for key in nodes[8].node_packets_received_root:
        nodes[8].avg_delay_root += (
            nodes[8].node_packets_received_root[key] - nodes[1].root_packets_sent_id8[key]
        )
    nodes[8].avg_delay_root /= len(nodes[8].node_packets_received_root)

    for key in nodes[9].node_packets_received_root:
        nodes[9].avg_delay_root += (
            nodes[9].node_packets_received_root[key] - nodes[1].root_packets_sent_id9[key]
        )
    nodes[9].avg_delay_root /= len(nodes[9].node_packets_received_root)

    #compute average delay for 7,8,9  packets received from ID 3
    for key in nodes[7].node_packets_received_id3:
        nodes[7].avg_delay_id3 += (
            nodes[7].node_packets_received_id3[key] - nodes[3].node_packets_sent_id7[key]
        )
    nodes[7].avg_delay_id3 /= len(nodes[7].node_packets_received_id3)

    for key in nodes[8].node_packets_received_id3:
        nodes[8].avg_delay_id3 += (
            nodes[8].node_packets_received_id3[key] - nodes[3].node_packets_sent_id8[key]
        )
    nodes[8].avg_delay_id3 /= len(nodes[8].node_packets_received_id3)

    for key in nodes[9].node_packets_received_id3:
        nodes[9].avg_delay_id3 += (
            nodes[9].node_packets_received_id3[key] - nodes[3].node_packets_sent_id9[key]
        )
    nodes[9].avg_delay_id3 /= len(nodes[9].node_packets_received_id3)

    for k in sorted(nodes.keys()):
        n = nodes[k]
        if n.id != COORDINATOR_ID:
            n.calc()
        else:
            n.is_valid = True
            n.rpl_join_time_sec = 0
            n.parent_changes = 0
            n.parent = None
        if n.is_valid or PLOT_ALL_NODES:
            d = {
                "id": n.id,
                "dis_no": n.dis_no,
                "dio_no": n.dio_no,
                "dao_no": n.dao_no,
                "dao_ack_no": n.dao_ack_no,
                "npdao_no": n.npdao_no,
                "npdao_ack_no": n.npdao_ack_no,
                "dco_no": n.dco_no,
                "dco_ack_no": n.dco_ack_no,
                "rpl_parent": n.rpl_parent,
                "rpl_switches": n.rpl_parent_changes,
                "rpl_join_time_sec": n.rpl_join_time_sec,
                "pdr_root": n.pdr_root,
                "pdr_id3": n.pdr_id3,
                "avg_delay_root": n.avg_delay_root,
                "avg_delay_id3": n.avg_delay_id3,
            }
            r.append(d)
        print
    return r


#######################################################
# Plot the results of a given metric as a bar chart


def plot(results, metric, ylabel):
    pl.figure(figsize=(5, 4))

    data = [r[metric] for r in results]
    x = range(len(data))
    barlist = pl.bar(x, data, width=0.4)

    for b in barlist:
        b.set_color("orange")
        b.set_edgecolor("black")
        b.set_linewidth(1)

    ids = [r["id"] for r in results]
    pl.xticks(x, [str(u) for u in ids], rotation=90)
    pl.xlabel("Node ID")
    pl.ylabel(ylabel)

    if metric == "pdr":
        miny = min(80, min(data))
        pl.ylim([miny, 100])
    else:
        pl.ylim(ymin=0)

    pl.savefig("plot_{}.pdf".format(metric), format="pdf", bbox_inches="tight")
    pl.close()


#######################################################
# Run the application


def main():
    input_file = LOG_FILE
    if len(sys.argv) > 1:
        # change from the default
        input_file = sys.argv[1]

    if not os.access(input_file, os.R_OK):
        print('The input file "{}" does not exist'.format(input_file))
        exit(-1)

    with open(input_file, "r") as f:
        is_testbed = "Starting COOJA logger" not in f.read()

    results = analyze_results(input_file, is_testbed)

    results_df = pd.DataFrame(results)
    results_df.to_csv("results.csv", index=False)

    # plot(results, "rpl_switches", "RPL parent switches")

    # plot(results, "dis_no", "Number of DIS packets, #")
    # plot(results, "dio_no", "Number of DIO packets, #")
    # plot(results, "dao_no", "Number of DAO packets, #")
    # plot(results, "dao_ack_no", "Number of DAO ACK packets, #")
    # plot(results, "npdao_no", "Number of NPDAO packets, #")
    # plot(results, "npdao_ack_no", "Number of NPDAO ACK packets, #")
    # plot(results, "dco_no", "Number of DCO packets, #")
    # plot(results, "dco_ack_no", "Number of DCO ACK packets, #")
    # plot(results, "pdr", "Packet delivery ratio, %")
    # plot(results, "rpl_join_time_sec", "RPL join time, s")


#######################################################

if __name__ == "__main__":
    main()
