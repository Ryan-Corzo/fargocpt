#include "quantities.h"

#include "Theo.h"
#include "constants.h"
#include "global.h"
#include "logging.h"
#include "parameters.h"
#include "stress.h"
#include "util.h"
#include <math.h>
#include <mpi.h>
#include <vector>
#include "SourceEuler.h"
#include "pvte_law.h"


extern boolean Corotating;
extern double M0;

namespace quantities
{

/**
	Calculates total gas mass.
*/
double gas_total_mass(t_data &data, const double quantitiy_radius)
{
    double local_mass = 0.0;
    double global_mass = 0.0;

    // calculate mass of this process' cells
    for (unsigned int n_radial = radial_first_active;
	 n_radial < radial_active_size; ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::DENSITY].get_size_azimuthal();
	     ++n_azimuthal) {
	    if (Rmed[n_radial] <= quantitiy_radius) {
		local_mass += Surf[n_radial] *
			      data[t_data::DENSITY](n_radial, n_azimuthal);
	    }
	}
    }

    MPI_Allreduce(&local_mass, &global_mass, 1, MPI_DOUBLE, MPI_SUM,
		  MPI_COMM_WORLD);

    return global_mass;
}


double gas_quantity_reduce(const t_polargrid& arr, const double quantitiy_radius)
{

	double global_reduced_quantity = 0.0;
	double local_reduced_quantity = 0.0;

	// Loop thru all cells excluding GHOSTCELLS & CPUOVERLAP cells (otherwise
	// they would be included twice!)
	for (unsigned int nr = radial_first_active; nr < radial_active_size; ++nr) {
	for (unsigned int naz = 0; naz < arr.get_size_azimuthal(); ++naz) {
		// eccentricity and semi major axis weighted with cellmass
		if (Rmed[nr] <= quantitiy_radius) {
		local_reduced_quantity += arr(nr, naz) ;
		}
	}
	}

	// synchronize threads
	MPI_Allreduce(&local_reduced_quantity, &global_reduced_quantity, 1, MPI_DOUBLE, MPI_SUM,
		  MPI_COMM_WORLD);

	return global_reduced_quantity;
}

double gas_quantity_mass_average(t_data &data, const t_polargrid& arr, const double quantitiy_radius)
{

	const t_polargrid& sigma = data[t_data::DENSITY];

	double local_mass = 0.0;
	double global_mass = 0.0;

	double global_reduced_quantity = 0.0;
	double local_reduced_quantity = 0.0;

	// Loop thru all cells excluding GHOSTCELLS & CPUOVERLAP cells (otherwise
	// they would be included twice!)
	for (unsigned int nr = radial_first_active; nr < radial_active_size; ++nr) {
	for (unsigned int naz = 0; naz < arr.get_size_azimuthal(); ++naz) {
		// eccentricity and semi major axis weighted with cellmass
		if (Rmed[nr] <= quantitiy_radius) {
		const double cell_mass = sigma(nr, naz) * Surf[nr];
		local_mass += cell_mass;
		local_reduced_quantity += arr(nr, naz) * cell_mass;
		}
	}
	}

	MPI_Allreduce(&local_mass, &global_mass, 1, MPI_DOUBLE, MPI_SUM,
		  MPI_COMM_WORLD);

	// synchronize threads
	MPI_Allreduce(&local_reduced_quantity, &global_reduced_quantity, 1, MPI_DOUBLE, MPI_SUM,
		  MPI_COMM_WORLD);

	global_reduced_quantity /= global_mass;
	return global_reduced_quantity;
}


/**
 * @brief gas_disk_radius
 * @param data
 * @return
 */
double gas_disk_radius(t_data &data, const double total_mass)
{

    const unsigned int local_array_start = Zero_or_active;
    const unsigned int local_array_end = Max_or_active;
    const unsigned int send_size = local_array_end - local_array_start;

    std::vector<double> local_mass(send_size);

    for (unsigned int n_radial = local_array_start; n_radial < local_array_end;
	 ++n_radial) {
	local_mass[n_radial - local_array_start] = 0.0;
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::DENSITY].get_size_azimuthal();
	     ++n_azimuthal) {
	    local_mass[n_radial - local_array_start] +=
		Surf[n_radial] * data[t_data::DENSITY](n_radial, n_azimuthal);
	}
    }
    double radius = 0.0;
    double current_mass = 0.0;

    MPI_Gatherv(&local_mass[0], send_size, MPI_DOUBLE, GLOBAL_bufarray,
		RootNradialLocalSizes, RootNradialDisplacements, MPI_DOUBLE, 0,
		MPI_COMM_WORLD);

    if (CPU_Master) {
	int j = -1;
	for (int rank = 0; rank < CPU_Number; ++rank) {
	    int id = RootRanksOrdered[rank];
	    for (int i = RootIMIN[id]; i <= RootIMAX[id]; ++i) {
		++j;
		current_mass += GLOBAL_bufarray[i];
		if (current_mass > 0.99 * total_mass) {
		    radius = GlobalRmed[j];
		    goto found_radius; // break out of nested loop
		}
	    }
	}

    found_radius:
	void();
    }
    return radius;
}

