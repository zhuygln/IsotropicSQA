#ifndef _NULIB_INTERFACE_H
#define _NULIB_INTERFACE_H

#include "H5Cpp.h"
#include <string>
#include <cstdlib>
//#include "mstl.h"

inline bool hdf5_dataset_exists(const char* filename, const char* datasetname){
  bool exists = true;

  // Temporarily turn off error printing                                                                                   
  H5E_auto2_t func;
  void* client_data;
  H5::Exception::getAutoPrint(func,&client_data);
  H5::Exception::dontPrint();

  // See if dataset exists                                                                                                 
  H5::H5File file(filename, H5F_ACC_RDONLY);
  H5::DataSet dataset;
  try{
    dataset = file.openDataSet(datasetname);
  }
  catch(H5::FileIException& exception){
    exists = false;
  }

  // Turn error printing back on                                                                                           
  H5::Exception::setAutoPrint(func,client_data);
  file.close();

  return exists;
}

// module variables set in fortran NuLib code
extern int     __nulibtable_MOD_nulibtable_number_species;
extern int     __nulibtable_MOD_nulibtable_number_easvariables;
extern int     __nulibtable_MOD_nulibtable_number_groups;
extern int     __nulibtable_MOD_nulibtable_nrho;
extern int     __nulibtable_MOD_nulibtable_ntemp;
extern int     __nulibtable_MOD_nulibtable_nye;
extern int     __nulibtable_MOD_nulibtable_nitemp;
extern int     __nulibtable_MOD_nulibtable_nieta;
extern double* __nulibtable_MOD_nulibtable_energies;
extern double* __nulibtable_MOD_nulibtable_ewidths;
extern double* __nulibtable_MOD_nulibtable_ebottom;
extern double* __nulibtable_MOD_nulibtable_etop;
extern double* __nulibtable_MOD_nulibtable_logrho;
extern double* __nulibtable_MOD_nulibtable_logtemp;
extern double* __nulibtable_MOD_nulibtable_ye;
extern double* __nulibtable_MOD_nulibtable_logitemp;
extern double* __nulibtable_MOD_nulibtable_logieta;
extern double  __nulibtable_MOD_nulibtable_logtemp_min;
extern double  __nulibtable_MOD_nulibtable_logtemp_max;
extern double  __nulibtable_MOD_nulibtable_logrho_min;
extern double  __nulibtable_MOD_nulibtable_logrho_max;
extern double  __nulibtable_MOD_nulibtable_ye_min;
extern double  __nulibtable_MOD_nulibtable_ye_max;
extern double  __nulibtable_MOD_nulibtable_logitemp_min;
extern double  __nulibtable_MOD_nulibtable_logitemp_max;
extern double  __nulibtable_MOD_nulibtable_logieta_min;
extern double  __nulibtable_MOD_nulibtable_logieta_max;
extern int     __nulib_MOD_total_eos_variables;

// These are fortran functions and module variables in nulib.a                                                                  
extern "C"{
  void nulibtable_range_species_range_energy_(
		  double*, //rho
		  double*, //temp
		  double*, //ye
		  double*, //eas_species_energy (3D array)
		  int*,    //number of species (3,5,6)
		  int*,    //number of groups
		  int*);   //number of easvariables (3)

  void nulibtable_single_species_range_energy_(
		  double*, //rho
		  double*, //temp
		  double*, //Ye
		  int*,    //species number
		  double*, //eas_energy (2D array)
		  int*,    //number of groups
		  int*);   //number of easvariables(3)

  void nulibtable_epannihil_single_species_range_energy_(
		  double* temp,  // MeV
		  double* eta,   // mu/kT
		  int* lns,      // species number
		  double* phi,   // phi[legendre-p/a index][this_group][anti-group]
		  int* ngroups1,
		  int* ngroups2,
		  int* n_phis);

  void nulibtable_inelastic_single_species_range_energy_(
		  double* temp,  // MeV
		  double* eta,   // mu/kT
		  int* lns,      // species number
		  double* phi,   // phi[legendre index][out group][in group]
		  int* ngroups1, // ng in
		  int* ngroups2, // ng out (should be same as eas_n1)
		  int* n_phis);   // number of legendre terms (=2)

  void nulibtable_reader_(char*,int*,int*,int*,int);
  void set_eos_variables_(double* eos_variables);
  void read_eos_table_(char* filename);
}

