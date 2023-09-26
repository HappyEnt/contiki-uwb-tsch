import networkx as nx
import matplotlib.pyplot as plt
import numpy as np
from itertools import count
from scipy.interpolate import griddata
#json
import json
import pickle
# mmap
import mmap, ast

import sys
# add ../../python_common to python load path

sys.path.append('../../python_common')

from iotlab_tools import get_node_list, get_short_node_id, get_position_mapping
from generate_pretty_map_toulouse import generate_networkx_graph


def filter_outliers(distances):
    # throw away outliers through use of median
    filtered_distances = []
    filter_count = 0
    median = np.median(distances)
    for distance in distances:
        if abs(distance - median) < 0.5*100:
            filtered_distances.append(distance)
        else:
            filter_count += 1

    print("Filtered " + str(filter_count) + " outliers")
    return filtered_distances

def filter_outliers_in_measurements(measurements):
    filtered_measurements = {}
    for node_id, measurements_dict in measurements.items():
        if node_id not in filtered_measurements:
            filtered_measurements[node_id] = {}

        for anchor_pair, distances in measurements_dict.items():
            filtered_measurements[node_id][anchor_pair] = filter_outliers(distances)

    return filtered_measurements

# for toulouse we ad
def toulouse_mac_to_testbed_id(short_mac):
    return f"dwm1001-{short_mac}"


def filter_measurements_only_ceiling(measurements, node_positions):
    # select only measurements where the anchors are on the same height
    same_height_measurements = {}
    for node_id, measurements_dict in measurements.items():
        node_pos = node_positions[node_id]
        if node_pos[2] > 2.5:
            same_height_measurements[node_id] = measurements_dict
        
    return same_height_measurements

def create_edges_for_tdoa(graph, measurements):
    colored_anchor_pair = {}
    
    for node1, anchor_measurements_dict in measurements.items():
        print(node1)
        for anchor_pair, _ in anchor_measurements_dict.items():

            if anchor_pair not in colored_anchor_pair:
                # pick new color which is not already in colored_anchor_pair
                color = 0
                while color in colored_anchor_pair.values():
                    color += 1
                colored_anchor_pair[anchor_pair] = color
            
            id_node1 = node1
            id_anchor_1 = anchor_pair[0]
            id_anchor_2 = anchor_pair[1]
            # add edge
            print(id_node1, id_anchor_1, id_anchor_2)
            graph.add_edge(id_node1, id_anchor_1)
            graph.add_edge(id_node1, id_anchor_2)

            # color edges according to anchor pair color
            graph[id_node1][id_anchor_1]['color'] = colored_anchor_pair[anchor_pair]
            graph[id_node1][id_anchor_2]['color'] = colored_anchor_pair[anchor_pair]


# Utilities
def load_tdoa_measurements(file_name):
    # Formatted as a Python dict
    with open(file_name, 'r') as f:
        content = f.read()
        measurements = ast.literal_eval(content)
    return measurements


def load_tdoa_measurements_from_pickle(file_name):
    raw_measurements = None
    renamed_measurements = {}
    with open(file_name, 'rb') as f:
        raw_measurements = pickle.load(f)

    # convert node ids to toulouse ids
    for node1, meaurements_dict in raw_measurements.items():
        new_node_id = toulouse_mac_to_testbed_id(node1)
        if node1 not in renamed_measurements:
            renamed_measurements[new_node_id] = {}
        for (anc1, anc2), measurements in meaurements_dict.items():
            renamed_measurements[new_node_id][(toulouse_mac_to_testbed_id(anc1), toulouse_mac_to_testbed_id(anc2))] = measurements
    
    return renamed_measurements

def load_measurements_from_twr_pickle(file_name):
    twr_measurements = None
    mean_measurements = {}
    tdoa_measurements = {}
    with open(file_name, 'rb') as f:
        twr_measurements = pickle.load(f)

    for node1, measurements_dict in twr_measurements.items():
        if node1 not in mean_measurements:
            mean_measurements[node1] = {}
        for node2, measurements in measurements_dict.items():
            filtered_measurements = filter_outliers(measurements)
            mean_measurements[node1][node2] = np.mean(filtered_measurements)
            
    for node1, measurements_dict in mean_measurements.items():
        if node1 not in tdoa_measurements:
            node1_id = toulouse_mac_to_testbed_id(node1)
            tdoa_measurements[node1_id] = {}
        for node2, mean_2 in measurements_dict.items():
            for node3, mean_3 in measurements_dict.items():
                if node2 == node3:
                    continue

                node2_id = toulouse_mac_to_testbed_id(node2)
                node3_id = toulouse_mac_to_testbed_id(node3)

                tdoa_measurements[node1_id][(node2_id, node3_id)] = mean_2 - mean_3

    return tdoa_measurements

                

                

        