/**
	Calculates angular gas momentum.
*/
double gas_angular_momentum(t_data &data, const double quantitiy_radius)
{
    double local_angular_momentum = 0.0;
    double global_angular_momentum = 0.0;

    for (unsigned int n_radial = radial_first_active;
	 n_radial < radial_active_size; ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::DENSITY].get_size_azimuthal();
	     ++n_azimuthal) {
	    if (Rmed[n_radial] <= quantitiy_radius) {
		local_angular_momentum +=
		    Surf[n_radial] * 0.5 *
		    (data[t_data::DENSITY](n_radial, n_azimuthal) +
		     data[t_data::DENSITY](
			 n_radial,
			 n_azimuthal == 0
			     ? data[t_data::DENSITY].get_max_azimuthal()
			     : n_azimuthal - 1)) *
		    Rmed[n_radial] *
		    (data[t_data::V_AZIMUTHAL](n_radial, n_azimuthal) +
		     OmegaFrame * Rmed[n_radial]);
	    }
	    // local_angular_momentum +=
	    // Surf[n_radial]*data[t_data::DENSITY](n_radial,n_azimuthal)*Rmed[n_radial]*(0.5*(data[t_data::V_AZIMUTHAL](n_radial,n_azimuthal)+data[t_data::V_AZIMUTHAL](n_radial,n_azimuthal
	    // == data[t_data::V_AZIMUTHAL].get_max_azimuthal() ? 0 :
	    // n_azimuthal+1))+OmegaFrame*Rmed[n_radial]);
	}
    }

    MPI_Allreduce(&local_angular_momentum, &global_angular_momentum, 1,
		  MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

    return global_angular_momentum;
}

/**
	Calculates gas internal energy
*/
double gas_internal_energy(t_data &data, const double quantitiy_radius)
{
    double local_internal_energy = 0.0;
    double global_internal_energy = 0.0;

    for (unsigned int n_radial = radial_first_active;
	 n_radial < radial_active_size; ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::ENERGY].get_size_azimuthal();
	     ++n_azimuthal) {
	    if (Rmed[n_radial] <= quantitiy_radius) {
		local_internal_energy +=
		    Surf[n_radial] *
		    data[t_data::ENERGY](n_radial, n_azimuthal);
	    }
	}
    }

    MPI_Reduce(&local_internal_energy, &global_internal_energy, 1, MPI_DOUBLE,
	       MPI_SUM, 0, MPI_COMM_WORLD);

    return global_internal_energy;
}

double gas_viscous_dissipation(t_data &data, const double quantitiy_radius)
{
    double local_qplus = 0.0;
    double global_qplus = 0.0;

    for (unsigned int n_radial = radial_first_active;
	 n_radial < radial_active_size; ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::QPLUS].get_size_azimuthal();
	     ++n_azimuthal) {
	    if (Rmed[n_radial] <= quantitiy_radius) {
		local_qplus +=
		    Surf[n_radial] * data[t_data::QPLUS](n_radial, n_azimuthal);
	    }
	}
    }

    MPI_Reduce(&local_qplus, &global_qplus, 1, MPI_DOUBLE, MPI_SUM, 0,
	       MPI_COMM_WORLD);

    return global_qplus;
}

double gas_luminosity(t_data &data, const double quantitiy_radius)
{
    double local_qminus = 0.0;
    double global_qminus = 0.0;

    for (unsigned int n_radial = radial_first_active;
	 n_radial < radial_active_size; ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::QMINUS].get_size_azimuthal();
	     ++n_azimuthal) {
	    if (Rmed[n_radial] <= quantitiy_radius) {
		local_qminus += Surf[n_radial] *
				data[t_data::QMINUS](n_radial, n_azimuthal);
	    }
	}
    }

    MPI_Reduce(&local_qminus, &global_qminus, 1, MPI_DOUBLE, MPI_SUM, 0,
	       MPI_COMM_WORLD);

    return global_qminus;
}

/**
	Calculates gas kinematic energy
*/
double gas_kinematic_energy(t_data &data, const double quantitiy_radius)
{
    double local_kinematic_energy = 0.0;
    double global_kinematic_energy = 0.0;

    double v_radial_center, v_azimuthal_center;

    for (unsigned int n_radial = radial_first_active;
	 n_radial < radial_active_size; ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::DENSITY].get_size_azimuthal();
	     ++n_azimuthal) {
	    // centered-in-cell radial velocity
	    if (Rmed[n_radial] <= quantitiy_radius) {
		v_radial_center =
		    (Rmed[n_radial] - Rinf[n_radial]) *
			data[t_data::V_RADIAL](n_radial + 1, n_azimuthal) +
		    (Rsup[n_radial] - Rmed[n_radial]) *
			data[t_data::V_RADIAL](n_radial, n_azimuthal);
		v_radial_center /= (Rsup[n_radial] - Rinf[n_radial]);

		// centered-in-cell azimuthal velocity
		v_azimuthal_center =
		    0.5 *
			(data[t_data::V_AZIMUTHAL](n_radial, n_azimuthal) +
			 data[t_data::V_AZIMUTHAL](
			     n_radial, n_azimuthal == data[t_data::V_AZIMUTHAL]
							  .get_max_azimuthal()
					   ? 0
					   : n_azimuthal + 1)) +
		    Rmed[n_radial] * OmegaFrame;

		local_kinematic_energy +=
		    0.5 * Surf[n_radial] *
		    data[t_data::DENSITY](n_radial, n_azimuthal) *
		    (std::pow(v_radial_center, 2) +
		     std::pow(v_azimuthal_center, 2));
	    }
	}
    }

    MPI_Reduce(&local_kinematic_energy, &global_kinematic_energy, 1, MPI_DOUBLE,
	       MPI_SUM, 0, MPI_COMM_WORLD);

    return global_kinematic_energy;
}

