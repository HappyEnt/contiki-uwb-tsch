#!/usr/bin/env python

from __future__ import print_function
import argparse
import time
import iotlabaggregator.common
from iotlabaggregator.serial import SerialAggregator


neighbor_map = {}
identifier_mac_addr_mapping = {}
ranging_timeslot_mapping = {}
short_addr_mapping = {}
devaddr1_mapping = {}

overall_measurements = {}
measurement_count = {}

associated = {}

def read_line(identifier, line):
    # print("{}: {}".format(identifier, line))

    # check if line starts with dpnl, and if it does read all ids that come after that i.e. line looks like dpnl 205 10 83 for example
    if line.startswith("dpnl"):
        if identifier in identifier_mac_addr_mapping:
            neighbor_map[identifier_mac_addr_mapping[identifier]] = [int(i) for i in line.split()[1:]]
            # print(neighbor_map)

    # if line starts with TA, write out linkaddr, line looks like TA, %u, %u  whereby %u, %u are the high and low byte of the address respectively
    if line.startswith("TA"):
        identifier_mac_addr_mapping[identifier] = int(line.split()[-1])
        short_addr_mapping[identifier] = [int(line.split()[-2].replace(',', '')), int(line.split()[-1])]
        # print(identifier_mac_addr_mapping)

    if line.startswith("devaddr1"):
        devaddr1_mapping[identifier] = int(line.split()[-1])


    # time slot mapping
    if line.startswith("ts"):
        ranging_timeslot_mapping[identifier] = int(line.split()[-1])

    if line.startswith("tschass"):
        associated[identifier] = int(line.split()[-1])
        
    

    # TW, 218,8.75
    if line.startswith("TW"):
        if identifier in identifier_mac_addr_mapping:
            our_mac = int(identifier_mac_addr_mapping[identifier])
            other_mac = int(line.split(',')[-2])
            measurement = float(line.split(',')[-1])
            if not our_mac in overall_measurements:
                overall_measurements[our_mac] = {}
                measurement_count[our_mac] = {}

            if not other_mac in overall_measurements[our_mac]:
                overall_measurements[our_mac][other_mac] = []
                measurement_count[our_mac][other_mac] = 0

            overall_measurements[our_mac][other_mac].append(measurement)
            measurement_count[our_mac][other_mac] += 1
            

def main():
    """ Launch serial aggregator.
    """
    parser = argparse.ArgumentParser()
    iotlabaggregator.common.add_nodes_selection_parser(parser)
    opts = parser.parse_args()
    opts.with_a8 = False # redirect a8-m3 serial port
    nodes = SerialAggregator.select_nodes(opts)
    with SerialAggregator(nodes, line_handler=read_line) as aggregator:
        while True:
            try:
                # send 't' on all serial links
                # if you want to specify a list of nodes use
                # aggregator.send_nodes([m3-<id1>, m3-<id2>], 't')
                # aggregator.broadcast("t")
                # time.sleep(2)
                time.sleep(10)
                print(neighbor_map)
                print(devaddr1_mapping)
                # print(identifier_mac_addr_mapping)
                # print(short_addr_mapping)
                print(measurement_count)
                print(associated)
                           
            except KeyboardInterrupt:
                print("Interrupted by user ...")

                # write neighbor map and identifier mac addr mapping to file
                with open("neighbor_map.txt", "w") as f:
                    f.write(str(neighbor_map))
                    
                with open("identifier_mac_addr_mapping.txt", "w") as f:
                    f.write(str(identifier_mac_addr_mapping))

                with open("devaddr1_mapping.txt", "w") as f:
                    f.write(str(devaddr1_mapping))

                with open("measurements.txt", "w") as f:
                    f.write(str(overall_measurements))
                    
                break


if __name__ == "__main__":
    main()