def plot_all_measurements(measurements, ground_truths):
    for node1, anchor_measurements_dict in measurements.items():
        for anchor_pair, distances in anchor_measurements_dict.items():

            # plot ground truth as horizontal line
            ground_truth = ground_truths[node1][anchor_pair]
            plt.axhline(y=ground_truth, color='r', linestyle='-')
            
            
            plt.scatter(range(len(distances)), distances)
            plt.title("Node: " + str(node1) + " Anchor Pair: " + str(anchor_pair))
            plt.show()

def calculate_ground_truth_distance_differences(node_positions, measurements):
    ground_truth_tdoa = {}
    for node1, anchor_measurements_dict in measurements.items():
        node1_pos = np.array(node_positions[node1])
        if node1 not in ground_truth_tdoa:
            ground_truth_tdoa[node1] = {}
        for anchor_pair, distances in anchor_measurements_dict.items():
            if anchor_pair not in ground_truth_tdoa[node1]:
                # calculate ground truth distance
                anchor1_pos = np.array(node_positions[anchor_pair[0]])
                anchor2_pos = np.array(node_positions[anchor_pair[1]])

                dist_diff = np.linalg.norm(node1_pos - anchor1_pos) - np.linalg.norm(node1_pos - anchor2_pos)

                ground_truth_tdoa[node1][anchor_pair] = dist_diff * 100

    return ground_truth_tdoa

def calculate_mean_distance(measurements):
    mean_tdoa = {}
    for node1, anchor_measurements_dict in measurements.items():
        if node1 not in mean_tdoa:
            mean_tdoa[node1] = {}
        for anchor_pair, distances in anchor_measurements_dict.items():
            if anchor_pair not in mean_tdoa[node1]:
                # calculate ground truth distance
                mean_tdoa[node1][anchor_pair] = np.mean(distances)

    return mean_tdoa

def calculate_error(mean_tdoa, ground_truth_tdoa):
    error = {}
    for node1, anchor_measurements_dict in mean_tdoa.items():
        if node1 not in error:
            error[node1] = {}
        for anchor_pair, mean_distance in anchor_measurements_dict.items():
            if anchor_pair not in error[node1]:
                error[node1][anchor_pair] = abs(mean_distance - ground_truth_tdoa[node1][anchor_pair])

    return error

def get_anchor_pairs_in_measurements(measurements):
    anchor_pairs = []
    for node1, anchor_measurements_dict in measurements.items():
        for anchor_pair, distances in anchor_measurements_dict.items():
            anc1_id = anchor_pair[0]
            anc2_id = anchor_pair[1]
            anchor_pair_ids = (anc1_id, anc2_id)
            if anchor_pair_ids not in anchor_pairs:
                anchor_pairs.append(anchor_pair_ids)

    return anchor_pairs

def color_graph_nodes_according_to_error(graph, tdoa_error, anchor_pair):
    # color in continous gradient of red
    # get max error
    max_error = 0
    for node1, anchor_measurements_dict in tdoa_error.items():
        for anchor_pair_1, error in anchor_measurements_dict.items():
            if anchor_pair_1 == anchor_pair and error > max_error:
                max_error = error

    # color nodes according to error
    for node1, anchor_measurements_dict in tdoa_error.items():
        for anchor_pair_1, error in anchor_measurements_dict.items():
            if anchor_pair_1 == anchor_pair:
                # color node
                color = error / max_error
                graph.nodes[node1]['color'] = color

    # color all uncolored nodes with gray
    for node1 in graph.nodes():
        if 'color' not in graph.nodes[node1]:
            graph.nodes[node1]['color'] = 0.5

def min_measurements(measurements, min_count):
    measurements_filtered = {}
    for node1, anchor_measurements_dict in measurements.items():
        if node1 not in measurements_filtered:
            measurements_filtered[node1] = {}
        for anchor_pair, distances in anchor_measurements_dict.items():
            if len(distances) >= min_count:
                measurements_filtered[node1][anchor_pair] = distances

    return measurements_filtered

