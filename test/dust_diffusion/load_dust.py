#!/usr/bin/env python3

import os
import re

import numpy as np
import matplotlib.pyplot as plt
import astropy.units as u

def get_output_times(datadir, units=True):
    filepath = os.path.join(datadir, "snapshots/timeSnapshot.dat")
    t = np.genfromtxt(filepath, usecols=2)
    # get unit
    if units:
        timeunit = None
        with open(filepath, "r") as infile:
            for line in infile:
                m = re.search("physical time \| ([0-9e+-\.]+) s", line)
                if m is not None:
                    timeunit = u.Unit(m.groups()[0] + "s")
        if timeunit is not None:
            t = t*timeunit
    return t


def construct_dust_trajectories(datadir, units=True):
    """ Construct trajectories for all dust particles. """
    particles = {}
    times = get_output_times(datadir, units=units)

    for n, time in enumerate(times):
        vals = get_particles(datadir, n, units=units)
        ids = vals["id"]

        if n == 0:
            for i in ids:
                particles[i] = {}
                for key in [k for k in vals.keys() if not k == "id"]:
                    particles[i][key] = []
                particles[i]["time"] = []

        for key in [k for k in vals.keys() if not k == "id"]:
            for i, v in zip(ids, vals[key]):
                particles[i][key].append(v)
        for i in ids:
            particles[i]["time"].append(time)

    if units:
        for ind, p in particles.items():
            for varname in p:
                p[varname] = u.Quantity(p[varname])

    return particles


def get_particles(datadir, N, units=True):
    filename = os.path.join(datadir, f"snapshots/{N}/particles.dat")
    res = np.fromfile(
        filename, dtype=[('Id', np.dtype(int)), ('Values', np.dtype(float), 11)])
    ids = res["Id"]
    vals = res["Values"]

    particles = {
        "id": ids,
        "r": vals[:, 0],
        "phi": vals[:, 1],
        "r dot": vals[:, 2],
        "phi dot": vals[:, 3],
        "r ddot": vals[:, 4],
        "phi ddot": vals[:, 5],
        "mass": vals[:, 6],
        "size": vals[:, 7],
        "timestep": vals[:, 8],
        "facold": vals[:, 9],
        "stokes": vals[:, 10]
    }
    
    if units:
        L0 = u.Unit("1 au")
        particles["r"] *= L0
        particles["phi"] *= L0
        particles["size"] *= L0

    # hacky way around fargocpt outputting x and y
    # for adaptive integrator
    is_cartesian = any(particles["r"] < 0)
    if is_cartesian:
        x = particles["r"]
        y = particles["phi"]

        r = np.sqrt(x**2 + y**2)
        phi = np.arctan2(x, y)
    else:
        r = particles["r"]
        phi = particles["phi"]
        if units:
            phi = phi.value*u.rad

        x = r*np.cos(phi)
        y = r*np.sin(phi)

    particles["x"] = x
    particles["y"] = y
    particles["r"] = r
    particles["phi"] = phi
    
    return particles