class EAS{
 public:
  int do_iscat, do_pair, do_delta;
  int ns, ng, nv;
  double munue = 0./0.; /*erg*/
  vector<double> eas;
  vector<double> escat_kernel0; // out-scattering
  vector<double> escat_kernel1;
  vector<double> pair_kernel0; // neutrino pair annihilation
  vector<double> pair_kernel1;

  EAS(){}
  EAS(string filename, string eosfilename){
    // select what to read
    do_iscat = 0;
    do_pair = 0;
    do_delta = 0;
    if(hdf5_dataset_exists(filename.c_str(),"/scattering_delta")) do_delta = true;
    if(hdf5_dataset_exists(filename.c_str(),"/inelastic_phi0"))   do_iscat = true;
    if(hdf5_dataset_exists(filename.c_str(),"/epannihil_phi0"))   do_pair = true;
    nulibtable_reader_((char*)filename.c_str(), &do_iscat, &do_pair, &do_delta, filename.length());
    read_eos_table_((char*)eosfilename.c_str());

    // resize arrays
    ns = __nulibtable_MOD_nulibtable_number_species;
    ng = __nulibtable_MOD_nulibtable_number_groups;
    nv = __nulibtable_MOD_nulibtable_number_easvariables;
    eas.resize(ns*ng*nv);
    if(do_iscat){
      escat_kernel0.resize(ns*ng*ng);
      escat_kernel1.resize(ns*ng*ng);
    }
    if(do_pair){
      pair_kernel0.resize(ns*ng*ng);
      pair_kernel1.resize(ns*ng*ng);
    }

    cout << "#   logrho range: {" << __nulibtable_MOD_nulibtable_logrho_min << "," << __nulibtable_MOD_nulibtable_logrho_max << "} g/ccm" << endl;
    cout << "#   logT   range: {" << __nulibtable_MOD_nulibtable_logtemp_min << "," << __nulibtable_MOD_nulibtable_logtemp_max << "} MeV" << endl;
    cout << "#   Ye  range: {" << __nulibtable_MOD_nulibtable_ye_min << "," << __nulibtable_MOD_nulibtable_ye_max << "}" << endl;
    cout << "#   E   range: {" << __nulibtable_MOD_nulibtable_ebottom[0] << "," << __nulibtable_MOD_nulibtable_etop[ng-1] << "} MeV" << endl;
    cout << "#   n_species = " << ns << endl;
    cout << "#   n_rho   = " << __nulibtable_MOD_nulibtable_nrho << endl;
    cout << "#   n_T     = " << __nulibtable_MOD_nulibtable_ntemp << endl;
    cout << "#   n_Ye    = " << __nulibtable_MOD_nulibtable_nye << endl;
    cout << "#   n_E     = " << ng << endl;
  }

  void set(double rho_in, double T_in/*MeV*/, double Ye_in){
    // condition inputs, get EOS info
    Ye_in = max(Ye_in,__nulibtable_MOD_nulibtable_ye_min);
    munue = get_munue(rho_in,T_in,Ye_in); // MeV
    double eta = get_eta(rho_in,T_in,Ye_in); // dimensionless
    int n_legendre = 2;

    if(do_interact){
    // BASIC EOS
    nulibtable_range_species_range_energy_(&rho_in, &T_in, &Ye_in, &eas.front(),
					   &__nulibtable_MOD_nulibtable_number_species,
					   &__nulibtable_MOD_nulibtable_number_groups,
					   &__nulibtable_MOD_nulibtable_number_easvariables);

    // INELASTIC SCATTERING KERNELS
    if(do_iscat){
      double phi[n_legendre][ng][ng]; //[a][j][i] = legendre index a, out group i, and in group j (ccm/s)
      for(int lns=1; lns<=__nulibtable_MOD_nulibtable_number_species; lns++){
	nulibtable_inelastic_single_species_range_energy_(&T_in, &eta, &lns, (double*)phi,
							  &ng,&ng,&n_legendre);
	for(int og=0; og<ng; og++)
	  for(int ig=0; ig<ng; ig++){
	    escat_kernel0[kernel_index(lns-1, ig, og)] = phi[0][og][ig];
	    escat_kernel1[kernel_index(lns-1, ig, og)] = phi[1][og][ig];
	  }
      }
    } 

    // PAIR ANNIHILATION KERNELS
    if(do_pair){
      //[a][j][i] = out group i, and in group j (ccm/s)
      // a=0:Phi0a a=1:Phi0p a=2:Phi1a a=2:Phi1p
      int nvars = n_legendre*2;
      double phi[nvars][ng][ng]; 
      for(int lns=1; lns<=__nulibtable_MOD_nulibtable_number_species; lns++){
	nulibtable_epannihil_single_species_range_energy_(&T_in, &eta, &lns, (double*)phi,
							  &ng,&ng,&nvars);
	for(int og=0; og<ng; og++)
	  for(int ig=0; ig<ng; ig++){
	    pair_kernel0[kernel_index(lns-1, ig, og)] = phi[0][og][ig];
	    pair_kernel1[kernel_index(lns-1, ig, og)] = phi[2][og][ig];
	  }
      }
    }
    }
  }
  
