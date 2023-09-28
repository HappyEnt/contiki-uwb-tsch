#!/usr/bin/env python

from __future__ import print_function
import argparse
import time
import iotlabaggregator.common
from iotlabaggregator.serial import SerialAggregator
import pickle, random
from datetime import datetime, timedelta


identifier_mac_addr_mapping = {}
ranging_timeslot_mapping = {}
timeslot_history = []
devaddr1_mapping = {}
associated = {}

tdoa_measurements = {}
twr_measurements = {}
total_measurements = {}
initial_timestamp = {}
initial_asn = {}
max_package_lengths = {}

finished = False 

def read_line(identifier, line):
    global finished

    # # if line starts with TA, write out linkaddr, line looks like TA, %u, %u  whereby %u, %u are the high and low byte of the address respectively
    if line.startswith("TA,"):
        if identifier not in identifier_mac_addr_mapping:
            identifier_mac_addr_mapping[identifier] = int(line.split(',')[2])

    # # time slot mapping
    elif line.startswith("ts,"):
        ranging_timeslot_mapping[identifier] = int(line.split(',')[-1])

    # elif line.startswith("tschass"):
    #     associated[identifier] = int(line.split()[-1])

    elif line.startswith("ppl,"):
        if identifier not in max_package_lengths:
            max_package_lengths[identifier] = int(line.split(',')[1])
        max_package_lengths[identifier] = max(max_package_lengths[identifier], int(line.split(',')[1]))

    elif line.startswith("TW,"):
        timeslot_asn = int(line.split(',')[2])

        if identifier not in initial_timestamp:
            initial_asn[identifier] = timeslot_asn
            initial_timestamp[identifier] = time.time()
        
        timeslot_time = initial_timestamp[identifier] + (timeslot_asn - initial_asn[identifier]) * 0.005
        other_mac = int(line.split(',')[1])
        dist = float(line.split(',')[3])

        if identifier not in total_measurements:
            total_measurements[identifier] = 0
        if identifier not in twr_measurements:
            twr_measurements[identifier] = {}

        if other_mac not in twr_measurements[identifier]:
            twr_measurements[identifier][other_mac] = []

        twr_measurements[identifier][other_mac].append((timeslot_time, dist))
        total_measurements[identifier] += 1
        

    elif line.startswith("TD,"):
        timeslot_asn = int(line.split(',')[3])

        if identifier not in initial_timestamp:
            initial_asn[identifier] = timeslot_asn
            initial_timestamp[identifier] = time.time()
        
        timeslot_time = initial_timestamp[identifier] + (timeslot_asn - initial_asn[identifier]) * 0.005
        
        other_mac_1 = int(line.split(',')[1])
        other_mac_2 = int(line.split(',')[2])
        dist = float(line.split(',')[4])
        if identifier not in total_measurements:
            total_measurements[identifier] = 0

        if identifier not in tdoa_measurements:
            tdoa_measurements[identifier] = {}

        if (other_mac_1, other_mac_2) not in tdoa_measurements[identifier]:
            tdoa_measurements[identifier][(other_mac_1, other_mac_2)] = []

        tdoa_measurements[identifier][(other_mac_1, other_mac_2)].append((timeslot_time, dist))
        total_measurements[identifier] += 1

    # elif line.startswith("received line"):
    #     print(line)

def main():
    global finished
    """ Launch serial aggregator.
    """
    parser = argparse.ArgumentParser()
    iotlabaggregator.common.add_nodes_selection_parser(parser)
    opts = parser.parse_args()
    opts.with_a8 = False # redirect a8-m3 serial port
    opts.with_dwm1001 = True # redirect a8-m3 serial port    
    nodes = SerialAggregator.select_nodes(opts)
    time_since_last = time.time()
    with SerialAggregator(nodes, line_handler=read_line) as aggregator:
        while not finished:
            try:
                time.sleep(0.5)

                # add ranging_timeslot_mapping to timeslot history
                timeslot_history.append(ranging_timeslot_mapping.copy())

                # randomly select one node and send "t 4\n" to it
                if time.time() - time_since_last > 30:
                    print("destroys the network")
                    time_since_last = time.time()
                    node_to_destroy = [random.choice(nodes)]
                    timeslot_history[-1]["destroy"] = node_to_destroy[0] # we add an destroy entry to the dictionary, so we can later add markings during evaluation
                    aggregator.send_nodes(node_to_destroy, "t 6\n")

                # print(associated)
                print(ranging_timeslot_mapping)
                print("")
                print(total_measurements)
                print("")
                print(max_package_lengths)


            except KeyboardInterrupt:
                print("Interrupted by user ...")
                break
            
        with open("timeslot_history.pkl", "wb") as f:
            # save as pickle
            pickle.dump(timeslot_history, f)

        with open("tdoa_measurements.pkl", "wb") as f:
                # save as pickle
            pickle.dump(tdoa_measurements, f)
            
        with open("twr_measurements.pkl", "wb") as f:
            # save as pickle
            pickle.dump(twr_measurements, f)
                

if __name__ == "__main__":
    main()
