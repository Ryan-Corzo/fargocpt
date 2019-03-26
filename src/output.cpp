#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include "stress.h"
#include "util.h"
#include "global.h"
#include "LowTasks.h"
#include "constants.h"
#include "output.h"
#include "nongnu.h"
#include "logging.h"
#include "viscosity.h"
#include "parameters.h"
#include "SideEuler.h"
#include "quantities.h"
#include "options.h"

#include <sys/stat.h>
#include <fstream>
#include <cstdio>
#include <limits>


namespace output {

// info on variables in misc file
const std::map<const std::string, const int> misc_file_column_v1 = {
	{ "TimeStep", 0 }
	,{ "PhysicalTime", 1 }
	,{ "OmegaFrame", 2 }
	,{ "LostMass", 3 }
	,{ "FrameAngle", 4 } };

const std::map<const std::string, const int> misc_file_column_v2 = {
	{ "TimeStep", 0 }
	,{ "PhysicalTime", 1 }
	,{ "OmegaFrame", 2 }
	,{ "FrameAngle", 3 } };

auto misc_file_columns = misc_file_column_v2;

const std::map<const std::string, const std::string> misc_file_variables = {
	{ "TimeStep", "1" },
	{ "PhysicalTime", "s" },
	{ "OmegaFrame", "1/s" },
	{ "LostMass", "g" },
	{ "FrameAngle", "1" }
	};

const std::map<const std::string, const int> quantities_file_column_v2 = {
{ "physical time", 0 },
{ "mass", 1 },
{ "angular momentum", 2 },
{ "total energy", 3 },
{ "internal energy", 4 },
{ "kinematic energy", 5 },
{ "potential energy", 6 },
{ "qplus", 7 },
{ "qminus", 8 },
{ "pvdiv", 9 },
{ "radial kinetic energy", 10 },
{ "azimuthal kinetic energy", 11 },
{ "delta mass inner positive", 12 },
{ "delta mass inner negative", 13 },
{ "delta mass outer positive", 14 },
{ "delta mass outer negative", 15 },
{ "delta mass wave damping positive", 16 },
{ "delta mass wave damping negative", 17 }
};

auto quantities_file_column = quantities_file_column_v2;

const std::map<const std::string, const std::string> quantities_file_variables = {
{ "physical time", "s" },
{ "mass", "g" },
{ "angular momentum", "g cm2/s" },
{ "total energy", "J" },
{ "internal energy", "J" },
{ "kinematic energy", "J" },
{ "potential energy", "J" },
{ "qplus", "1" },
{ "qminus", "1" },
{ "pvdiv", "1" },
{ "radial kinetic energy", "J" },
{ "azimuthal kinetic energy", "J" },
{ "delta mass inner positive", "g" },
{ "delta mass inner negative", "g" },
{ "delta mass outer positive", "g" },
{ "delta mass outer negative", "g" },
{ "delta mass wave damping positive", "g" },
{ "delta mass wave damping negative", "g" }
};

void check_free_space(t_data &data)
{
	char *directory_name;
	DIR *directory_pointer;
	struct dirent *directory_entry;
	struct statvfs fiData;

	if (asprintf(&directory_name, "%s/",OUTPUTDIR) <0) {
		die("Not enough memory.");
    }

    // Create output directory if it doesn't exist
    if(CPU_Master)
    {
        struct stat buffer;
        if(stat(OUTPUTDIR, &buffer))
        {
            mkdir(OUTPUTDIR, 0700);
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);


	// check if output directory exists
	if ((directory_pointer = opendir(directory_name)) == NULL) {
        logging::print_master(LOG_ERROR "Output directory %s doesn't exist!\n", OUTPUTDIR);
        die("Not output directory!");
    }

	while ((directory_entry = readdir(directory_pointer))) {
		if ((strcmp("..",directory_entry->d_name) != 0) && (strcmp(".",directory_entry->d_name) != 0)) {
				logging::print_master(LOG_NOTICE "Output directory %s is not empty!\n", OUTPUTDIR);
			break;
		}
	}

	closedir(directory_pointer);

	unsigned long int space_needed = 0l;
	unsigned int number_of_files = 0l;

	// go thru all polargrids in output_polargrids and check if they are going to be put out
	for (unsigned int i = 0; i < t_data::N_POLARGRID_TYPES; ++i) {
		if (data[(t_data::t_polargrid_type)i].get_write_1D()) {
			space_needed += data[(t_data::t_polargrid_type)i].bytes_needed_1D();
			number_of_files += 1;
		}

		if (data[(t_data::t_polargrid_type)i].get_write_2D()) {
			space_needed += data[(t_data::t_polargrid_type)i].bytes_needed_2D();
			number_of_files += 1;
		}
	}

	space_needed *= NTOT/NINTERM;
	number_of_files *= NTOT/NINTERM;

	logging::print_master(LOG_INFO "Output information:\n");
	logging::print_master(LOG_INFO "   Output directory: %s\n", OUTPUTDIR);
	logging::print_master(LOG_INFO "    Number of files: %u\n", number_of_files);
	logging::print_master(LOG_INFO "  Total output size: %.2f GB\n", (double)space_needed/1024.0/1024.0/1024.0);

	if((statvfs(directory_name,&fiData)) < 0 ) {
		logging::print_master(LOG_WARNING "Couldn't stat filesystem. You have to check for enough free space manually!\n");
	} else {
		// free space of device, more precisely number of space available to non-priv processes (like us)
		unsigned long int free_space = fiData.f_bavail * fiData.f_frsize;

		logging::print_master(LOG_INFO "    Space Available: %.2f GB\n", (double)free_space/1024.0/1024.0/1024.0);

		if (space_needed > free_space) {
			logging::print_master(LOG_WARNING "There is not enough space for all outputs! The program will fail at same point!\n");
		}
	}

	free(directory_name);
}

void write_grids(t_data &data, int index, int iter, double phystime)
{
	logging::print_master(LOG_INFO "Writing output %d, Timestep Number %d, Physical Time %f.\n", index, iter, phystime);

	// go thru all grids and write them
	for (unsigned int i = 0; i < t_data::N_POLARGRID_TYPES; ++i) {
		data[(t_data::t_polargrid_type)i].write(index, data);
	}

	// go thru all grids and write them
	for (unsigned int i = 0; i < t_data::N_RADIALGRID_TYPES; ++i) {
		data[(t_data::t_radialgrid_type)i].write(index, data);
	}
}

/**

*/
void write_quantities(t_data &data)
{
	FILE* fd = 0;
	char* fd_filename;
	static bool fd_created = false;

	if (CPU_Master) {

		if (asprintf(&fd_filename, "%s%s", OUTPUTDIR, "Quantities.dat") == -1) {
			logging::print_master(LOG_ERROR "Not enough memory for string buffer.\n");
			PersonalExit(1);
		}
		// check if file exists and we restarted
		if ((options::restart) && !(fd_created)) {
			fd = fopen(fd_filename, "r");
			if (fd) {
				fd_created = true;
			}
			fclose(fd);
		}

		// open logfile
		if (!fd_created) {
			fd = fopen(fd_filename, "w");
		} else {
			fd = fopen(fd_filename, "a");
		}
		if (fd == NULL) {
			logging::print_master(LOG_ERROR "Can't write 'Quantities.dat' file. Aborting.\n");
			PersonalExit(1);
		}

		free(fd_filename);

		if (!fd_created) {
			// print header
			fprintf(fd,"#FargoCPT quantities file\n");
			fprintf(fd,"#version: 2\n");
			fprintf(fd,"%s", text_file_variable_description(quantities_file_column, quantities_file_variables).c_str() );
			fd_created = true;
		}
	}

	// computate absolute deviation from start values (this has to be done on all nodes!)
	double totalMass = quantities::gas_total_mass(data);
	double totalAngularMomentum = quantities::gas_angular_momentum(data)*units::angular_momentum;
	double internalEnergy = quantities::gas_internal_energy(data)*units::energy;
	double kinematicEnergy = quantities::gas_kinematic_energy(data)*units::energy;
	double radialKinematicEnergy = quantities::gas_radial_kinematic_energy(data)*units::energy;
	double azimuthalKinematicEnergy = quantities::gas_azimuthal_kinematic_energy(data)*units::energy;
	double gravitationalEnergy = quantities::gas_gravitational_energy(data)*units::energy;
	double totalEnergy = internalEnergy + kinematicEnergy + gravitationalEnergy;

	if (CPU_Master) {
		// print to logfile
		fprintf(fd, "%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\t%#.16e\n",
			PhysicalTime,
			totalMass,
			totalAngularMomentum,
			totalEnergy,
			internalEnergy,
			kinematicEnergy,
			gravitationalEnergy,
			data.qplus_total,
			data.qminus_total,
			data.pdivv_total,
			radialKinematicEnergy,
			azimuthalKinematicEnergy,
			MassDelta.InnerPositive,
			MassDelta.InnerNegative,
			MassDelta.OuterPositive,
			MassDelta.OuterNegative,
			MassDelta.WaveDampingPositive,
			MassDelta.WaveDampingNegative );

		// set mass delta to 0
		MassDelta.reset();

		// close file
		fclose(fd);
	}
}

/**
	log misc. data
*/
void write_misc(unsigned int timestep)
{
	FILE *fd = 0;
	char* fd_filename;
	static bool fd_created = false;

	if (CPU_Master) {
		if (asprintf(&fd_filename, "%s%s", OUTPUTDIR, "misc.dat") == -1) {
			logging::print_master(LOG_ERROR "Not enough memory for string buffer.\n");
			PersonalExit(1);
		}
		// check if file exists and we restarted
		if ((options::restart) && !(fd_created)) {
			fd = fopen(fd_filename, "r");
			if (fd) {
				fd_created = true;
			}
			fclose(fd);
		}
		// open logfile
		if (!fd_created) {
			fd = fopen(fd_filename, "w");
		} else {
			fd = fopen(fd_filename, "a");
		}
		if (fd == NULL) {
			logging::print_master(LOG_ERROR "Can't write 'misc.dat' file. Aborting.\n");
			PersonalExit(1);
		}

		free(fd_filename);

		if (!fd_created) {
			// print header
			fprintf(fd,"#FargoCPT misc file\n");
			fprintf(fd,"#version: 2\n");
			fprintf(fd,"%s", text_file_variable_description(misc_file_columns, misc_file_variables).c_str());
			fd_created = true;
		}
	}

	if (CPU_Master) {
		// print to logfile
		fprintf(fd, "%u\t%#.18g\t%#.18g\t%#.18g\n", timestep, PhysicalTime, OmegaFrame, FrameAngle);

		// close file
		fclose(fd);
	}
}

std::string get_version(std::string filename) {
	std::string version;
	std::ifstream infile(filename);
	std::string line_start;
	while (infile >> line_start) {
		// is it the version line
		if (line_start == "#version:") {
			infile >> version;
			return version;
		} else if (line_start.substr(0,1) == "#") {
			// were still in the header, jump to next line
			infile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		} else {
			// not a comment line anymore, so nothing was found
			break;
		}
	}
	// nothing found up until here, assume dafault version 1.
	return "1";
}

std::string text_file_variable_description(const std::map<const std::string, const int> &variables, const std::map<const std::string, const std::string> &units) {
	// construct a header string describing each variable in
	// its own line including the column and its unit. e.g.
	// #variable: 1 | PhysicalTime | s

	std::map<int, std::string> vars_by_column;
	for (auto const &ent : variables) {
		std::string name = ent.first;
		int column = ent.second;
		vars_by_column[column] = name;
	}
	std::string var_descriptor;
	for (auto const &ent : vars_by_column) {
		std::string column = std::to_string(ent.first);
		std::string name = ent.second;
		std::string unit = units.at(name);
		var_descriptor += "#variable: " + column + " | "
			+ name + " | " + unit + "\n";
	}
	return var_descriptor;
}

double get_from_ascii_file(std::string filename, unsigned int timestep, unsigned int column) {
	unsigned int line_timestep = 0;
	std::ifstream infile(filename);
	std::string line_start;

	while (infile >> line_start) {
		// search the file until the correct timestep is found
		if (line_start.substr(0,1) == "#") {
			// jump to next line
			infile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		} else {
			// check the timestep
			line_timestep = std::stoul(line_start);
			if (line_timestep == timestep) {
				break;
			} else {
				// jump to next line
				infile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			}
		}
	}
	double rv;
	// read as many times as needed to reach the desired value
	for (unsigned int i=0; i<column; i++) {
		infile >> rv;
	}
	return rv;
}

double get_misc(unsigned int timestep, std::string variable)
{
	unsigned int column = 0;
	std::string filename = std::string(OUTPUTDIR) + "misc.dat";
	std::string version = get_version(filename);

	if (version == "2") {
		if (variable == "timestep") column = 0;
		else if (variable == "physical time") column = 1;
		else if (variable == "OmegaFrame") column = 2;
		else if (variable == "FrameAngle") column = 3;
		else {
			printf("Don't know variable '%s' in misc.dat v2\n", variable.c_str());
			PersonalExit(1);
		}
	}
	else if (version == "1") {
		if (variable == "timestep") column = 0;
		else if (variable == "physical time") column = 1;
		else if (variable == "OmegaFrame") column = 2;
		else if (variable == "LostMass") column = 3;
		else if (variable == "FrameAngle") column = 4;
		else {
			printf("Don't know variable '%s' in misc.dat v1\n", variable.c_str());
			PersonalExit(1);
		}
	}
    return get_from_ascii_file(filename, timestep, column);
}

void write_torques(t_data &data, unsigned int timestep, bool force_update) {
	double *global_torques = (double*)malloc(sizeof(*global_torques)*(data.get_planetary_system().get_number_of_planets()+1));
	double *local_torques = (double*)malloc(sizeof(*local_torques)*(data.get_planetary_system().get_number_of_planets()+1));

	// central star
	local_torques[0] = 0.0;

	// do everything for all planets/stars
	for (unsigned int n_planet = 0; n_planet < data.get_planetary_system().get_number_of_planets(); ++n_planet) {
		local_torques[n_planet+1] = 0;
		t_planet& planet = data.get_planetary_system().get_planet(n_planet);
		double smooth = parameters::thickness_smoothing * ASPECTRATIO * pow(planet.get_distance(), 1.0+FLARINGINDEX);

		// hill radius
		double r_hill = pow(planet.get_mass()/(3.0*(M+planet.get_mass())),1.0/3.0)*planet.get_semi_major_axis();
		// 0.8 = cut off parameter
		double r_taper = 0.8 * r_hill;

		for (unsigned int n_radial = 0; n_radial <= data[t_data::TORQUE].get_max_radial(); ++n_radial) {
			data[t_data::TORQUE_1D](n_radial) = 0.0;

			for (unsigned int n_azimuthal = 0; n_azimuthal <= data[t_data::TORQUE].get_max_azimuthal(); ++n_azimuthal) {
				double phi = (double)n_azimuthal/(double)data[t_data::TORQUE].get_size_azimuthal()*2.0*PI;
				double dx = planet.get_x() - Rmed[n_radial]*cos(phi);
				double dy = planet.get_y() - Rmed[n_radial]*sin(phi);
				double m = data[t_data::DENSITY](n_radial, n_azimuthal)*Surf[n_radial];

				double distance2 = pow2(dx) + pow2(dy) + pow2(smooth);

				double taper = 1.0;

				if (sqrt(pow2(dx) + pow2(dy)) < 4.0*r_taper) {
					taper = 1.0/(exp(-(sqrt(pow2(dx)+pow2(dy))-r_taper)/(0.1*r_taper))+1.0);
				}

				double F_x = - dx * m * planet.get_mass() * pow(distance2, -1.5) * taper;
				double F_y = - dy * m * planet.get_mass() * pow(distance2, -1.5) * taper;

				data[t_data::TORQUE](n_radial, n_azimuthal) = planet.get_x()*F_y - planet.get_y()*F_x;
				data[t_data::TORQUE_1D](n_radial) += data[t_data::TORQUE](n_radial, n_azimuthal);
				if ((n_radial >= ((CPU_Rank == 0) ? GHOSTCELLS_B : CPUOVERLAP)) && (n_radial <= data[t_data::TORQUE].get_max_radial() - ( CPU_Rank == CPU_Highest ? GHOSTCELLS_B : CPUOVERLAP ))) {
					local_torques[n_planet+1] += data[t_data::TORQUE](n_radial, n_azimuthal);
				}
			}
		}

		char* name;
		if (asprintf(&name, "1D_torque_planet%i_", n_planet)<0) {
			die("Not enough memory!");
		}
		data[t_data::TORQUE_1D].set_name(name);
		free(name);

		if (force_update == false) {
			data[t_data::TORQUE_1D].write1D(timestep);
		}
	}

	MPI_Allreduce(local_torques, global_torques, data.get_planetary_system().get_number_of_planets()+1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

	if (CPU_Master) {
		// output
		FILE* fd = 0;
		char* fd_filename;
		static bool fd_created = false;

		if (asprintf(&fd_filename, "%s%s", OUTPUTDIR, "torques.dat") == -1) {
			logging::print_master(LOG_ERROR "Not enough memory for string buffer.\n");
			PersonalExit(1);
		}

		// check if file exists and we restarted
		if ((options::restart) && !(fd_created)) {
			fd = fopen(fd_filename, "r");
			if (fd) {
				fd_created = true;
				fclose(fd);
			}
		}
		// open logfile
		if (!fd_created) {
			fd = fopen(fd_filename, "w");
		} else {
			fd = fopen(fd_filename, "a");
		}
		if (fd == NULL) {
			logging::print_master(LOG_ERROR "Can't write 'torques.dat' file. Aborting.\n");
			PersonalExit(1);
		}

		free(fd_filename);

		if (!fd_created) {
			// print header
			fprintf(fd,"# \n");
			fd_created = true;
		}

		fprintf(fd,"%.20e", PhysicalTime);
		for (unsigned int i = 0; i <= data.get_planetary_system().get_number_of_planets(); ++i) {
			fprintf(fd,"\t%.20e", global_torques[i]*units::torque);
		}
		fprintf(fd, "\n");

		// close file
		fclose(fd);
	}

	free(local_torques);
	free(global_torques);
}


/**
	Calculates eccentricity, semi major axis and periastron averaged with the radial cells
*/
void write_disk_quantities(t_data &data, unsigned int timestep, bool force_update)
{
	double local_eccentricity = 0.0;
	double gas_total_mass = quantities::gas_total_mass(data);
	double disk_eccentricity = 0.0;
	//double semi_major_axis = 0.0;
	//double local_semi_major_axis = 0.0;
	double local_mass= 0.0;
	double periastron = 0.0;
	double local_periastron = 0.0;

	// calculate eccentricity, semi_major_axis and periastron grid
	quantities::calculate_disk_quantities(data, timestep, force_update);

	// Loop thru all cells excluding GHOSTCELLS & CPUOVERLAP cells (otherwise they would be included twice!)
	for (unsigned int n_radial = (CPU_Rank == 0) ? GHOSTCELLS_B : CPUOVERLAP; n_radial <= data[t_data::DENSITY].get_max_radial() - ( CPU_Rank == CPU_Highest ? GHOSTCELLS_B : CPUOVERLAP ); ++n_radial) {
		for (unsigned int n_azimuthal = 0; n_azimuthal <= data[t_data::DENSITY].get_max_azimuthal(); ++n_azimuthal) {
			// eccentricity and semi major axis weighted with cellmass
			local_mass = data[t_data::DENSITY](n_radial, n_azimuthal) * Surf[n_radial];
			local_eccentricity += data[t_data::ECCENTRICITY](n_radial, n_azimuthal) * local_mass;
			//local_semi_major_axis += data[t_data::SEMI_MAJOR_AXIS](n_radial, n_azimuthal) * local_mass;
			local_periastron += data[t_data::PERIASTRON](n_radial, n_azimuthal) * local_mass;
		}
	}

	// synchronize threads
	MPI_Allreduce(&local_eccentricity, &disk_eccentricity, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
	//MPI_Allreduce(&local_semi_major_axis, &semi_major_axis, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
	MPI_Allreduce(&local_periastron, &periastron, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

	disk_eccentricity /= gas_total_mass;
	//semi_major_axis /= gas_total_mass;
	periastron /= gas_total_mass;

	if (CPU_Master) {
		// output
		FILE* fd = 0;
		char* fd_filename;
		static bool fd_created = false;

		if (asprintf(&fd_filename, "%s%s", OUTPUTDIR, "disk_quantities.dat") == -1) {
			logging::print_master(LOG_ERROR "Not enough memory for string buffer.\n");
			PersonalExit(1);
		}

		// check if file exists and we restarted
		if ((options::restart) && !(fd_created)) {
			fd = fopen(fd_filename, "r");
			if (fd) {
				fd_created = true;
			}
			fclose(fd);
		}
		// open logfile
		if (!fd_created) {
			fd = fopen(fd_filename, "w");
		} else {
			fd = fopen(fd_filename, "a");
		}
		if (fd == NULL) {
			logging::print_master(LOG_ERROR "Can't write 'disk_quantities.dat' file. Aborting.\n");
			PersonalExit(1);
		}

		free(fd_filename);

		if (!fd_created) {
			// print header
			fprintf(fd,"# PhysicalTime\tdisk_eccentricity\tsemi_major_axis (NYI)\tperiastron\tdiskmass\n");
			fd_created = true;
		}

		fprintf(fd,"%.20e\t%.20e\t%.20e\t%.20e\t%.20e\n",PhysicalTime,disk_eccentricity,0.0,periastron,gas_total_mass);

		// close file
		fclose(fd);
	}
}

void write_lightcurves(t_data &data, unsigned int timestep, bool force_update)
{
	// calculate luminosity
	quantities::calculate_radial_luminosity(data, timestep, force_update);

	// calculate dissipation
	quantities::calculate_radial_dissipation(data, timestep, force_update);

	double* luminosity_values = new double[parameters::lightcurves_radii.size()];
	double* dissipation_values = new double[parameters::lightcurves_radii.size()];

	for (unsigned int i = 0; i < parameters::lightcurves_radii.size(); ++i) {
		luminosity_values[i] = 0.0;
		dissipation_values[i] = 0.0;
	}

	// all cpu except the lowest one expects data
	if (CPU_Rank > 0) {
		MPI_Recv(luminosity_values, parameters::lightcurves_radii.size(), MPI_DOUBLE, CPU_Prev, 0, MPI_COMM_WORLD, NULL);
		MPI_Recv(dissipation_values, parameters::lightcurves_radii.size(), MPI_DOUBLE, CPU_Prev, 0, MPI_COMM_WORLD, NULL);
	}

	unsigned int current_lightcurves_bin = 0;
	for (unsigned int n_radial = (CPU_Rank == 0) ? GHOSTCELLS_B : CPUOVERLAP; n_radial <= data[t_data::LUMINOSITY_1D].get_max_radial() - ( CPU_Rank == CPU_Highest ? GHOSTCELLS_B : CPUOVERLAP ); ++n_radial) {
		while ((current_lightcurves_bin < parameters::lightcurves_radii.size()-1) && (parameters::lightcurves_radii[current_lightcurves_bin] < Rmed[n_radial])) {
			current_lightcurves_bin++;
		}
		luminosity_values[current_lightcurves_bin] += data[t_data::LUMINOSITY_1D](n_radial);
		dissipation_values[current_lightcurves_bin] += data[t_data::DISSIPATION_1D](n_radial);
	}

	// all cpu except the hightest one sends data
	if (CPU_Rank < CPU_Highest) {
		MPI_Send(luminosity_values, parameters::lightcurves_radii.size(), MPI_DOUBLE, CPU_Next, 0, MPI_COMM_WORLD);
		MPI_Send(dissipation_values, parameters::lightcurves_radii.size(), MPI_DOUBLE, CPU_Next, 0, MPI_COMM_WORLD);
	}

	// the last process can write the data
	if (CPU_Rank == CPU_Highest) {
		// write luminosities
		FILE* fd = 0;
		char* fd_filename;
		static bool fd_created_luminosity = false;

		if (asprintf(&fd_filename, "%s%s", OUTPUTDIR, "luminosity.dat") == -1) {
			logging::print_master(LOG_ERROR "Not enough memory for string buffer.\n");
			PersonalExit(1);
		}

		// check if file exists and we restarted
		if ((options::restart) && !(fd_created_luminosity)) {
			fd = fopen(fd_filename, "r");
			if (fd) {
				fd_created_luminosity = true;
			}
			fclose(fd);
		}
		// open logfile
		if (!fd_created_luminosity) {
			fd = fopen(fd_filename, "w");
		} else {
			fd = fopen(fd_filename, "a");
		}
		if (fd == NULL) {
			logging::print_master(LOG_ERROR "Can't write 'luminosity.dat' file. Aborting.\n");
			PersonalExit(1);
		}

		free(fd_filename);

		if (!fd_created_luminosity) {
			// print header
			fprintf(fd,"# PhysicalTime\tluminosities\n");
			fd_created_luminosity = true;
		}

		fprintf(fd,"%.20e\t",PhysicalTime);

		for (unsigned int i = 1; i < parameters::lightcurves_radii.size(); ++i) {
			fprintf(fd, "%.20e\t", luminosity_values[i]*units::power.get_cgs_factor());
		}
		fprintf(fd,"\n");

		// close file
		fclose(fd);

		// write dissipation
		static bool fd_created_dissipation = false;

		if (asprintf(&fd_filename, "%s%s", OUTPUTDIR, "dissipation.dat") == -1) {
			logging::print_master(LOG_ERROR "Not enough memory for string buffer.\n");
			PersonalExit(1);
		}

		// check if file exists and we restarted
		if ((options::restart) && !(fd_created_dissipation)) {
			fd = fopen(fd_filename, "r");
			if (fd) {
				fd_created_dissipation = true;
				fclose(fd);
			}
		}
		// open logfile
		if (!fd_created_dissipation) {
			fd = fopen(fd_filename, "w");
		} else {
			fd = fopen(fd_filename, "a");
		}
		if (fd == NULL) {
			logging::print_master(LOG_ERROR "Can't write 'dissipation.dat' file. Aborting.\n");
			PersonalExit(1);
		}

		free(fd_filename);

		if (!fd_created_dissipation) {
			// print header
			fprintf(fd,"# PhysicalTime\tdissipation\n");
			fd_created_dissipation = true;
		}

		fprintf(fd,"%.20e\t",PhysicalTime);

		for (unsigned int i = 1; i < parameters::lightcurves_radii.size(); ++i) {
			fprintf(fd, "%.20e\t", dissipation_values[i]*units::power.get_cgs_factor());
		}
		fprintf(fd,"\n");

		// close file
		fclose(fd);
	}

    delete[] luminosity_values;
    delete[] dissipation_values;
}

/**
Write for each coarse output step the corresponding fine grained output number and the simulation time in cgs units.
*/
void write_coarse_time(unsigned int coarseOutputNumber, unsigned int fineOutputNumber)
{
	FILE* fd = 0;
	char* fd_filename;
	static bool fd_created = false;

	if (CPU_Master) {

		if (asprintf(&fd_filename, "%s%s", OUTPUTDIR, "timeCoarse.dat") == -1) {
			logging::print_master(LOG_ERROR "Not enough memory for string buffer.\n");
			PersonalExit(1);
		}
		// check if file exists and we restarted
		if ((options::restart) && !(fd_created)) {
			fd = fopen(fd_filename, "r");
			if (fd) {
				fd_created = true;
				fclose(fd);
			}
		}

		// open logfile
		if (!fd_created) {
			fd = fopen(fd_filename, "w");
		} else {
			fd = fopen(fd_filename, "a");
		}
		if (fd == NULL) {
			logging::print_master(LOG_ERROR "Can't write 'timeCoarse.dat' file. Aborting.\n");
			PersonalExit(1);
		}

		free(fd_filename);

		if (!fd_created) {
			// print header
			fprintf(fd,"# Time log for course output.\n# Syntax: coarse output step <tab> fine output step <tab> physical time (cgs)\n");
			fd_created = true;
		}
	}

	if (CPU_Master) {
		fprintf(fd, "%u\t%u\t%#.16e\n"
				, coarseOutputNumber
				, fineOutputNumber
				, PhysicalTime*units::time);
		fclose(fd);
	}
}

} // close namespace
