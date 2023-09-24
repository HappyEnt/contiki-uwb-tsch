#!/usr/bin/env python

from __future__ import print_function
import argparse
import time
import iotlabaggregator.common
from iotlabaggregator.serial import SerialAggregator
import pickle


identifier_mac_addr_mapping = {}
ranging_timeslot_mapping = {}
devaddr1_mapping = {}
associated = {}

tdoa_measurements = {}

finished = False 

def read_line(identifier, line):
    global finished
    # print("{}: {}".format(identifier, line))

    # check if line starts with dpnl, and if it does read all ids that come after that i.e. line looks like dpnl 205 10 83 for example
    # if line.startswith("dpnl,"):
    #     if identifier in identifier_mac_addr_mapping:
    #         neighbor_map[identifier_mac_addr_mapping[identifier]] = [int(i) for i in line.split()[1:]]
    #         # print(neighbor_map)

    # # if line starts with TA, write out linkaddr, line looks like TA, %u, %u  whereby %u, %u are the high and low byte of the address respectively
    if line.startswith("TA,"):
        if identifier not in identifier_mac_addr_mapping:
            identifier_mac_addr_mapping[identifier] = int(line.split(',')[2])

    # if line.startswith("devaddr1"):
    #     devaddr1_mapping[identifier] = int(line.split()[-1])

    # time slot mapping
    if line.startswith("ts,"):
        ranging_timeslot_mapping[identifier] = int(line.split()[-1])

    if line.startswith("tschass,"):
        associated[identifier] = int(line.split()[-1])

    if line.startswith("received line"):
        print(line)

    # TW, 218,8.75
    if line.startswith("TD,"):
        if identifier in identifier_mac_addr_mapping:
            our_mac = int(identifier_mac_addr_mapping[identifier])
            anchor_1_mac = int(line.split(',')[1])
            anchor_2_mac = int(line.split(',')[2])
            measurement = float(line.split(',')[3])
            if not our_mac in tdoa_measurements:
                tdoa_measurements[our_mac] = {}

            if not (anchor_1_mac, anchor_2_mac) in tdoa_measurements[our_mac]:
                tdoa_measurements[our_mac][(anchor_1_mac, anchor_2_mac)] = []

            tdoa_measurements[our_mac][(anchor_1_mac, anchor_2_mac)].append(measurement)

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
    with SerialAggregator(nodes, line_handler=read_line) as aggregator:
        time.sleep(10)
        while not finished:
            try:
                time.sleep(5)
                
                print(associated)
                print(identifier_mac_addr_mapping)
                           
            except KeyboardInterrupt:
                print("Interrupted by user ...")
                break

        with open("tdoa_measurements.pkl", "wb") as f:
            # save as pickle
            pickle.dump(tdoa_measurements, f)

        with open("identifier_mac_addr_mapping.txt") as f:
            f.write(str(identifier_mac_addr_mapping))


if __name__ == "__main__":
    main()
