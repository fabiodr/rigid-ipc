#!/usr/local/bin/python3
"""
Script to generate a fixture of a chain with N simple links.

Usage: python generate_simple_chainmail_fixture.py N
"""

import sys
import json
import numpy
import pathlib

from default_fixture import generate_default_fixture

vertices = numpy.array([[-1.5 * 1.625, 0.0],
                        [0.0, 0.0],
                        [1.5 * 1.625, 0.0],
                        [-3.0, -3.0],
                        [0.0, -3.0],
                        [3.0, -3.0],
                        [-3.0, -6.0],
                        [-1.5 * 0.375, -6.0],
                        [1.5 * 0.375, -6.0],
                        [3.0, -6.0]], dtype=float)

edges = numpy.array([[0, 1],
                     [1, 2],
                     [1, 4],
                     [3, 4],
                     [4, 5],
                     [3, 6],
                     [5, 9],
                     [6, 7],
                     [8, 9]], dtype=int)


def generate_fixture(n_links: int) -> dict:
    """Generate a fixture of a chain with N simple links."""
    fixture = generate_default_fixture()
    rigid_bodies = fixture["rigid_body_problem"]["rigid_bodies"]
    for i in range(n_links):
        rigid_bodies.append({
            "vertices": (vertices + [0, -4.5 * i]).tolist(),
            "edges": edges.tolist(),
            "velocity": [0.0, -1.0 if i else 0.0, 0.0],
            "is_dof_fixed": [i == 0, i == 0, i == 0]
        })
    return fixture


def main() -> None:
    """Parse command-line arguments to generate the desired fixture."""
    assert(len(sys.argv) >= 2)

    n_links = int(sys.argv[1])
    fixture = generate_fixture(n_links)

    if(len(sys.argv) > 2):
        out_path = pathlib.Path(sys.argv[2])
        out_path.parent.mkdir(parents=True, exist_ok=True)
    else:
        directory = (pathlib.Path(__file__).resolve().parents[1] /
                     "fixtures" / "chain")
        directory.mkdir(parents=True, exist_ok=True)
        out_path = directory / f"simple_{n_links:d}_link_chain.json"
    with open(out_path, 'w') as outfile:
        json.dump(fixture, outfile)


if __name__ == "__main__":
    main()