/**
	Calculates gas kinematic energy
*/
double gas_radial_kinematic_energy(t_data &data, const double quantitiy_radius)
{
    double local_kinematic_energy = 0.0;
    double global_kinematic_energy = 0.0;

    double v_radial_center;

    for (unsigned int n_radial = radial_first_active;
	 n_radial < radial_active_size; ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::DENSITY].get_size_azimuthal();
	     ++n_azimuthal) {
	    if (Rmed[n_radial] <= quantitiy_radius) {
		// centered-in-cell radial velocity
		v_radial_center =
		    (Rmed[n_radial] - Rinf[n_radial]) *
			data[t_data::V_RADIAL](n_radial + 1, n_azimuthal) +
		    (Rsup[n_radial] - Rmed[n_radial]) *
			data[t_data::V_RADIAL](n_radial, n_azimuthal);
		v_radial_center /= (Rsup[n_radial] - Rinf[n_radial]);

		local_kinematic_energy +=
		    0.5 * Surf[n_radial] *
		    data[t_data::DENSITY](n_radial, n_azimuthal) *
		    std::pow(v_radial_center, 2);
	    }
	}
    }

    MPI_Reduce(&local_kinematic_energy, &global_kinematic_energy, 1, MPI_DOUBLE,
	       MPI_SUM, 0, MPI_COMM_WORLD);

    return global_kinematic_energy;
}

/**
	Calculates gas kinematic energy
*/
double gas_azimuthal_kinematic_energy(t_data &data,
				      const double quantitiy_radius)
{
    double local_kinematic_energy = 0.0;
    double global_kinematic_energy = 0.0;

    double v_azimuthal_center;

    for (unsigned int n_radial = radial_first_active;
	 n_radial < radial_active_size; ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::DENSITY].get_size_azimuthal();
	     ++n_azimuthal) {
	    if (Rmed[n_radial] <= quantitiy_radius) {
		// centered-in-cell azimuthal velocity
		v_azimuthal_center =
		    0.5 *
			(data[t_data::V_AZIMUTHAL](n_radial, n_azimuthal) +
			 data[t_data::V_AZIMUTHAL](
			     n_radial, n_azimuthal == data[t_data::V_AZIMUTHAL]
							  .get_max_azimuthal()
					   ? 0
					   : n_azimuthal + 1)) +
		    Rmed[n_radial] * OmegaFrame;

		local_kinematic_energy +=
		    0.5 * Surf[n_radial] *
		    data[t_data::DENSITY](n_radial, n_azimuthal) *
		    std::pow(v_azimuthal_center, 2);
	    }
	}
    }

    MPI_Reduce(&local_kinematic_energy, &global_kinematic_energy, 1, MPI_DOUBLE,
	       MPI_SUM, 0, MPI_COMM_WORLD);

    return global_kinematic_energy;
}

/**
	Calculates gas gravitational energy
*/
double gas_gravitational_energy(t_data &data, const double quantitiy_radius)
{
    double local_gravitational_energy = 0.0;
    double global_gravitational_energy = 0.0;

    for (unsigned int n_radial = radial_first_active;
	 n_radial < radial_active_size; ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::DENSITY].get_size_azimuthal();
	     ++n_azimuthal) {
	    if (Rmed[n_radial] <= quantitiy_radius) {
		local_gravitational_energy +=
		    -Surf[n_radial] *
		    data[t_data::DENSITY](n_radial, n_azimuthal) *
		    data[t_data::POTENTIAL](n_radial, n_azimuthal);
	    }
	}
    }

    MPI_Reduce(&local_gravitational_energy, &global_gravitational_energy, 1,
	       MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    return global_gravitational_energy;
}

