#ifndef PARAMETERS_H
#define PARAMETERS_H

#include "data.h"

namespace parameters {

/// Type of radial Grid
enum t_radial_grid {
	arithmetic_spacing,
	logarithmic_spacing,
	exponential_spacing
};
extern t_radial_grid radial_grid_type;
extern const char* radial_grid_names[];

// boundary conditions
enum t_boundary_condition {boundary_condition_open, boundary_condition_reflecting, boundary_condition_nonreflecting, boundary_condition_evanescent, boundary_condition_viscous_outflow, boundary_condition_boundary_layer, boundary_condition_keplerian};

/// type of inner boundary
extern t_boundary_condition boundary_inner;
/// type of outer boundary
extern t_boundary_condition boundary_outer;
/// set dr/dOmega zero at outer boundary
extern bool domegadr_zero;

/// Struct for handling damping at boundaries
struct t_DampingType
{
	void (*inner_damping_function)(t_polargrid&, t_polargrid&, double);
	void (*outer_damping_function)(t_polargrid&, t_polargrid&, double);
	t_data::t_polargrid_type array_to_damp;
	t_data::t_polargrid_type array_with_damping_values;
	std::string description_inner;
	std::string description_outer;
};
extern int damping_energy_id;


/// enable different damping types
enum t_damping_type {damping_none, damping_initial, damping_mean, damping_zero};
extern bool damping;
/// inner damping limit
extern double damping_inner_limit;
/// outer damping limit
extern double damping_outer_limit;
/// damping time factor
extern double damping_time_factor;
/// vector to handle damping structs
extern std::vector<t_DampingType> damping_vector;

// energy equation
/// mean molecular mass
extern double MU;
/// minimum temperature allowed
extern double minimum_temperature;
/// maximum temperature allowed
extern double maximum_temperature;
/// enable viscous heating
extern bool heating_viscous_enabled;
/// viscous heating factor
extern double heating_viscous_factor;
/// enable star heating
extern bool heating_star_enabled;
/// star heating factor
extern double heating_star_factor;
/// star heating ramping time
extern double heating_star_ramping_time;
/// star heating mode
extern bool heating_star_simple;

/// local radiative cooling enabled
extern bool cooling_radiative_enabled;
/// local radiative cooling factor
extern double cooling_radiative_factor;
/// beta cooling enabled 
extern bool cooling_beta_enabled;
/// beta cooling constant
extern double cooling_beta;

/// enable radiative diffusion
extern bool radiative_diffusion_enabled;
/// omega for SOR in radiative diffusion
extern double radiative_diffusion_omega;
/// enable automatic omega in SOR in radiative diffusion
extern bool radiative_diffusion_omega_auto_enabled;
/// maximum iterations in SOR in radiative diffusion
extern unsigned int radiative_diffusion_max_iterations;

// initialisation
enum t_initialize_condition {initialize_condition_profile, initialize_condition_read1D, initialize_condition_read2D, initialize_condition_shakura_sunyaev};

/// initialize condition for sigma
extern t_initialize_condition sigma_initialize_condition;
/// filename to read sigma profile from
extern char* sigma_filename;
/// random seed
extern int random_seed;
/// randomize sigma?
extern bool sigma_randomize;
/// sigma random factor
extern double sigma_random_factor;
/// open simplex feature size
extern double sigma_feature_size;
/// sigma floor (in multiples of Sigma0)
extern double sigma_floor;
/// adjust sigma0 to have total discmass = sigma_discmass
extern bool sigma_adjust;
/// total discmass
extern double sigma_discmass;
/// Sigma0
extern double sigma0;

/// initiliaze condition for energy
extern t_initialize_condition energy_initialize_condition;
/// filename to read energy profile from
extern char* energy_filename;

/// enable profile damping
extern bool profile_damping;
/// profile damping point
extern double profile_damping_point;
/// profile damping width
extern double profile_damping_width;

// type of artifical viscosity
enum t_artificial_viscosity {
	artificial_viscosity_none,	// no artificial viscosity
	artificial_viscosity_TW,	// artificial viscosity based on Tscharnuter & Winkler, 1979
	artificial_viscosity_SN		// artificial viscosity based on Stone & Norman, 1991 (ZEUS)
};

/// type of artificial viscosity
extern t_artificial_viscosity artificial_viscosity;
/// factor/constant for artificial for viscosity. (CVNR)
extern double artificial_viscosity_factor;
/// enable/disable dissipation thru artificial viscosity
extern bool artificial_viscosity_dissipation;

extern double thickness_smoothing;
extern double thickness_smoothing_sg;

/// calculate disk (hydro)
extern bool calculate_disk;

extern bool massoverflow;
extern unsigned int mof_planet;
extern double mof_sigma;
extern double mof_value;

/// star feels disk
extern bool feels_disk;

/// factor for conversation from surface density to density
extern double density_factor;

/// factor for tau calculation
extern double tau_factor;

/// factor for kappa calculation
extern double kappa_factor;

///
extern bool integrate_planets;

// self gravity
extern bool self_gravity;

// output
extern bool write_torques;

/// write disk quantities
extern bool write_disk_quantities;
extern bool write_at_every_timestep;
extern bool write_lightcurves;
extern std::vector<double> lightcurves_radii;
extern bool write_massflow;

// runtime output
extern unsigned int log_after_steps;
extern double log_after_real_seconds;


// type of opacity
enum t_opacity {
	opacity_lin,	// opacity based on Lin & Papaloizou, 1985
	opacity_bell,	// opacity based on Bell & Lin, 1994
	opacity_zhu,	// opacity based on Zhu, Hartmann & Gammie, 2008
	opacity_kramers,	// opacity based on Kramers Law plus electron scattering (Thomson)
	opacity_const_op // constant opacity
};

extern t_opacity opacity;

// For use of constant opacity
extern double kappa_const;

/// initialize pure keplerian
extern bool initialize_pure_keplerian;
extern bool initialize_vradial_zero;

/// star parameters
extern double star_radius;
extern double star_temperature;

extern unsigned int zbuffer_size;
extern double zbuffer_maxangle;

/// boundary layer parameters
extern double radial_viscosity_factor;
extern double vrad_fraction_of_kepler;
extern double stellar_rotation_rate;
extern double mass_accretion_rate;

/// CFL Factor
extern double CFL;

/// length base unit
extern double L0;
/// mass base unit
extern double M0;

/// (total) number of particles
extern unsigned int number_of_particles;
/// enable particle integration
extern bool integrate_particles;
/// particle radius
extern double particle_radius;
/// particle density
extern double particle_density;
/// particle slope
extern double particle_slope;
/// particle minimum radius
extern double particle_minimum_radius;
/// particle maximum radius
extern double particle_maximum_radius;
/// particle escape radius
extern double particle_escape_radius;
/// particle gas drag
extern bool particle_gas_drag_enabled;
/// particle disk gravity
extern bool particle_disk_gravity_enabled;

void read(char* filename, t_data &data);
void summarize_parameters();
void apply_units();
void write_grid_data_to_file();
};

#endif // PARAMETERS_H
