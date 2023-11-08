#!/usr/bin/env python3

import os
import re
import yaml

import numpy as np
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

    for ind, p in particles.items():
        for varname in p:
            if units:
                p[varname] = u.Quantity(p[varname])
            else:
                p[varname] = np.array(p[varname])
    return particles

def get_particles(datadir, N, units=True):
    filename = os.path.join(datadir, f"snapshots/{N}/particles.dat")
    return load_particles_file(filename, units=units)

def get_sigma_dust(outdir, N, nbins=51):
    
    particles = get_particles(outdir, N, units=False)
    r = particles["r"]
    counts, interfaces = np.histogram(r, bins=nbins)
    mid = 0.5*(interfaces[1:] + interfaces[:-1])
    dr = interfaces[1:] - interfaces[:-1]
    sigma_dust = counts/(dr*mid*2*np.pi)
    return sigma_dust, mid, dr


def filter_by_size(particles, size=[]):
    """ Construct trajectories for all dust particles. """
    try:
        len(size)
    except TypeError:
        size = [0.9*size, 1.1*size]

    smin = size[0]
    smax = size[1]

    vals = {}
    sizes = particles["size"]

    mask = np.logical_and(sizes >= smin, sizes <= smax)
    for key in particles:
        vals[key] = particles[key][mask]

    return vals


def load_particles(file_path, rmin=None, profile=None, 
                   sigma_gas=None, dtgr=None, sortby=None):
    """ Load particle data from a FargoCPT particle file.
        
    Parameters
    ----------
    file_path: str
        Path to the particle file.
    rmin: astropy.Units.Quantity
        Minimum radius to filter out particles inwards.
    profile: func
        Function of the initial radius (in au) to scale the mass by.
    sortby: str
        Sort the particles by this key.
    """
    
    particles = load_particles_file(file_path)
    particles = load_init_radius(file_path, particles)

    # print("r0s", particles["r0"][:100])
    if rmin is not None:
        mask = np.ones(len(particles["id"]), dtype=bool)
        mask = particles["r0"] > rmin
        
        for key, val in particles.items():
            particles[key] = particles[key][mask]
    
    r0 = particles["r0"]

    mass_factor = np.ones_like(r0)
    if profile is not None:
        if hasattr(r0, "unit"):
            r0 = r0.value
        mass_factor *= profile(r0)
        
    mass_factor /= np.max(mass_factor)
    particles["mass_factor"] = mass_factor
    
    if sortby is not None:
        sorted_particles = {}
        inds = np.argsort(rv[sortby])
        for key, val in rv.items():
            sorted_particles[key] = val[inds]
        rv = sorted_particles
    return rv

def load_particles_file(filename, units=False):
    """ Load a FargoCPT particles file.

    Paramters
    ---------
    filename: str
        Path to the particles file.
    units: bool
        Apply astropy units or not.

    Returns
    -------
    dict
        Dictionary containing id, x, y and size (the particle radius).
    """
    res = np.fromfile(
        filename, dtype=[('Id', np.dtype(int)), ('Values', np.dtype(float), 11)])
    ids = res["Id"]
    vals = res["Values"]

    particles = {
        "id": ids,
        "r": vals[:, 0],
        "phi": vals[:, 1],
        "r_dot": vals[:, 2],
        "phi_dot": vals[:, 3],
        "r_ddot": vals[:, 4],
        "phi_ddot": vals[:, 5],
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

def load_init_radius(file_path, particles):
    first_file = get_first_particle_file(os.path.dirname(file_path))
    first_particles = load_particles_file(first_file)
    r0s_init = {i : r for i,r in zip(first_particles["id"], first_particles["r"])}
    r0s = []
    for i in particles["id"]:
        r0 = r0s_init[i]
        r0s.append(r0)
    particles["r0"] = np.array(r0s)
    return particles

def get_first_particle_file(outdir):
    numbers = [int(m.groups()[0]) for m in [re.match("particles(\d+)\.dat", f) for f in os.listdir(outdir)] if m is not None]
    n = sorted(numbers)[0]
    first_file = f"{outdir}/particles{n}.dat"
    return first_file

def get_time(datadir, N):
    filename = os.path.join(datadir, f"snapshots/timeSnapshot.dat")
    times = np.genfromtxt(filename, usecols=(2))
    units = get_units(datadir)
    times = (times*units["time"]).to_value("yr")
    return times[N]

def get_units(datadir):
    filename = os.path.join(datadir, "units.yml")

    unit_dict = {}
    if os.path.isfile(filename):
        with open(filename) as f:
            data = yaml.safe_load(f)
        unit_dict = {}
        for key, val in data.items():
            unit_dict[key] = u.Unit(val["unit"])
    else:
        raise ValueError("Error in read_units, could not find : " + filename  + "!\n")

    return unit_dict