static void calculate_disk_ecc_peri_nbody_center(t_data &data, unsigned int timestep,
				   bool force_update)
{
	static int last_timestep_calculated = -1;
	double angle, r_x, r_y, j;
	double v_xmed, v_ymed;
	double e_x, e_y;
	double total_mass = 0.0;

	const double num_nbody = data.get_planetary_system().get_number_of_planets();
	const Pair cms_pos = data.get_planetary_system().get_center_of_mass(num_nbody);
	const Pair cms_vel = data.get_planetary_system().get_center_of_mass_velocity(num_nbody);

	if (!force_update) {
	if (last_timestep_calculated == (int)timestep) {
		return;
	} else {
		last_timestep_calculated = timestep;
	}
	}
	// calculations outside the loop for speedup
	double sinFrameAngle = std::sin(FrameAngle);
	double cosFrameAngle = std::cos(FrameAngle);
	for (unsigned int n_radial = 0;
	 n_radial < data[t_data::DENSITY].get_size_radial(); ++n_radial) {
	for (unsigned int n_azimuthal = 0;
		 n_azimuthal < data[t_data::DENSITY].get_size_azimuthal();
		 ++n_azimuthal) {
		total_mass =
		hydro_center_mass +
		data[t_data::DENSITY](n_radial, n_azimuthal) * Surf[n_radial];

		// location of the cell
		angle = (double)n_azimuthal /
			(double)data[t_data::V_RADIAL].get_size_azimuthal() * 2.0 *
			M_PI;
		r_x = Rmed[n_radial] * std::cos(angle) - cms_pos.x;
		r_y = Rmed[n_radial] * std::sin(angle) - cms_pos.y;

		// averaged velocities
		v_xmed =
		std::cos(angle) * 0.5 *
			(data[t_data::V_RADIAL](n_radial, n_azimuthal) +
			 data[t_data::V_RADIAL](n_radial + 1, n_azimuthal)) -
		std::sin(angle) *
			(0.5 *
			 (data[t_data::V_AZIMUTHAL](n_radial, n_azimuthal) +
			  data[t_data::V_AZIMUTHAL](
				  n_radial, n_azimuthal == data[t_data::V_AZIMUTHAL]
							   .get_max_azimuthal()
						? 0
						: n_azimuthal + 1)) +
			 OmegaFrame * Rmed[n_radial]) - cms_vel.x;
		v_ymed =
		std::sin(angle) * 0.5 *
			(data[t_data::V_RADIAL](n_radial, n_azimuthal) +
			 data[t_data::V_RADIAL](n_radial + 1, n_azimuthal)) +
		std::cos(angle) *
			(0.5 *
			 (data[t_data::V_AZIMUTHAL](n_radial, n_azimuthal) +
			  data[t_data::V_AZIMUTHAL](
				  n_radial, n_azimuthal == data[t_data::V_AZIMUTHAL]
							   .get_max_azimuthal()
						? 0
						: n_azimuthal + 1)) +
			 OmegaFrame * Rmed[n_radial]) - cms_vel.y;

		// specific angular momentum for each cell j = j*e_z
		j = r_x * v_ymed - r_y * v_xmed;
		// Runge-Lenz vector Ax = x*vy*vy-y*vx*vy-G*m*x/d;
		e_x =
		j * v_ymed / (constants::G * total_mass) - r_x / Rmed[n_radial];
		e_y = -1.0 * j * v_xmed / (constants::G * total_mass) -
		  r_y / Rmed[n_radial];

		data[t_data::ECCENTRICITY](n_radial, n_azimuthal) =
		std::sqrt(std::pow(e_x, 2) + std::pow(e_y, 2));

		if (FrameAngle != 0.0) {
		// periastron grid is rotated to non-rotating coordinate system
		// to prevent phase jumps of atan2 in later transformations like
		// you would have had if you back-transform the output
		// periastron values
		data[t_data::PERIASTRON](n_radial, n_azimuthal) =
			std::atan2(e_y * cosFrameAngle + e_x * sinFrameAngle,
				   e_x * cosFrameAngle - e_y * sinFrameAngle);
		} else {
		data[t_data::PERIASTRON](n_radial, n_azimuthal) =
			std::atan2(e_y, e_x);
		}
	}
	}
}

void calculate_disk_delta_ecc_peri(t_data &data, t_polargrid &dEcc, t_polargrid &dPer)
{

	t_polargrid &sigma = data[t_data::DENSITY];

	// ecc holds the current eccentricity
	t_polargrid &ecc = data[t_data::ECCENTRICITY];
	t_polargrid &peri = data[t_data::PERIASTRON];

	t_polargrid &ecc_tmp = data[t_data::ECCENTRICITY_PING_PONG];
	t_polargrid &peri_tmp = data[t_data::PERIASTRON_PING_PONG];

	// store data ecc in ecc_tmp
	move_polargrid(ecc_tmp, ecc);
	move_polargrid(peri_tmp, peri);

	// compute new eccentricity into ecc
	calculate_disk_ecc_peri(data, 0, true);

	// normalize by mass
	const double mass = quantities::gas_total_mass(data, 2.0*RMAX);

	for (unsigned int nr = 0;	 nr < ecc.get_size_radial(); ++nr) {
	for (unsigned int naz = 0; naz < ecc.get_size_azimuthal(); ++naz) {

		dEcc(nr, naz) += (ecc(nr, naz) - ecc_tmp(nr, naz)) * sigma(nr, naz) * Surf[nr] / mass;
		dPer(nr, naz) += (peri(nr, naz) - peri_tmp(nr, naz)) * sigma(nr, naz) * Surf[nr] / mass;

	}
	}

}