  double get_munue(const double rho /* g/ccm */, const double temp /*MeV*/, const double ye){ // MeV
    double eos_variables[__nulib_MOD_total_eos_variables];
    for(int i=0; i<__nulib_MOD_total_eos_variables; i++) eos_variables[i] = 0;
    eos_variables[0] = rho;
    eos_variables[1] = temp;
    eos_variables[2] = ye;
    
    set_eos_variables_(eos_variables);
    double mue = eos_variables[10];
    double muhat = eos_variables[13];
    return (mue-muhat);
  }
  double get_eta(const double rho /* g/ccm */, const double temp /*MeV*/, const double ye){ // dimensionless
    double eos_variables[__nulib_MOD_total_eos_variables];
    for(int i=0; i<__nulib_MOD_total_eos_variables; i++) eos_variables[i] = 0;
    eos_variables[0] = rho;
    eos_variables[1] = temp;
    eos_variables[2] = ye;
    
    set_eos_variables_(eos_variables);
    double mue = eos_variables[10];
    return mue/eos_variables[1];
  }
  
  int index(int is,int ig,int iv){
    return is + ig*ns + iv*ns*ng;
  }

  int kernel_index(int is,int igin, int igout){
    return is + igin*ns + igout*ns*ng;
  }

  double emis(int is,int ig){ // 1/cm/sr
    return abs(is,ig) * fermidirac(is,ig);
  }
  double abs(int is,int ig){ // 1/cm
    return eas[index(is,ig,1)];
  }
  double scat(int is,int ig){ // 1/cm
    return eas[index(is,ig,2)];
  }
  double delta(int is,int ig){ // 1/cm
    return (do_delta ? eas[index(is,ig,3)] : 0);
  }
  double fermidirac(int is, int ig){
    double mu;
    if(is==0)      mu =  munue;
    else if(is==1) mu = -munue;
    else           mu = 0;
    mu *= 1e6*cgs::units::eV;
    double kTerg = temperature * 1e6*cgs::units::eV;
    double result = 1./(1. + exp((E[ig]-mu)/kTerg));
    return result;
  }
  double Phi0scat(int is,int igin, int igout){ // cm^3/s/sr
    double result = 0;
    if(igin == igout)
      result += scat(is,igin)
	/(4.*M_PI*nu[igin]*nu[igin]*dnu[igin]/cgs::constants::c4);
    if(do_iscat)
      result += escat_kernel0[kernel_index(is,igin,igout)];
    return result;
  }
  double Phi1scat(int is,int igin, int igout){ // cm^3/s/sr
    double result = 0;
    if(igin == igout)
      result += scat(is,igin)*delta(is,igin)/3.
	/(4.*M_PI*nu[igin]*nu[igin]*dnu[igin]/cgs::constants::c4);
    if(do_iscat)
      result += escat_kernel1[kernel_index(is,igin,igout)];
    else return 0;
  }
  double Phi0pair(int is,int igin, int igout){ // cm^3/s/sr
    double result = 0;
    if(do_pair)
      result += pair_kernel0[kernel_index(is,igin,igout)];
    return result;
  }
  double Phi1pair(int is,int igin, int igout){ // cm^3/s/sr
    double result = 0;
    if(do_pair)
      result += pair_kernel1[kernel_index(is,igin,igout)];
    else return 0;
  }
};
EAS eas;

#endif
