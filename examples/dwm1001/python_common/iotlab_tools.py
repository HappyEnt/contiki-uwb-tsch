import requests
import json
import numpy as np
import matplotlib.pyplot as plt
import math
import time, os, sys

# networkx
import networkx as nx
import matplotlib as mpl


iotlab_endpoint = "https://www.iot-lab.info/api"
node_list_endpoint = "nodes"


# For example: get_node_list("toulouse")
def get_node_list(testbed, device):
    node_list = None
    
    if os.path.exists('node_list.txt'):
        with open('node_list.txt') as json_file:
            node_list = json.load(json_file)['items']

    if node_list == None:
        url = iotlab_endpoint + "/" + node_list_endpoint
        response = requests.get(url)
        node_list = json.loads(response.text)

        # cache node_list in txt
        with open('node_list.txt', 'w') as outfile:
            json.dump(node_list, outfile)


        # filter for entries
        node_list = node_list['items']
        
    testbed_node_list = []

    for node in node_list:
        if node['site'] == testbed and node['archi'] == device:
            testbed_node_list.append(node)
            
    return testbed_node_list

# "dwm1001-8.lille.iot-lab.info -> dwm1001-8"
# "dwm1001-8.toulouse.iot-lab.info -> dwm1001-8"
def get_short_node_id(long_id):
    return long_id.split(".")[0]

# returns mapping from long_ids to 3D coordinates
def get_position_mapping(node_list):
    node_positions = {}
    for node in node_list:
        node_positions[get_short_node_id(node['network_address'])] = [ float(node['x']) , float(node['y']) , float(node['z']) ]

    return node_positions
    