static void calculate_disk_ecc_peri_hydro_center(t_data &data, unsigned int timestep,
			       bool force_update)
{
    static int last_timestep_calculated = -1;
    double angle, r_x, r_y, j;
    double v_xmed, v_ymed;
    double e_x, e_y;
    double total_mass = 0.0;

	const Pair cms_pos = data.get_planetary_system().get_hydro_frame_center_position();
	const Pair cms_vel = data.get_planetary_system().get_hydro_frame_center_velocity();


    if (!force_update) {
	if (last_timestep_calculated == (int)timestep) {
	    return;
	} else {
	    last_timestep_calculated = timestep;
	}
    }
    // calculations outside the loop for speedup
    double sinFrameAngle = std::sin(FrameAngle);
    double cosFrameAngle = std::cos(FrameAngle);
    for (unsigned int n_radial = 0;
	 n_radial < data[t_data::DENSITY].get_size_radial(); ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::DENSITY].get_size_azimuthal();
	     ++n_azimuthal) {
	    total_mass =
		hydro_center_mass +
		data[t_data::DENSITY](n_radial, n_azimuthal) * Surf[n_radial];

	    // location of the cell
	    angle = (double)n_azimuthal /
		    (double)data[t_data::V_RADIAL].get_size_azimuthal() * 2.0 *
		    M_PI;
		r_x = Rmed[n_radial] * std::cos(angle) - cms_pos.x;
		r_y = Rmed[n_radial] * std::sin(angle) - cms_pos.y;

	    // averaged velocities
	    v_xmed =
		std::cos(angle) * 0.5 *
		    (data[t_data::V_RADIAL](n_radial, n_azimuthal) +
		     data[t_data::V_RADIAL](n_radial + 1, n_azimuthal)) -
		std::sin(angle) *
		    (0.5 *
			 (data[t_data::V_AZIMUTHAL](n_radial, n_azimuthal) +
			  data[t_data::V_AZIMUTHAL](
			      n_radial, n_azimuthal == data[t_data::V_AZIMUTHAL]
							   .get_max_azimuthal()
					    ? 0
					    : n_azimuthal + 1)) +
			 OmegaFrame * Rmed[n_radial]) - cms_vel.x;
	    v_ymed =
		std::sin(angle) * 0.5 *
		    (data[t_data::V_RADIAL](n_radial, n_azimuthal) +
		     data[t_data::V_RADIAL](n_radial + 1, n_azimuthal)) +
		std::cos(angle) *
		    (0.5 *
			 (data[t_data::V_AZIMUTHAL](n_radial, n_azimuthal) +
			  data[t_data::V_AZIMUTHAL](
			      n_radial, n_azimuthal == data[t_data::V_AZIMUTHAL]
							   .get_max_azimuthal()
					    ? 0
					    : n_azimuthal + 1)) +
			 OmegaFrame * Rmed[n_radial]) - cms_vel.y;

	    // specific angular momentum for each cell j = j*e_z
	    j = r_x * v_ymed - r_y * v_xmed;
	    // Runge-Lenz vector Ax = x*vy*vy-y*vx*vy-G*m*x/d;
	    e_x =
		j * v_ymed / (constants::G * total_mass) - r_x / Rmed[n_radial];
	    e_y = -1.0 * j * v_xmed / (constants::G * total_mass) -
		  r_y / Rmed[n_radial];

	    data[t_data::ECCENTRICITY](n_radial, n_azimuthal) =
		std::sqrt(std::pow(e_x, 2) + std::pow(e_y, 2));

	    if (FrameAngle != 0.0) {
		// periastron grid is rotated to non-rotating coordinate system
		// to prevent phase jumps of atan2 in later transformations like
		// you would have had if you back-transform the output
		// periastron values
		data[t_data::PERIASTRON](n_radial, n_azimuthal) =
		    std::atan2(e_y * cosFrameAngle + e_x * sinFrameAngle,
			       e_x * cosFrameAngle - e_y * sinFrameAngle);
	    } else {
		data[t_data::PERIASTRON](n_radial, n_azimuthal) =
		    std::atan2(e_y, e_x);
	    }
	}
    }
}

void calculate_disk_ecc_peri(t_data &data, unsigned int timestep,
				   bool force_update){
	if(parameters::n_bodies_for_hydroframe_center == 1){
		if(data.get_planetary_system().get_number_of_planets() > 1){
			// Binary has effects out to ~ 15 abin, if that is not inside the domain, compute ecc around primary
			if(data.get_planetary_system().get_planet(1).get_semi_major_axis() < RMAX*0.1){
				calculate_disk_ecc_peri_hydro_center(data, timestep, force_update);
			} else {
				calculate_disk_ecc_peri_nbody_center(data, timestep, force_update);
			}
		} else {
			// We only have a star, compute ecc around primary
			calculate_disk_ecc_peri_hydro_center(data, timestep, force_update);
		}
	} else {
		// If we have multiple objects as hydro center, always compute eccentricity around hydro center
		calculate_disk_ecc_peri_hydro_center(data, timestep, force_update);
	}
}

void state_disk_ecc_peri_calculation_center(t_data &data){
	if(parameters::n_bodies_for_hydroframe_center == 1){
		if(data.get_planetary_system().get_number_of_planets() > 1){
			// Binary has effects out to ~ 15 abin, if that is not inside the domain, compute ecc around primary
			if(data.get_planetary_system().get_planet(1).get_semi_major_axis() < RMAX*0.1){
				printf("%.5e < %.5e\n", data.get_planetary_system().get_planet(1).get_semi_major_axis(), RMAX*0.1);
				logging::print_master(LOG_INFO "Computing eccentricity / pericenter with respect to the hydro frame (primary) center!\n");
			} else {
				printf("%.5e > %.5e\n", data.get_planetary_system().get_planet(1).get_semi_major_axis(), RMAX*0.1);
				logging::print_master(LOG_INFO "Computing eccentricity / pericenter with respect to the center of mass of the Nbody system!\n");
			}
		} else {
			// We only have a star, compute ecc around primary
			logging::print_master(LOG_INFO "Computing eccentricity / pericenter with respect to the hydro frame (primary) center!\n");
		}
	} else {
		// If we have multiple objects as hydro center, always compute eccentricity around hydro center
		logging::print_master(LOG_INFO "Computing eccentricity / pericenter with respect to the hydro frame (Nbody) center!\n");
	}
}

/**
	compute alpha gravitational

	alpha(R) = |d ln Omega/d ln R|^-1 (Tgrav)/(Sigma cs^2)
*/
void calculate_alpha_grav(t_data &data, unsigned int timestep,
			  bool force_update)
{
    static int last_timestep_calculated = -1;

    if (parameters::self_gravity != true)
	return;

    if (!force_update) {
	if (last_timestep_calculated == (int)timestep) {
	    return;
	} else {
	    last_timestep_calculated = timestep;
	}
    }

    stress::calculate_gravitational_stress(data);

    for (unsigned int n_radial = 0;
	 n_radial < data[t_data::ALPHA_GRAV].get_size_radial(); ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::ALPHA_GRAV].get_size_azimuthal();
	     ++n_azimuthal) {
	    /*data[t_data::ALPHA_GRAV](n_radial, n_azimuthal) = -2.0/3.0 *
	     * (data[t_data::T_GRAVITATIONAL](n_radial,n_azimuthal)+data[t_data::T_REYNOLDS](n_radial,
	     * n_azimuthal))/(data[t_data::DENSITY](n_radial,n_azimuthal)*pow2(data[t_data::SOUNDSPEED](n_radial,
	     * n_azimuthal)));*/
	    data[t_data::ALPHA_GRAV](n_radial, n_azimuthal) =
		2.0 / 3.0 *
		data[t_data::T_GRAVITATIONAL](n_radial, n_azimuthal) /
		(data[t_data::DENSITY](n_radial, n_azimuthal) *
		 std::pow(data[t_data::SOUNDSPEED](n_radial, n_azimuthal), 2));
	}
    }
}