def overlay_error_as_heatmap_on_graph(node_positions, tdoa_error, anchor_pair, vmin, vmax):
    tdoa_errors_for_anchor_pair = {}
    for node1, distances in tdoa_error.items():
        for (anchor1, anchor2), error in distances.items():
            anchor1_id = anchor1
            anchor2_id = anchor2
            if (anchor1_id, anchor2_id) == anchor_pair or (anchor2_id, anchor1_id) == anchor_pair:
                tdoa_errors_for_anchor_pair[node1] = error


    # Generate heatmap
    x = []
    y = []
    z = []

    max_error = 0
    min_error = 100000
    for key in tdoa_errors_for_anchor_pair:
        if tdoa_errors_for_anchor_pair[key] > max_error:
            max_error = tdoa_errors_for_anchor_pair[key]
        if tdoa_errors_for_anchor_pair[key] < min_error:
            min_error = tdoa_errors_for_anchor_pair[key]

    # max_error = np.log(max_error)
    # min_error = np.log(min_error)
    max_error = (max_error)
    min_error = (min_error)    
    
    for key in node_positions:
        if key in tdoa_errors_for_anchor_pair:
            x.append(node_positions[key][0])
            y.append(node_positions[key][1])
            z.append((tdoa_errors_for_anchor_pair[key]))

            print(f"{key}: {tdoa_errors_for_anchor_pair[key]}")

    grid_x, grid_y = np.mgrid[min(x):max(x):1000j, min(y):max(y):1000j]
    grid_z = griddata((x, y), z, (grid_x, grid_y), method = 'nearest')
    # grid_z = griddata((x, y), z, (grid_x, grid_y), method = 'cubic')    



    # chooose color map, high errors should be red
    plt.imshow(grid_z.T, extent=(min(x), max(x), min(y), max(y)), origin='lower', cmap='Reds', vmin=vmin, vmax=vmax)


#  color anchors in green, all other nodes in gray
def color_nodes(graph, anchor_pair, measurements):
    for node1 in graph.nodes():
        if node1 in anchor_pair:
            graph.nodes[node1]['color'] = 'green'
        else:
            graph.nodes[node1]['color'] = 'gray'

    for node1, measurements_dict in measurements.items():
        for (anc1, anc2), distances in measurements_dict.items():
            if (anc1, anc2) == anchor_pair or (anc2, anc1) == anchor_pair:
                graph.nodes[node1]['color'] = 'purple'


def plot_include_image_in_background(file_name, scale, offset):
    img = plt.imread(file_name)
    # scale but keep proportions
    # x = img.shape[1] * scale
    # y = img.shape[0] * scale
    # plt.imshow(img, extent=[offset[0], offset[0] + x, offset[1], offset[1] + y], alpha=0.5)

    # mirror y and mirror x
    x = img.shape[1] * scale
    y = img.shape[0] * scale
    plt.imshow(img, extent=[offset[0] + x , offset[0], offset[1] + y, offset[1]])

    

    
    
# Load Evaluation Data

node_list = get_node_list("toulouse", "dwm1001:dw1000")
node_positions = get_position_mapping(node_list)
network_graph = generate_networkx_graph("toulouse", "dwm1001:dw1000")



# Experiment with 1-2-13 anchors
# measurements = load_tdoa_measurements("tdoa_measurements_centimeters.txt")
# mean_tdoa = calculate_mean_distance(measurements)
# ground_truths = calculate_ground_truth_distance_differences(node_positions, measurements)
# error = calculate_error(mean_tdoa, ground_truths)

# plot_all_measurements(measurements, ground_truths)

# overlay_error_as_heatmap_on_graph(node_positions, error, ("dwm1001-1", "dwm1001-2"))
# # plt.show()

# # draw graph
# # create_edges_for_tdoa(network_graph, measurements)
# edge_colors = [network_graph[u][v]['color'] for u,v in network_graph.edges()]
# node_colors = [network_graph.nodes[node]['color'] for node in network_graph.nodes()]
# nx.draw(network_graph, nx.get_node_attributes(network_graph, 'pos'), edge_color=edge_colors, node_color=node_colors, with_labels=True)
# plt.show()

# print(node_positions)


# -------------
# Experiment with 29 33 26 anchors
# measurements =  load_tdoa_measurements_from_pickle("tdoa_measurements_29_33_26_medium.pkl")
# measurements = filter_measurements_only_ceiling(filter_outliers_in_measurements(measurements), node_positions)
# mean_tdoa = calculate_mean_distance(measurements)
# ground_truths = calculate_ground_truth_distance_differences(node_positions, measurements)
# error = calculate_error(mean_tdoa, ground_truths)
# anchors = get_anchor_pairs_in_measurements(measurements)

# # plot_all_measurements(measurements, ground_truths)

# for anchor_pair in anchors:
#     plt.figure(figsize=(10, 7))
    

