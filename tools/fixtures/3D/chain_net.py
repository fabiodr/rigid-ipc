import json
import pathlib
import copy
import math
import itertools

import numpy

import context

from fixture_utils import save_fixture, get_fixture_dir_path

link_thickness = 0.3  # padded_link_thickness (actual thickness: 0.190211)
link_height = 1.5
link_width = 1

scene = {
    "scene_type": "distance_barrier_rb_problem",
    "solver": "ipc_solver",
    "timestep": 0.01,
    "max_time": 1.0,
    "distance_barrier_constraint": {
        "trajectory_type": "linearized"  # TODO: replace with screwing
    },
    "rigid_body_problem": {
        "coefficient_restitution": -1,
        "gravity": [0, -9.81, 0],
        "rigid_bodies": []
    }
}

link = {
    "mesh": "wrecking-ball/link.obj",
    "position": [0, 0, 0],
    "rotation": [0, 0, 0],
    "density": 7680,
    "is_dof_fixed": False
}

bodies = scene["rigid_body_problem"]["rigid_bodies"]

net_rows = 8
net_cols = 8

row_gap = link_height / 2.5
col_gap = link_height / 10

# Generate the chain net
for i in range(net_rows):
    for j in range(net_cols):
        if (i == 0 and j == 0 or
                i == 0 and j == net_cols - 1 or
                i == net_rows - 1 and j == 0 or
                i == net_rows - 1 and j == net_cols - 1):
            continue
        bodies.append(copy.deepcopy(link))
        bodies[-1]["rotation"] = [90, 90, 0]
        bodies[-1]["position"] = [
            j * (link_height + col_gap), 0, i * (link_width + row_gap)]
        if i == 0 or i == net_rows - 1 or j == 0 or j == net_cols - 1:
            bodies[-1]["is_dof_fixed"] = True
        if i != 0 and i != net_rows - 1 and j < net_cols - 1:
            bodies.append(copy.deepcopy(link))
            bodies[-1]["rotation"] = [0, 0, 90]
            bodies[-1]["position"] = [
                (j + 0.5) * (link_height + col_gap), 0, i * (link_width + row_gap)]
        if j != 0 and j != net_rows - 1 and i < net_rows - 1:
            bodies.append(copy.deepcopy(link))
            bodies[-1]["rotation"] = [90, 0, 90]
            bodies[-1]["position"] = [
                j * (link_height + col_gap), 0, (i + 0.5) * (link_width + row_gap)]

# Add block of cubes

# cube = {
#     "mesh": "cube.obj",
#     "position": [0, 0, 0],
#     "density": 2800,
#     "is_dof_fixed": False
# }
#
# width, height, depth = 8, 7, 10
# # width, height, depth = 1, 3, 1
#
# shift = origin + [0.5 - width / 2, 0.55, 0.5 - depth / 2]
#
# for w, h, d in itertools.product(range(width), range(height), range(depth)):
#     bodies.append(copy.deepcopy(cube))
#     bodies[-1]["position"] = ([1.05 * w, 1.05 * h, 1.05 * d] + shift).tolist()


save_fixture(scene, get_fixture_dir_path() / "3D" / "chain" / "chain-net.json")
