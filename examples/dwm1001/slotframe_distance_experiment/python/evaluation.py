import numpy as np
import matplotlib.pyplot as plt
import sys

file_name = None

if len(sys.argv) > 1:
    file_name = sys.argv[1]
else:
    file_name = 'distance_measurements-toulouse-single-channel-7.txt'

DW_TIMESTAMP_MAX_VALUE = 0xFFFFFFFFFF

lille_node_positions = {
    1: [23.31, 0.26, 7.55],
    2: [24.51, 0.26, 8.96],
    3: [26.91, 0.26, 8.96],
}

toulouse_node_positions = {
    1: [-3.962, 7.931, 2.65],
    2: [-3.132, 4.339, 2.65],
}

def calculate_ground_truth_distance(testbed_positions, id1, id2):
    pos1 = testbed_positions[id1]
    pos2 = testbed_positions[id2]
    return np.sqrt((pos1[0] - pos2[0])**2 + (pos1[1] - pos2[1])**2 + (pos1[2] - pos2[2])**2)

def overflow_integer_correct(high_ts, low_ts):
    if high_ts < low_ts:
        return (DW_TIMESTAMP_MAX_VALUE - low_ts) + high_ts
    else:
        return high_ts - low_ts

def calculate_distance_from_timestamps(timestamp_list):
    # 6 integer timestamps
    t_a1 = timestamp_list[0]
    r_b1 = timestamp_list[1]
    t_b1 = timestamp_list[2]
    r_a1 = timestamp_list[3]
    t_a2 = timestamp_list[4]
    r_b2 = timestamp_list[5]


    initiator_roundtrip = overflow_integer_correct(r_a1, t_a1)
    initiator_reply = overflow_integer_correct(t_a2, r_a1)
    replier_roundtrip = overflow_integer_correct(r_b2, t_b1)
    replier_reply = overflow_integer_correct(t_b1, r_b1)

    nom = (initiator_roundtrip * replier_roundtrip - initiator_reply * replier_reply)
    denom = initiator_roundtrip + replier_roundtrip + initiator_reply + replier_reply

    return 1/2 * float(nom) / float(denom)

def convert_timestamp_list_to_distance_list(timestamps_list):
    distance_list = []
    for timestamps in timestamps_list:
        distance_list.append(calculate_distance_from_timestamps(timestamps))
        
    return distance_list
        
def load_distances_from_timestamps_measurements(file_name):
    s = open(file_name, 'r').read()
    timestamps = eval(s)
    distance_measurements = {}
    
    for node1, timestamp_dict in timestamps.items():
        if node1 not in distance_measurements:
            distance_measurements[node1] = {}
        for node2, timestamps_list, in timestamp_dict.items():
            distance_measurements[node1][node2] = convert_timestamp_list_to_distance_list(timestamps_list)
    
    return distance_measurements

# load the range estimations which where directly processed onboard of the device
def load_distances_from_distance_measurements(file_name):
    s = open(file_name, 'r').read()
    distances = eval(s)
    dists = {}
    cfos = {}
    
    for node1, distances_dict in distances.items():
        if node1 not in dists:
            dists[node1] = {}
            cfos[node1] = {}
            
        for node2, distances_list, in distances_dict.items():
            if node2 not in dists[node1]:
                dists[node1][node2] = []
                cfos[node1][node2] = []
            
            for (slot_dist, cfo, range) in distances_list:
                dists[node1][node2].append(range)
                cfos[node1][node2].append(cfo)
    
    return dists, cfos

def load_distance_differences_from_measurents_file_without_slotdistances(file_name):
    s = open(file_name, 'r').read()
    evaluation_dict = eval(s)

    distance_differences = {}
    for passive_node, measurements_dict in evaluation_dict.items():
        if passive_node not in distance_differences:
            distance_differences[passive_node] = {}
            
        for anchor_pair, per_slot_distance_measurements_dict in measurements_dict.items():
            if anchor_pair not in distance_differences[passive_node]:
                distance_differences[passive_node][anchor_pair] = []

            for slot_distance, distance_differences_list in per_slot_distance_measurements_dict.items():
                distance_differences[passive_node][anchor_pair].extend(distance_differences_list)

    return distance_differences

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

