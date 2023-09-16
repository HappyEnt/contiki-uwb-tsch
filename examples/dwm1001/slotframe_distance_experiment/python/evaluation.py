import numpy as np
import matplotlib.pyplot as plt


# read data from distance_measurements.txt which is formatted as a python dictionary
s = open('distance_measurements.txt', 'r').read()
distance_measurements = eval(s)

dwm1001_1_xpos = 23.31
dwm1001_1_ypos = 0.26
dwm1001_1_zpos = 7.55

dwm1001_2_xpos = 24.51
dwm1001_2_ypos = 0.26
dwm1001_2_zpos = 8.96

dwm1001_3_xpos = 26.91
dwm1001_3_ypos = 0.26
dwm1001_3_zpos = 8.96

ground_truth_distance = np.sqrt((dwm1001_1_xpos - dwm1001_2_xpos)**2 + (dwm1001_1_ypos - dwm1001_2_ypos)**2 + (dwm1001_1_zpos - dwm1001_2_zpos)**2)
# dist difference between 3 to 2 and 3 to 1

dist_3_2 = np.sqrt((dwm1001_3_xpos - dwm1001_2_xpos)**2 + (dwm1001_3_ypos - dwm1001_2_ypos)**2 + (dwm1001_3_zpos - dwm1001_2_zpos)**2)
dist_3_1 = np.sqrt((dwm1001_3_xpos - dwm1001_1_xpos)**2 + (dwm1001_3_ypos - dwm1001_1_ypos)**2 + (dwm1001_3_zpos - dwm1001_1_zpos)**2)

dist_difference = dist_3_2 - dist_3_1

print("Distance difference: " + str(dist_difference))
print("Ground truth distance: " + str(ground_truth_distance))

direction_215_to_206_measurements = distance_measurements[215][206]
direction_206_to_215_measurements = distance_measurements[206][215]

# each measurement direction contains touples of slot distances (int) and measured distances (float)
# we want to plot the measured distances for each slot distance

# create dictionary with slot distances as keys and empty lists as values

max_slotdistance = 0
for measurement in direction_215_to_206_measurements:
    if measurement[0] > max_slotdistance:
        max_slotdistance = measurement[0]

slot_distances = {}
for slot_distance in range(1, max_slotdistance+1):
    slot_distances[slot_distance] = []


# fill dictionary with measured distances
for measurement in direction_215_to_206_measurements:
# for measurement in direction_206_to_215_measurements:    
    slot_distance = measurement[0]
    distance = measurement[1]
    slot_distances[slot_distance].append(distance)

def filter_outliers(distances):
    # throw away outliers through use of median
    filtered_distances = []
    median = np.median(distances)
    for distance in distances:
        if abs(distance - median) < 0.5*100:
            filtered_distances.append(distance)
    return filtered_distances


variances = []
means = []
errors = []    
# plot histogram
for slot_distance, distances in slot_distances.items():
    distances = filter_outliers(distances)
    variances.append(np.var(distances))
    means.append(np.mean(distances))
    errors.append(abs(ground_truth_distance*100 - np.mean(distances)))
    plt.hist(distances, bins=100)
    plt.title(f"Histogram of {len(distances)} distances between nodes 215 and 206 with slot distance {str(slot_distance)}")
    plt.xlabel("Distance")
    plt.ylabel("Frequency")
    plt.show()

# create plot slot_distance vs. mean + variance
print(errors)
plt.plot(list(slot_distances.keys()), errors, label="Error")
# plt.plot(list(slot_distances.keys()), variances, label="Variance")
plt.title(f"Error Between Mean of Distances and Ground Truth")
plt.xlabel("Slot Distance")
plt.ylabel("Error (cm)")
plt.legend()
plt.show()
