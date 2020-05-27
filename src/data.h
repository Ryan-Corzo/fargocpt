#ifndef DATA_H
#define DATA_H

#include <stddef.h>
#include <stdlib.h>

#include "planetary_system.h"
#include "polargrid.h"
#include "radialgrid.h"

class t_data
{
  public:
    t_data();
    ~t_data();

    enum t_polargrid_type {
	DENSITY,	     // surface density
	V_RADIAL,	     // radial velocity
	V_AZIMUTHAL,	     // azimuthal velocity
	ENERGY,		     // energy
	TEMPERATURE,	     // temperature
	PRESSURE,	     // pressure
	SOUNDSPEED,	     // soundspeed c_s (introduced by Fargo-ADSG)
	TOOMRE,		     // Toomre parameter Q
	ECCENTRICITY,	     // disk eccentricity
	PERIASTRON,	     // disk periastron
	ALPHA_GRAV,	     // alpha grav
	ALPHA_GRAV_MEAN,     // alpha grav time average
	ALPHA_REYNOLDS,	     // alpha reynolds
	ALPHA_REYNOLDS_MEAN, // alpha reynolds time average
	V_RADIAL0,	     // radial velocity at beginning
	V_AZIMUTHAL0,	     // azimuthal velocity at beginning
	DENSITY0,	     // radial velocity at beginning
	ENERGY0,	     // azimuthal velocity at beginning
	KAPPA,		     // opacity
	TAU_COOL,	     // cooling time
	QPLUS,		     // heating terms
	QMINUS,		     // cooling terms
	P_DIVV,		     // pdV work
	VISCOSITY,	     // (kinematic) viscosity
	TAU_R_R,	     // viscous stress tensor r,r component
	TAU_R_PHI,	     // viscous stress tensor r,phi component
	TAU_PHI_PHI,	     // viscous stress tensor phi,phi component
	DIV_V,		     // divergence of velocity
	T_REYNOLDS,	     // Reynold Stress tensor
	T_GRAVITATIONAL,     // graviational Stress tensor
	POTENTIAL, // the gravitational potential felt by any fluid elements
		   // (introduced by Fargo-ADSG)
	V_RADIAL_SOURCETERMS,	 // v_radial with sourceterms (after substep 1)
	V_AZIMUTHAL_SOURCETERMS, // v_azimuthal with sourceterms (after substep
				 // 1)
	V_RADIAL_NEW,	 // new v_radial after all substeps
	V_AZIMUTHAL_NEW, // new v_azimuthal after all substeps
	ENERGY_NEW,	 //
	ENERGY_INT,	 //
	DENSITY_INT,	 //
	Q_R,		 //
	Q_PHI,		 //
	TAU,		 // optical depth
	TAU_EFF,	 // effective optical depth (in vertical direction)
	TAU2,
	ASPECTRATIO, // aspect ratio H/r
	VISIBILITY,  //
	TORQUE,
	RHO,

	// number of t_polargrid_types
	N_POLARGRID_TYPES
    };

    enum t_radialgrid_type {
	DENSITY_1D,
	V_AZIMUTHAL_1D,
	SOUNDSPEED_1D,
	TOOMRE_1D,
	T_REYNOLDS_1D,		// Reynold Stress tensor
	T_GRAVITATIONAL_1D,	// graviational Stress tensor
	ALPHA_GRAV_1D,		// alpha grav
	ALPHA_GRAV_MEAN_1D,	// alpha grav time average
	ALPHA_REYNOLDS_1D,	// alpha reynolds
	ALPHA_REYNOLDS_MEAN_1D, // alpha reynolds time average
	LUMINOSITY_1D,		//
	DISSIPATION_1D,
	TORQUE_1D,
	MASSFLOW_1D,

	// number of t_radialgrid_types
	N_RADIALGRID_TYPES
    };

    inline t_polargrid &operator[](t_polargrid_type polargrid_type)
    {
	return m_polargrids[polargrid_type];
    }
    inline t_radialgrid &operator[](t_radialgrid_type radialgrid_type)
    {
	return m_radialgrids[radialgrid_type];
    }

    void set_size(unsigned int global_n_radial,
		  unsigned int global_n_azimiuthal, unsigned int n_radial,
		  unsigned int n_azimuthal);
    inline t_planetary_system &get_planetary_system()
    {
	return m_planetary_system;
    }
    void print_memory_usage(unsigned int n_radial, unsigned int n_azimuthal);

    double pdivv_total;
    inline ptrdiff_t get_n_radial(void) { return m_n_radial; };
    inline ptrdiff_t get_n_azimuthal(void) { return m_n_azimuthal; };
    inline ptrdiff_t get_global_n_radial(void) { return m_global_n_radial; };
    inline ptrdiff_t get_global_n_azimuthal(void)
    {
	return m_global_n_azimuthal;
    };

  private:
    ptrdiff_t m_n_radial;
    ptrdiff_t m_n_azimuthal;
    ptrdiff_t m_global_n_radial;
    ptrdiff_t m_global_n_azimuthal;
    t_polargrid m_polargrids[N_POLARGRID_TYPES];
    t_radialgrid m_radialgrids[N_RADIALGRID_TYPES];
    t_planetary_system m_planetary_system;
};

#endif // DATA_H