def load_tdoa_from_measurents_file_with_slot_distances(file_name):
    s = open(file_name, 'r').read()
    evaluation_dict = eval(s)

    distance_differences = {}
    for passive_node, measurements_dict in evaluation_dict.items():
        if passive_node not in distance_differences:
            distance_differences[passive_node] = {}
            
        for anchor_pair, per_slot_distance_measurements_dict in measurements_dict.items():
            if anchor_pair not in distance_differences[passive_node]:
                distance_differences[passive_node][anchor_pair] = {}

            for slot_distance, distance_differences_list in per_slot_distance_measurements_dict.items():
                distance_differences[passive_node][anchor_pair][slot_distance] = filter_outliers(distance_differences_list)

    return distance_differences


def load_twr_from_measurements_file_with_slot_distances(file_name):
    s = open(file_name, 'r').read()
    distances = eval(s)
    dists = {}
    
    for node1, distances_dict in distances.items():
        if node1 not in dists:
            dists[node1] = {}
            
        for node2, distances_list, in distances_dict.items():
            if node2 not in dists[node1]:
                dists[node1][node2] = {}
            
            for (slot_dist, cfo, range) in distances_list:
                if slot_dist not in dists[node1][node2]:
                    dists[node1][node2][slot_dist] = []

                if abs(range) > 5e2:
                    print("Range is too large: " + str(range))
                    continue

                dists[node1][node2][slot_dist].append(range)

            # dists[node1][node2][slot_dist] = filter_outliers(dists[node1][node2][slot_dist])

    return dists

    

# ------- Plotting Code---------

# ts_distances = load_distances_from_timestamps_measurements("raw_timestamps_inbetween_rx.txt")
# distances_1_2_from_timestamps = filter_outliers(ts_distances[1][2])

def plot_ground_truth(ax, testbed_positions, id1, id2):
    dist = calculate_ground_truth_distance(testbed_positions, id1, id2) * 100
    ax.axhline(y=dist, color='r', linestyle='-', label='Ground truth')
    

def plot_cfos_and_distances(ax, dists, cfos, experiment_desc):
    # some 
    color = 'tab:red'
    ax.set_xlabel('Measurement number')
    ax.set_ylabel('Distance [cm]', color=color)
    ax.scatter(range(len(dists)), dists, color=color, alpha=0.5, marker='x')
    ax.tick_params(axis='y', labelcolor=color)

    ax2 = ax.twinx()  # instantiate a second axes that shares the same x-axis
    color = 'tab:blue'
    ax2.set_ylabel('CFO [Raw]', color=color)  # we already handled the x-label with ax1
    ax2.scatter(range(len(cfos)), cfos, color=color, marker='x', alpha=0.5)
    ax2.tick_params(axis='y', labelcolor=color)

    fig.tight_layout()  # otherwise the right y-label is slightly clipped
    plt.title(experiment_desc)


def plot_distance_differences(ax, distance_differences, experiment_desc):
    color = 'tab:red'
    ax.set_xlabel('Measurement number')
    ax.set_ylabel('Distance difference [cm]', color=color)
    ax.scatter(range(len(distance_differences)), distance_differences, color=color, alpha=0.5, marker='x')
    ax.tick_params(axis='y', labelcolor=color)

    fig.tight_layout()  # otherwise the right y-label is slightly clippedn
    plt.title(experiment_desc)