void calculate_alpha_grav_mean_sumup(t_data &data, unsigned int timestep,
				     double dt)
{
    calculate_alpha_grav(data, timestep, true);

    for (unsigned int n_radial = 0;
	 n_radial < data[t_data::ALPHA_GRAV_MEAN].get_size_radial();
	 ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::ALPHA_GRAV_MEAN].get_size_azimuthal();
	     ++n_azimuthal) {
	    data[t_data::ALPHA_GRAV_MEAN](n_radial, n_azimuthal) +=
		data[t_data::ALPHA_GRAV](n_radial, n_azimuthal) * dt;
	}
    }
}

/**
	compute alpha Reynolds

	alpha(R) = |d ln Omega/d ln R|^-1 (Trey)/(Sigma cs^2)
*/
void calculate_alpha_reynolds(t_data &data, unsigned int timestep,
			      bool force_update)
{
    static int last_timestep_calculated = -1;

    stress::calculate_Reynolds_stress(data);

    if (!force_update) {
	if (last_timestep_calculated == (int)timestep) {
	    return;
	} else {
	    last_timestep_calculated = timestep;
	}
    }

    for (unsigned int n_radial = 0;
	 n_radial < data[t_data::ALPHA_REYNOLDS].get_size_radial();
	 ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::ALPHA_REYNOLDS].get_size_azimuthal();
	     ++n_azimuthal) {
	    data[t_data::ALPHA_REYNOLDS](n_radial, n_azimuthal) =
		2.0 / 3.0 * data[t_data::T_REYNOLDS](n_radial, n_azimuthal) /
		(data[t_data::DENSITY](n_radial, n_azimuthal) *
		 std::pow(data[t_data::SOUNDSPEED](n_radial, n_azimuthal), 2));
	}
    }
}

void calculate_alpha_reynolds_mean_sumup(t_data &data, unsigned int timestep,
					 double dt)
{
    calculate_alpha_reynolds(data, timestep, true);

    for (unsigned int n_radial = 0;
	 n_radial < data[t_data::ALPHA_REYNOLDS_MEAN].get_size_radial();
	 ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal <
	     data[t_data::ALPHA_REYNOLDS_MEAN].get_size_azimuthal();
	     ++n_azimuthal) {
	    data[t_data::ALPHA_REYNOLDS_MEAN](n_radial, n_azimuthal) +=
		data[t_data::ALPHA_REYNOLDS](n_radial, n_azimuthal) * dt;
	}
    }
}

/**
	Calculates Toomre Q parameter
*/
void calculate_toomre(t_data &data, unsigned int /* timestep */,
		      bool /* force_update */)
{
    double kappa;

    for (unsigned int n_radial = 1;
	 n_radial < data[t_data::TOOMRE].get_size_radial(); ++n_radial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::TOOMRE].get_size_azimuthal();
	     ++n_azimuthal) {
	    // kappa^2 = 1/r^3 d((r^2 Omega)^2)/dr = 1/r^3 d((r*v_phi)^2)/dr
	    kappa = std::sqrt(std::fabs(
		std::pow(InvRmed[n_radial], 3) *
		(std::pow(data[t_data::V_AZIMUTHAL](n_radial, n_azimuthal) *
			      Rmed[n_radial],
			  2) -
		 std::pow(data[t_data::V_AZIMUTHAL](n_radial - 1, n_azimuthal) *
			      Rmed[n_radial - 1],
			  2)) *
		InvDiffRmed[n_radial]));

	    // Q = (c_s kappa) / (Pi G Sigma)
	    // data[t_data::TOOMRE](n_radial, n_azimuthal) =
	    // data[t_data::SOUNDSPEED](n_radial,
	    // n_azimuthal)*calculate_omega_kepler(Rmed[n_radial])/(PI*G*data[t_data::DENSITY](n_radial,
	    // n_azimuthal));
	    data[t_data::TOOMRE](n_radial, n_azimuthal) =
		data[t_data::SOUNDSPEED](n_radial, n_azimuthal) * kappa /
		(M_PI * constants::G *
		 data[t_data::DENSITY](n_radial, n_azimuthal));
	}
    }
}

void calculate_radial_luminosity(t_data &data, unsigned int timestep,
				 bool force_update)
{
    static int last_timestep_calculated = -1;

    if ((!force_update) && (last_timestep_calculated == (int)timestep)) {
	return;
    }

    last_timestep_calculated = timestep;

    for (unsigned int n_radial = 0;
	 n_radial < data[t_data::LUMINOSITY_1D].get_size_radial(); ++n_radial) {
	// L = int( int(sigma T^4 r ,phi) ,r);
	data[t_data::LUMINOSITY_1D](n_radial) = 0.0;

	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::QMINUS].get_size_azimuthal();
	     ++n_azimuthal) {
	    double dr = (Rsup[n_radial] - Rinf[n_radial]);
	    data[t_data::LUMINOSITY_1D](n_radial) +=
		data[t_data::QMINUS](n_radial, n_azimuthal) * Rmed[n_radial] *
		dr * dphi;
	}
    }
}

