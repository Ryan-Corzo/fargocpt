/**
	\file boundary_conditions.cpp

	All boundary conditions are handled in this file.
*/
#ifdef _OPENMP
#include <omp.h>
#endif

#include "boundary_conditions.h"
#include "../global.h"
#include "../parameters.h"
#include "../simulation.h"
#include "../constants.h"


namespace boundary_conditions
{

/**
	set inner boundary values to initial values
 */
void initial_boundary_inner(t_data &data)
{
    if (CPU_Rank != 0){
		return;
	}
	if (sim::N_hydro_iter == 0) {
		return;
	}

    auto &Sigma = data[t_data::SIGMA];
    auto &Energy = data[t_data::ENERGY];
	auto &vr = data[t_data::V_RADIAL];

    auto &Sigma0 = data[t_data::SIGMA0];
    auto &Energy0 = data[t_data::ENERGY0];
	auto &vr0 = data[t_data::V_RADIAL0];

	const unsigned int Naz = Sigma.get_max_azimuthal();

	// printf("Sigma0 %e\n", Sigma0(0,0));

	#pragma omp parallel for
    for (unsigned int naz = 0; naz <= Naz; ++naz) {
		Sigma(0, naz) = Sigma0(0,naz);
		Energy(0, naz) = Energy0(0,naz);
		vr(0, naz) = vr0(0,naz);
		vr(1, naz) = vr0(1,naz);
	}
}

/**
	set outer boundary values to initial values
 */
void initial_boundary_outer(t_data &data)
{
    if (CPU_Rank != CPU_Highest){
		return;
	}
	if (sim::N_hydro_iter == 0) {
		return;
	}

    auto &sig = data[t_data::SIGMA];
    auto &e = data[t_data::ENERGY];
	auto &vr = data[t_data::V_RADIAL];

    auto &sig0 = data[t_data::SIGMA0];
    auto &e0 = data[t_data::ENERGY0];
	auto &vr0 = data[t_data::V_RADIAL0];

	const unsigned int Naz = sig.get_max_azimuthal();

	#pragma omp parallel for
    for (unsigned int naz = 0; naz <= Naz; ++naz) {
		const unsigned int nr = sig.get_max_radial();
		sig(nr, naz) = sig0(nr,naz);
		e(nr, naz) = e0(nr,naz);
		const unsigned int nrv = vr.get_max_radial();
		vr(nrv, naz) = vr0(nrv,naz);
		vr(nrv-1, naz) = vr0(nrv-1,naz);
	}
}




/**
	inner open boundary condition
*/
void open_boundary_inner(t_data &data)
{
    if (CPU_Rank != 0) {
		return;
	}
	
	t_polargrid &sig = data[t_data::SIGMA];
	t_polargrid &e = data[t_data::ENERGY];
	t_polargrid &vr = data[t_data::V_RADIAL];

	const unsigned int Naz = sig.get_max_azimuthal();

	#pragma omp parallel for
    for (unsigned int naz = 0; naz <= Naz; ++naz) {
		// copy first ring into ghost ring
		sig(0, naz) = sig(1, naz);
		e(0, naz) = e(1, naz);

		// set velocity to min(v[+1],0) (allow only outflow)
		if (vr(2, naz) > 0.0) {
			vr(1, naz) = 0.0;
			vr(0, naz) = 0.0;
		} else {
			vr(1, naz) = vr(2, naz);
			vr(0, naz) = vr(2, naz);
		}
    }
}


/**
	outer open boundary condition
 */
void open_boundary_outer(t_data &data)
{
    if (CPU_Rank != CPU_Highest) {
		return;
	}

	t_polargrid &sig = data[t_data::SIGMA];
	t_polargrid &e = data[t_data::ENERGY];
	t_polargrid &vr = data[t_data::V_RADIAL];

	const unsigned int Nrad = sig.get_max_radial();
	const unsigned int Nrad_v = vr.get_max_radial();
	const unsigned int Naz = sig.get_max_azimuthal();

	#pragma omp parallel for
    for (unsigned int naz = 0; naz <= Naz; ++naz) {
		// copy last ring into ghost ring
		sig(Nrad, naz) = sig(Nrad - 1, naz);
		e(Nrad, naz) = e(Nrad - 1, naz);

		// set velocity to min(v[+1],0) (allow only outflow)
		if (vr(Nrad_v - 2, naz) < 0.0) {
			vr(Nrad_v - 1, naz) = 0.0;
			vr(Nrad_v, naz) = 0.0;
		} else {
			vr(Nrad_v - 1, naz) = vr(Nrad_v - 2, naz);
			vr(Nrad_v, naz) = vr(Nrad_v - 2, naz);
		}
    }
}

/**
	inner reflecting boundary condition
*/
void reflecting_boundary_inner(t_data &data)
{
    if (CPU_Rank != 0) {
		return;
	}

	t_polargrid &sig = data[t_data::SIGMA];
	t_polargrid &e = data[t_data::ENERGY];
	t_polargrid &vr = data[t_data::V_RADIAL];

	const unsigned int Naz = sig.get_max_azimuthal();

	#pragma omp parallel for
	for (unsigned int naz = 0; naz <= Naz; ++naz) {
	    // copy first ring into ghost ring
	    sig(0, naz) = sig(1, naz);
	    e(0, naz) = e(1, naz);

	    vr(1, naz) = 0.0;
	    vr(0, naz) = -vr(2, naz);
	}
}

/**
	outer reflecting boundary condition
*/
void reflecting_boundary_outer(t_data &data)
{
    if (CPU_Rank != CPU_Highest) {
		return;
	}

	t_polargrid &sig = data[t_data::SIGMA];
	t_polargrid &e = data[t_data::ENERGY];
	t_polargrid &vr = data[t_data::V_RADIAL];

	const unsigned int Nrad = sig.get_max_radial();
	const unsigned int Nrad_v = vr.get_max_radial();
	const unsigned int Naz = sig.get_max_azimuthal();


	#pragma omp parallel for
	for (unsigned int naz = 0; naz <= Naz; ++naz) {
	    // copy last ring into ghost ring
	    sig(Nrad, naz) = sig(Nrad - 1, naz);
	    e(Nrad, naz) = e(Nrad - 1, naz);

	    vr(Nrad_v - 1, naz) = 0.0;
	    vr(Nrad_v, naz) = -vr(Nrad_v - 2, naz);
	}
}

void viscous_outflow_boundary_inner(t_data &data)
{
    if (CPU_Rank != 0) {
		return;
	}

	t_polargrid &sig = data[t_data::SIGMA];
	t_polargrid &e = data[t_data::ENERGY];
	t_polargrid &vr = data[t_data::V_RADIAL];
	t_polargrid &visc = data[t_data::VISCOSITY];

	const unsigned int Naz = sig.get_max_azimuthal();

	for (unsigned int naz = 0; naz <= Naz; ++naz) {
	    sig(0, naz) = sig(1, naz);
	    e(0, naz) = e(1, naz);
	}

	const double s = parameters::viscous_outflow_speed;

	#pragma omp parallel for
	for (unsigned int naz = 0; naz <= Naz; ++naz) {

	    const double Nu0 = visc(0, naz);
	    const double Nu1 = visc(1, naz);
	    const double Nu = 0.5 * (Nu0 + Nu1);

	    // V_rad =  - 1.5 / r * Nu (Kley, Papaloizou and Ogilvie, 2008)
	    vr(1, naz) = -1.5 * s / Rinf[1] * Nu;
	    vr(0, naz) = -1.5 * s / Rinf[0] * Nu;
	}
}


static double calc_sig(const double R) {
	const double sig0 = parameters::sigma0;
	const double sigexp = -parameters::SIGMASLOPE;
	return sig0 * std::pow(R, sigexp);
}

static double calc_e(const double R) {
	const double gamma = parameters::ADIABATICINDEX;
	const double sig0 = parameters::sigma0;
	const double sigexp = -parameters::SIGMASLOPE;
	const double f = parameters::FLARINGINDEX;
	const double h0 = parameters::ASPECTRATIO_REF;
	return 1.0 / (gamma - 1.0) * sig0 * std::pow(h0, 2) * std::pow(R, sigexp - 1.0 + 2.0 * f);
}

static double calc_T(const double e, const double sig) {
	const double gamma = parameters::ADIABATICINDEX;
	return e / sig * (gamma - 1.0) * parameters::MU * constants::R;
}

static double calc_vK(const double R) {
	return std::sqrt(constants::G * hydro_center_mass / R);
}

void keplerian2d_boundary_inner(t_data &data)
{

	auto &sig = data[t_data::SIGMA];
	auto &e = data[t_data::ENERGY];
	auto &T = data[t_data::TEMPERATURE];
	auto &vrad = data[t_data::V_RADIAL];
	auto &vaz = data[t_data::V_AZIMUTHAL];

	const unsigned int Naz = sig.get_max_azimuthal();

	#pragma omp parallel for
    for (unsigned int naz = 0; naz <= Naz; ++naz) {
		sig(0, naz) = calc_sig(Rmed[0]);

		e(0, naz) = calc_e(Rmed[0]);

		T(0, naz) = calc_T(e(0, naz), sig(0, naz));

		vrad(1, naz) = 0.0;
		vrad(0, naz) = -vrad(2, naz);

		vaz(0, naz) = calc_vK(Rmed[0]);
    }
}

void keplerian2d_boundary_outer(t_data &data)
{
	auto &sig = data[t_data::SIGMA];
	auto &e = data[t_data::ENERGY];
	auto &T = data[t_data::TEMPERATURE];
	auto &vrad = data[t_data::V_RADIAL];
	auto &vaz = data[t_data::V_AZIMUTHAL];

	const unsigned int Naz = sig.get_max_azimuthal();
	const unsigned int Nrad = sig.get_max_radial();
	const unsigned int Nrad_v = vrad.get_max_radial();

	#pragma omp parallel for
    for (unsigned int naz = 0; naz <= Naz; ++naz) {
		sig(Nrad, naz) = calc_sig(Rmed[Nrad]);

		e(Nrad, naz) = calc_e(Rmed[Nrad]);

		T(Nrad, naz) = calc_T(e(Nrad, naz), sig(Nrad, naz));

		vrad(Nrad_v, naz) = -vrad(Nrad_v - 2, naz);
		vrad(Nrad_v - 1, naz) = 0.0;

		vaz(Nrad, naz) = calc_vK(Rmed[Nrad]);
	}
}


} // namespace boundary_conditions