def plot_twr_per_slot_dist(ax, dist_distances_dict, experiment_desc):
    means = []
    variances = []
    for dist, ranges in dist_distances_dict.items():
        means.append(np.mean(ranges))
        variances.append(np.var(ranges))

    print(means)
    print(variances)

    # for each distance plot means and variances using error bars
    ax.errorbar(range(len(means)), means, yerr=variances, fmt='o', color='black',
                    ecolor='lightgray', elinewidth=3, capsize=0)
    ax.set_xlabel('Slot distance')
    ax.set_ylabel('Distance [cm]')
    ax.set_xticks(range(len(means)))
    ax.set_xticklabels(dist_distances_dict.keys())
    plt.title(experiment_desc)


# ---------------------- distances per slot -----------------------
fig, ax = plt.subplots()
dists_more_adv = load_twr_from_measurements_file_with_slot_distances("distance_measurements_long_with_more_advertisement.txt")
plot_twr_per_slot_dist(ax, dists_more_adv[2][1], "Distances per slot distance")
plt.show()

# ---------------------- dist diff per slot -----------------------
fig, ax = plt.subplots()
dists_diff_more_adv = load_tdoa_from_measurents_file_with_slot_distances("distances_differences_long_with_more_advertising.txt")
plot_twr_per_slot_dist(ax, dists_diff_more_adv[13][(2,1)], "Distances per slot distance")
plt.show()


# ----------------------- Distance Differences Experiments --------------------#
dists_diffs_float = load_distance_differences_from_measurents_file_without_slotdistances("distance_differences_float.txt")
dists_diffs_double = load_distance_differences_from_measurents_file_without_slotdistances("distance_differences_double.txt")
dists_diffs_more_double = load_distance_differences_from_measurents_file_without_slotdistances("distance_differences_even_more_double.txt")
dists_diffs_double_256 = load_distance_differences_from_measurents_file_without_slotdistances("distance_differences_even_double_256_preamble.txt")
dists_diffs_double_256_long = load_distance_differences_from_measurents_file_without_slotdistances("distance_differences_double_really_long_256_preamble.txt")
dists_diffs_more_adv = load_distance_differences_from_measurents_file_without_slotdistances("distances_differences_long_with_more_advertising.txt")

fig, axs = plt.subplots(4, 1, sharex=True)

current_ax = 0
for node_passive, measurements_dict in dists_diffs_more_adv.items():
    for anchor_pair, dist_diffs in measurements_dict.items():
        ax = axs[current_ax]
        current_ax += 1
        plot_distance_differences(ax, filter_outliers(dist_diffs), f"chan 7 (PHY 5 PRF 64), 2.5ms slot length, {node_passive} with {anchor_pair}, 256 preamble" )
        
plt.show()
    
                              
# ----------------------- Distance Experiments --------------------#
dists7, cfos7 = load_distances_from_distance_measurements("distances_with_cfos_chan_7_2500us.txt")
dists3, cfos3 = load_distances_from_distance_measurements("distances_with_cfos_chan_3.txt")
dists11, cfos11 = load_distances_from_distance_measurements("distances_with_cfos_chan_11_2500us.txt")
dists_256, cfos_256 = load_distances_from_distance_measurements("distances_with_cfos_chan_7_2500us_256_preamble.txt")
dists_64, cfos_64 = load_distances_from_distance_measurements("distances_with_cfos_chan_7_2500us_64_preamble.txt")
dists_880kb, cfos_880kb = load_distances_from_distance_measurements("distances_with_cfos_880kbs.txt")
# there was a bug in the code during channel selection, this data is measured with the fix in place
dists7_corr, cfos7_corr = load_distances_from_distance_measurements("distances_with_cfos_chan_7_2500us_corrected_code.txt")
dists3_corr, cfos3_corr = load_distances_from_distance_measurements("distances_with_cfos_chan_3_2500us_corrected_code.txt")
dists_256_double, cfo_256_double = load_distances_from_distance_measurements("distances_with_cfos_double_256_preamble.txt")
dists_256_long, cfo_256_long = load_distances_from_distance_measurements("distance_measurements_double_256_preamble_really_long.txt")
dists_more_adv, cfo_more_adv = load_distances_from_distance_measurements("distance_measurements_long_with_more_advertisement.txt")