void calculate_radial_dissipation(t_data &data, unsigned int timestep,
				  bool force_update)
{
    static int last_timestep_calculated = -1;

    if ((!force_update) && (last_timestep_calculated == (int)timestep)) {
	return;
    }

    last_timestep_calculated = timestep;

    for (unsigned int n_radial = 0;
	 n_radial < data[t_data::DISSIPATION_1D].get_size_radial();
	 ++n_radial) {
	data[t_data::DISSIPATION_1D](n_radial) = 0.0;

	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::QPLUS].get_size_azimuthal();
	     ++n_azimuthal) {
	    double dr = (Rsup[n_radial] - Rinf[n_radial]);

	    data[t_data::DISSIPATION_1D](n_radial) +=
		data[t_data::QPLUS](n_radial, n_azimuthal) * Rmed[n_radial] *
		dr * dphi;
	}
    }
}

void calculate_massflow(t_data &data, unsigned int timestep, bool force_update)
{
    (void)timestep;
    (void)force_update;

    double denom;
    denom = NINTERM * DT;

    // divide the data in massflow by the large timestep DT before writing out
    // to obtain the massflow from the mass difference
    for (unsigned int nRadial = 0;
	 nRadial < data[t_data::MASSFLOW].get_size_radial(); ++nRadial) {
	for (unsigned int n_azimuthal = 0;
	     n_azimuthal < data[t_data::MASSFLOW].get_size_azimuthal();
	     ++n_azimuthal) {
	    data[t_data::MASSFLOW](nRadial, n_azimuthal) *= 1. / denom;
	}
    }
}


void compute_aspectratio(t_data &data, unsigned int timestep, bool force_update)
{
    static int last_timestep_calculated = -1;

    if ((!force_update) && (last_timestep_calculated == (int)timestep)) {
	return;
    }

    switch (ASPECTRATIO_MODE) {
    case 0: {

			for (unsigned int n_radial = 0;
				 n_radial <= data[t_data::SCALE_HEIGHT].get_max_radial(); ++n_radial) {
				for (unsigned int n_azimuthal = 0;
					 n_azimuthal <= data[t_data::SCALE_HEIGHT].get_max_azimuthal();
					 ++n_azimuthal) {
					const double h = data[t_data::SCALE_HEIGHT](n_radial, n_azimuthal) / Rb[n_radial];
					data[t_data::ASPECTRATIO](n_radial, n_azimuthal) = h;
				}
			}

	break;
    }
    case 1: {

			static const unsigned int N_planets =
					data.get_planetary_system().get_number_of_planets();
			static std::vector<double> xpl(N_planets);
			static std::vector<double> ypl(N_planets);
			static std::vector<double> mpl(N_planets);
			static std::vector<double> rpl(N_planets);

			// setup planet data
			for (unsigned int k = 0; k < N_planets; k++) {
				const t_planet &planet = data.get_planetary_system().get_planet(k);
				mpl[k] = planet.get_rampup_mass(PhysicalTime);
				xpl[k] = planet.get_x();
				ypl[k] = planet.get_y();
				rpl[k] = planet.get_planet_radial_extend();
			}

			// h = H/r
			// H = = c_s,iso / (GM/r^3) = c_s/sqrt(gamma) / / (GM/r^3)
			// for an Nbody system, H^-2 = sum_n (H_n)^-2
			// See Günter & Kley 2003 Eq. 8, but beware of wrong extra square.
			// Better see Thun et al. 2017 Eq. 8 instead.
			for (unsigned int n_rad = 0;
				 n_rad <= data[t_data::SCALE_HEIGHT].get_max_radial(); ++n_rad) {
				for (unsigned int n_az = 0;
					 n_az <= data[t_data::SCALE_HEIGHT].get_max_azimuthal(); ++n_az) {

					const int cell = get_cell_id(n_rad, n_az);
					const double x = CellCenterX->Field[cell];
					const double y = CellCenterY->Field[cell];
					const double cs2 =
							std::pow(data[t_data::SOUNDSPEED](n_rad, n_az), 2);

					double inv_h2 = 0.0; // inverse aspectratio squared

					for (unsigned int k = 0; k < N_planets; k++) {

						/// since the mass is distributed homogeniously distributed
						/// inside the cell, we assume that the planet is always at
						/// least cell_size / 2 plus planet radius away from the gas
						/// this is an rough estimate without explanation
						/// alternatively you can think about it yourself
						const double min_dist =
								0.5 * std::max(Rsup[n_rad] - Rinf[n_rad],
											   Rmed[n_rad] * dphi) +
								rpl[k];

						const double dx = x - xpl[k];
						const double dy = y - ypl[k];

						const double dist = std::max(
									std::sqrt(std::pow(dx, 2) + std::pow(dy, 2)), min_dist);

						// H^2 = (GM / dist^3 / Cs_iso^2)^-1
						if (parameters::Adiabatic || parameters::Polytropic) {
							const double gamma1 = pvte::get_gamma1(data, n_rad, n_az);

								const double tmp_inv_h2 =
										constants::G * mpl[k] * gamma1 / (dist * cs2);
								inv_h2 += tmp_inv_h2;

						} else {

								const double tmp_inv_h2 =
										constants::G * mpl[k] / (dist * cs2);
								inv_h2 += tmp_inv_h2;
						}
					}

						const double h = std::sqrt(1.0 / inv_h2);
						data[t_data::ASPECTRATIO](n_rad, n_az) = h;
				}
			}

	break;
    }
    case 2: {

		const Pair r_cm = data.get_planetary_system().get_center_of_mass();
		const double m_cm = data.get_planetary_system().get_mass();

		for (unsigned int n_rad = 0;
		 n_rad <= data[t_data::SCALE_HEIGHT].get_max_radial(); ++n_rad) {
		for (unsigned int n_az = 0;
			 n_az <= data[t_data::SCALE_HEIGHT].get_max_azimuthal(); ++n_az) {

			const int cell = get_cell_id(n_rad, n_az);
			const double x = CellCenterX->Field[cell];
			const double y = CellCenterY->Field[cell];
			const double cs = data[t_data::SOUNDSPEED](n_rad, n_az);

			// const double min_dist =
			//	0.5 * std::max(Rsup[n_rad] - Rinf[n_rad],
			//		   Rmed[n_rad] * dphi);

			const double dx = x - r_cm.x;
			const double dy = y - r_cm.y;

			// const double dist = std::max(
			//	std::sqrt(std::pow(dx, 2) + std::pow(dy, 2)), min_dist);
			const double dist = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));

			// h^2 = Cs_iso / vk = (Cs_iso^2 / (GM / dist))
			// H^2 = Cs_iso / Omegak = (Cs_iso^2 / (GM / dist^3))
			// H = h * dist
			if (parameters::Adiabatic || parameters::Polytropic) {
			/// Convert sound speed to isothermal sound speed cs,iso = cs /
			/// sqrt(gamma)
			const double gamma1 = pvte::get_gamma1(data, n_rad, n_az);
			const double h = cs * std::sqrt(dist / (constants::G * m_cm * gamma1));

			if(parameters::heating_star_enabled || parameters::self_gravity){
			data[t_data::ASPECTRATIO](n_rad, n_az) = h;
			}
			const double H = dist * h;
			data[t_data::SCALE_HEIGHT](n_rad, n_az) = H;

			} else { // locally isothermal
			const double h = cs * std::sqrt(dist / (constants::G * m_cm));
			if(parameters::heating_star_enabled || parameters::self_gravity){
			data[t_data::ASPECTRATIO](n_rad, n_az) = h;
			}
			const double H = dist * h;
			data[t_data::SCALE_HEIGHT](n_rad, n_az) = H;
			}
		}
	}
	break;
    }
    default: {
	for (unsigned int nRad = 0;
	     nRad < data[t_data::ASPECTRATIO].get_size_radial(); ++nRad) {
	    for (unsigned int nAz = 0;
		 nAz < data[t_data::ASPECTRATIO].get_size_azimuthal(); ++nAz) {
		data[t_data::ASPECTRATIO](nRad, nAz) =
		    data[t_data::SCALE_HEIGHT](nRad, nAz) / Rmed[nRad];
	    }
	}
    }
	}
}

