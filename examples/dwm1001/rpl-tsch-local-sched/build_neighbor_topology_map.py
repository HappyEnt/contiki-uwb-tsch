#!/usr/bin/env python

from __future__ import print_function
import argparse
import time
import iotlabaggregator.common
from iotlabaggregator.serial import SerialAggregator


neighbor_map = {}
identifier_mac_addr_mapping = {}
ranging_timeslot_mapping = {}

def read_line(identifier, line):
    # print("{}: {}".format(identifier, line))

    # check if line starts with dpnl, and if it does read all ids that come after that i.e. line looks like dpnl 205 10 83 for example
    if line.startswith("dpnl"):
        if identifier in identifier_mac_addr_mapping:
            neighbor_map[identifier_mac_addr_mapping[identifier]] = [int(i) for i in line.split()[1:]]
            print(neighbor_map)

    # if line starts with TA, write out linkaddr, line looks like TA, %u, %u  whereby %u, %u are the high and low byte of the address respectively
    if line.startswith("TA"):
        identifier_mac_addr_mapping[identifier] = int(line.split()[-1])
        print(identifier_mac_addr_mapping)
        
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
                aggregator.broadcast("t")
                time.sleep(2)
            except KeyboardInterrupt:
                print("Interrupted by user ...")

                # write neighbor map and identifier mac addr mapping to file
                with open("neighbor_map.txt", "w") as f:
                    f.write(str(neighbor_map))
                    
                with open("identifier_mac_addr_mapping.txt", "w") as f:
                    f.write(str(identifier_mac_addr_mapping))
                    
                break


if __name__ == "__main__":
    main()