# fig, (ax1, ax2, ax3, ax4, ax5, ax6, ax7, ax8, ax9) = plt.subplots(9, 1, figsize=(15, 30))
# plot_cfos_and_distances(ax1, filter_outliers(dists7[1][2]), cfos7[1][2], "chan 7 (PHY 5 PRF 64), 2.5ms slot length, 1 <-> 2, 128 preamble" )
# plot_cfos_and_distances(ax2, filter_outliers(dists_880kb[1][2]), cfos_880kb[1][2], "chan 7 (PHY 5 PRF 64), 2.5ms slot length, 1 <-> 2, 880kbps, 128 preamble" )
# plot_cfos_and_distances(ax3, filter_outliers(dists11[1][2]), cfos11[1][2], "chan 11, 2.5ms slot length, 1 <-> 2, 128 preamble" )
# plot_cfos_and_distances(ax4, filter_outliers(dists_64[1][2]), cfos_64[1][2], "chan 7 (PHY 5 PRF 64), 2.5ms slot length, 1 <-> 2, 64 preamble" )
# plot_cfos_and_distances(ax5, filter_outliers(dists_256[1][2]), cfos_256[1][2], "chan 7 (PHY 5 PRF 64), 2.5ms slot length, 1 <-> 2, 256 preamble" )
# plot_cfos_and_distances(ax6, filter_outliers(dists3[1][2]), cfos3[1][2], "chan 3 (PHY 5 PRF 16), 5ms slot length, 1 <-> 2, 128 preamble" )
# plot_cfos_and_distances(ax7, filter_outliers(dists_256[2][1]), cfos_256[2][1], "chan 7 (PHY 5 PRF 64), 2.5ms slot length, 2 <-> 1, 256 preamble" )
# plot_cfos_and_distances(ax8, filter_outliers(dists7_corr[1][2]), cfos7_corr[1][2], "chan 7 (PHY 5 PRF 64), 2.5ms slot length, 1 <-> 2, 128 preamble, corrected code" )
# plot_cfos_and_distances(ax9, filter_outliers(dists3_corr[1][2]), cfos3_corr[1][2], "chan 3 (PHY 5 PRF 16), 2.5ms slot length, 1 <-> 2, 128 preamble, corrected code" )
# plot_ground_truth(ax1, toulouse_node_positions, 1, 2)
# plot_ground_truth(ax2, toulouse_node_positions, 1, 2)
# plot_ground_truth(ax3, toulouse_node_positions, 1, 2)
# plot_ground_truth(ax4, toulouse_node_positions, 1, 2)
# plot_ground_truth(ax5, toulouse_node_positions, 1, 2)
# plot_ground_truth(ax6, toulouse_node_positions, 1, 2)
# plot_ground_truth(ax7, toulouse_node_positions, 2, 1)
# plot_ground_truth(ax8, toulouse_node_positions, 1, 2)
# plot_ground_truth(ax9, toulouse_node_positions, 1, 2)

fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(15, 10))
plot_cfos_and_distances(ax1, filter_outliers(dists_256_double[1][2]), cfo_256_double[1][2], "chan 7 (PHY 5 PRF 64), 2.5ms slot length, 1 <-> 2, 256 preamble, double" )
plot_cfos_and_distances(ax2, filter_outliers(dists_256_long[1][2]), cfo_256_long[1][2], "chan 7 (PHY 5 PRF 64), 2.5ms slot length, 1 <-> 2, 256 preamble, really long" )
plot_cfos_and_distances(ax3, dists_more_adv[2][1], cfo_more_adv[2][1], "chan 7 (PHY 5 PRF 64), 2.5ms slot length, 1 <-> 2, 256 preamble, more adv" )
plot_ground_truth(ax1, toulouse_node_positions, 1, 2)
plot_ground_truth(ax2, toulouse_node_positions, 1, 2)
plot_ground_truth(ax3, toulouse_node_positions, 1, 2)

# save as img
plt.savefig("distances_image_complete.png")
plt.show()