void calculate_viscous_torque(t_data &data, unsigned int timestep,
			      bool force_update)
{
    (void)timestep;
    (void)force_update;

    double denom;

    if (!parameters::write_at_every_timestep) {
	denom = (double)NINTERM;
	// divide the data in massflow by the large timestep DT before writing
	// out to obtain the massflow from the mass difference
	for (unsigned int nRadial = 0;
	     nRadial < data[t_data::VISCOUS_TORQUE].get_size_radial();
	     ++nRadial) {
	    for (unsigned int nAzimuthal = 0;
		 nAzimuthal < data[t_data::VISCOUS_TORQUE].get_size_azimuthal();
		 ++nAzimuthal) {
		data[t_data::VISCOUS_TORQUE](nRadial, nAzimuthal) *= 1. / denom;
	    }
	}
    }
}

void calculate_gravitational_torque(t_data &data, unsigned int timestep,
				    bool force_update)
{
    (void)timestep;
    (void)force_update;

    double denom;

    if (!parameters::write_at_every_timestep) {
	denom = (double)NINTERM;
	// divide the data in massflow by the large timestep DT before writing
	// out to obtain the massflow from the mass difference
	for (unsigned int nRadial = 0;
	     nRadial < data[t_data::GRAVITATIONAL_TORQUE_NOT_INTEGRATED]
			   .get_size_radial();
	     ++nRadial) {
	    for (unsigned int nAzimuthal = 0;
		 nAzimuthal < data[t_data::GRAVITATIONAL_TORQUE_NOT_INTEGRATED]
				  .get_size_azimuthal();
		 ++nAzimuthal) {
		data[t_data::GRAVITATIONAL_TORQUE_NOT_INTEGRATED](
		    nRadial, nAzimuthal) *= 1. / denom;
	    }
	}
    }
}

void calculate_advection_torque(t_data &data, unsigned int timestep,
				bool force_update)
{
    (void)timestep;
    (void)force_update;

    double denom;

    if (!parameters::write_at_every_timestep) {
	denom = (double)NINTERM;
	// divide the data in massflow by the large timestep DT before writing
	// out to obtain the massflow from the mass difference
	for (unsigned int nRadial = 0;
	     nRadial < data[t_data::ADVECTION_TORQUE].get_size_radial();
	     ++nRadial) {
	    for (unsigned int nAzimuthal = 0;
		 nAzimuthal <
		 data[t_data::ADVECTION_TORQUE].get_size_azimuthal();
		 ++nAzimuthal) {
		data[t_data::ADVECTION_TORQUE](nRadial, nAzimuthal) *=
		    1. / denom;
	    }
	}
    }
}

} // namespace quantities