#     overlay_error_as_heatmap_on_graph(node_positions, error, anchor_pair, 0, 150)
#     plot_include_image_in_background("locura_iotlab_map_no_labels.png", 0.01484651, np.array([-4.85, -3.4]))
#     color_nodes(network_graph, anchor_pair, measurements)
#     # get labels from labels attribute
#     short_labels = [network_graph.nodes[node]['label'] for node in network_graph.nodes()]
#     node_colors = [network_graph.nodes[node]['color'] for node in network_graph.nodes()]
#     nx.draw(network_graph, nx.get_node_attributes(network_graph, 'pos'), node_color=node_colors, alpha=0.5, labels=dict(zip(network_graph.nodes(), short_labels)))
#     plt.show()



# Experiment with 69 70 73
# measurements =  load_tdoa_measurements_from_pickle("tdoa_measurements_69_70_73_longer_no_calibration.pkl")
# # measurements = filter_outliers_in_measurements(measurements)
# mean_tdoa = calculate_mean_distance(measurements)
# ground_truths = calculate_ground_truth_distance_differences(node_positions, measurements)
# error = calculate_error(mean_tdoa, ground_truths)
# anchors = get_anchor_pairs_in_measurements(measurements)

# plot_all_measurements(measurements, ground_truths)

# for anchor_pair in anchors:
#     plt.figure(figsize=(10, 7))

#     overlay_error_as_heatmap_on_graph(node_positions, error, anchor_pair, 0, 20)
#     plot_include_image_in_background("locura_iotlab_map_no_labels.png", 0.01484651, np.array([-4.85, -3.4]))
#     color_nodes(network_graph, anchor_pair, measurements)
#     # get labels from labels attribute
#     short_labels = [network_graph.nodes[node]['label'] for node in network_graph.nodes()]
#     node_colors = [network_graph.nodes[node]['color'] for node in network_graph.nodes()]
#     nx.draw(network_graph, nx.get_node_attributes(network_graph, 'pos'), node_color=node_colors, alpha=0.5, labels=dict(zip(network_graph.nodes(), short_labels)))
#     plt.show()


# Experiment with 69 70 73 lower bitrate
measurements =  load_tdoa_measurements_from_pickle("tdoa_measurements_69_70_73_6.8mbps_chan_7_overflow_corr.pkl")
measurements = filter_outliers_in_measurements(measurements)
mean_tdoa = calculate_mean_distance(measurements)
ground_truths = calculate_ground_truth_distance_differences(node_positions, measurements)
error = calculate_error(mean_tdoa, ground_truths)
anchors = get_anchor_pairs_in_measurements(measurements)

# plot_all_measurements(measurements, ground_truths)

for anchor_pair in anchors:
    plt.figure(figsize=(10, 7))

    overlay_error_as_heatmap_on_graph(node_positions, error, anchor_pair, 0, 100)
    plot_include_image_in_background("locura_iotlab_map_no_labels.png", 0.01484651, np.array([-4.85, -3.4]))
    color_nodes(network_graph, anchor_pair, measurements)
    # get labels from labels attribute
    short_labels = [network_graph.nodes[node]['label'] for node in network_graph.nodes()]
    node_colors = [network_graph.nodes[node]['color'] for node in network_graph.nodes()]
    nx.draw(network_graph, nx.get_node_attributes(network_graph, 'pos'), node_color=node_colors, alpha=0.5, labels=dict(zip(network_graph.nodes(), short_labels)))
    plt.show()


# measurements =  load_measurements_from_twr_pickle("td_measurements.pkl")
# # measurements = filter_outliers_in_measurements(measurements)
# mean_tdoa = calculate_mean_distance(measurements)
# ground_truths = calculate_ground_truth_distance_differences(node_positions, measurements)
# error = calculate_error(mean_tdoa, ground_truths)
# anchors = get_anchor_pairs_in_measurements(measurements)

# # plot_all_measurements(measurements, ground_truths)

# for anchor_pair in anchors:
#     plt.figure(figsize=(10, 7))

#     overlay_error_as_heatmap_on_graph(node_positions, error, anchor_pair, 0, 100)
#     plot_include_image_in_background("locura_iotlab_map_no_labels.png", 0.01484651, np.array([-4.85, -3.4]))
#     color_nodes(network_graph, anchor_pair, measurements)
#     # get labels from labels attribute
#     short_labels = [network_graph.nodes[node]['label'] for node in network_graph.nodes()]
#     node_colors = [network_graph.nodes[node]['color'] for node in network_graph.nodes()]
#     nx.draw(network_graph, nx.get_node_attributes(network_graph, 'pos'), node_color=node_colors, alpha=0.5, labels=dict(zip(network_graph.nodes(), short_labels)))
#     plt.show()    
