
#include <complex>
using std::complex;
using std::polar;
using std::abs;
using std::arg;
using std::real;
using std::imag;
#include <cstdarg>
using std::va_list;
#include <cstdlib>

#include<iostream>
using std::cout;
#include<ostream>
using std::ostream;
using std::endl;
using std::flush;
#include<fstream>
using std::ifstream;
using std::ofstream;
#include<sstream>
using std::stringstream;

#include<algorithm>
using std::min;
using std::max;
using std::swap;
using std::lower_bound;
#include<string>
using std::string;
#include <utility>
using std::pair;

#include<functional>
#include<limits>
using std::numeric_limits;

#include<vector>
using std::vector;

#include "headers/matrix.h"

// global variables
double rho, Ye, temperature/*MeV*/; // rho is the mass density
int do_oscillate, do_interact;

// headers
#include "headers/parameters.h"
double Ve(double rho, double Ye){
  return (M_SQRT2*cgs::constants::GF/cgs::constants::Mp)*rho*Ye;
}
double Vmu(double rho, double Ye){ return 0.;}
#include "headers/flavour basis.h"
#include "headers/eigenvalues.h"
#include "headers/mixing angles.h"
#include "headers/adiabatic basis.h"
#include "headers/jacobians.h"

vector<MATRIX<complex<double>,NF,NF> > pmatrixm0matter(NM);
vector<vector<MATRIX<complex<double>,NF,NF> > > fmatrixf(NM);

MATRIX<complex<double>,NF,NF> B(vector<double> y);
void K(double r,
       double dr,
       vector<vector<vector<vector<double> > > > &Y,
       vector<vector<vector<MATRIX<complex<double>,NF,NF> > > > &C0,
       vector<vector<vector<vector<double> > > > &A0,
       vector<vector<vector<vector<double> > > > &K);
void Outputvsr(ofstream &foutf, double r);

#include "headers/update.h"
#include "headers/interact.h"

void getP(const double r,
	  const vector<vector<MATRIX<complex<double>,NF,NF> > > U0, 
	  vector<MATRIX<complex<double>,NF,NF> >& pmatrixm0matter){

  for(int i=0;i<=NE-1;i++){
    pmatrixm0matter[i] = Adjoint(U0[matter][i])
      * (fmatrixf[matter][i] - Conjugate(fmatrixf[antimatter][i]) )
      * U0[matter][i];
    pmatrixm0matter[i] *= M_SQRT2*cgs::constants::GF /* erg cm^3*/
      * 4.*M_PI*nu[i]*nu[i]*dnu[i]/*Hz^3*/ / pow(cgs::constants::c,3)/*cm^3 Hz^3*/;
  }
}

#include "headers/nulib_interface.h"

//======//
// MAIN //
//======//
int main(int argc, char *argv[]){
    int in=1;
    string inputfilename;
    ofstream foutf;
    string outputfilename,vfilename, spectrapath, nulibfilename, eosfilename;
    string outputfilenamestem;
    string note;
    
    inputfilename=string(argv[in]);
    ifstream fin(inputfilename.c_str());
    
    // load the nulib table
    fin>>nulibfilename;
    cout << nulibfilename << endl;
    fin >> eosfilename;
    cout << eosfilename << endl;
    eas = EAS(nulibfilename, eosfilename);

    fin>>rho;
    fin>>Ye;
    fin>>temperature;
    fin>>outputfilename;
    outputfilenamestem = outputfilename+"/";
    
    double rmax;
    fin>>rmax; // s
    rmax *= cgs::constants::c; // s -> cm
    m1=0.;
    fin>>dm21;
    fin>>theta12V;
    
    alphaV[0]=0.;
    alphaV[1]=0.;
    
    betaV[0]=0.;
    
    double accuracy;
    fin>>accuracy;
    fin>>note;

    cout<<"\n\n*********************************************************\n";
    cout<<"\nrho\t"<<rho;
    cout<<"\nYe\t"<<Ye;
    cout<<"\nT\t"<<temperature;
    cout<<"\noutput\t"<<outputfilename;
    cout<<"\ttmax\t"<<rmax/cgs::constants::c;
    cout<<"\n\nm1\t"<<m1<<"\tdm21^2\t"<<dm21;
    cout<<"\ntheta12V\t"<<theta12V;
    cout<<"\nalpha1V\t"<<alphaV[0]<<"\talpha2V\t"<<alphaV[1];
    cout<<"\nbeta1V\t"<<betaV[0];    
    cout<<"\naccuracy\t"<<accuracy<<"\n";
    cout.flush();
    
    // output filestreams: the arrays of ofstreams cannot use the vector container - bug in g++
    foutf.open((outputfilename+"/f.dat").c_str());
    foutf.precision(12);
    foutf << "# 1:r ";
    for(int i=0; i<NE; i++)
      for(state m=matter; m<=antimatter; m++)
	for(flavour f1=e; f1<=mu; f1++)
	  for(flavour f2=e; f2<=mu; f2++) {
	    int istart = 2*( f2 + f1*2 + m*2*2 + i*2*2*2) + 2;
	    foutf << istart   << ":ie"<<i<<"m"<<m<<"f"<<f1<<f2<<"R\t";
	    foutf << istart+1 << ":ie"<<i<<"m"<<m<<"f"<<f1<<f2<<"I\t";
    }
    foutf << endl;
    foutf.flush();
    
    // unit conversion to cgs
    //Emin *= 1.*mega*cgs::units::eV;
    //Emax *= 1.*mega*cgs::units::eV;
    m1   *= 1.*cgs::units::eV/cgs::constants::c2;
    dm21 *= 1.*cgs::units::eV*cgs::units::eV/cgs::constants::c4;
    theta12V *= M_PI/180.;
    c12V = cos(theta12V);
    s12V = sin(theta12V);
    
    // *************************************************
    // set up global variables defined in parameters.h *
    // *************************************************

    // vectors of energies and vacuum eigenvalues
    set_Ebins(E);
    kV = vector<vector<double> >(NE,vector<double>(NF));
    set_kV(kV);

    // determine eigenvalue ordering
    if(kV[0][1]>kV[0][0]){
      a1=-1;
      a2=+1;
      cout<<"\n\nNormal hierarchy" << endl;
    }
    else{ 
      if(kV[0][1]<kV[0][0]){
	a1=+1; 
	a2=-1; 
	cout<<"\n\nInverted hierarchy" << endl;
      }
      else{ 
	cout<<endl<<endl<<"Neither normal or Inverted"<<endl; 
	abort();
      }
    }
    
    // vaccum mixing matrices and Hamiltonians
    Evaluate_UV();
    
    HfV[matter] = vector<MATRIX<complex<double>,NF,NF> >(NE);
    HfV[antimatter] = vector<MATRIX<complex<double>,NF,NF> >(NE);
    Evaluate_HfV();
    
    // cofactor matrices in vacuum
    CV=vector<vector<MATRIX<complex<double>,NF,NF> > >(NE,vector<MATRIX<complex<double>,NF,NF> >(NF));
    Evaluate_CV();
    
    // mixing matrix element prefactors in vacuum
    AV=vector<vector<vector<double> > >(NE,vector<vector<double> >(NF,vector<double>(NF)));
    Evaluate_AV();
    
    // **************************************
    // quantities evaluated at inital point *
    // **************************************
    
    // MSW potential matrix
    MATRIX<complex<double>,NF,NF> VfMSW0, Hf0;
    vector<double> k0, deltak0;
    
    VfMSW0[e][e]=Ve(rho,Ye);
    VfMSW0[mu][mu]=Vmu(rho,Ye);
    
    // cofactor matrices at initial point - will be recycled as cofactor matrices at beginning of every step
    vector<vector<vector<MATRIX<complex<double>,NF,NF> > > > 
      C0(NM,vector<vector<MATRIX<complex<double>,NF,NF> > >(NE,vector<MATRIX<complex<double>,NF,NF> >(NF)));

    // mixing matrix element prefactors at initial point - will be recycled like C0
    vector<vector<vector<vector<double> > > > 
      A0(NM,vector<vector<vector<double> > >(NE,vector<vector<double> >(NF,vector<double>(NF))));
    
    // mixing angles to MSW basis at initial point
    U0[matter] = vector<MATRIX<complex<double>,NF,NF> >(NE);
    U0[antimatter] = vector<MATRIX<complex<double>,NF,NF> >(NE);
    
    for(int i=0;i<=NE-1;i++){
      Hf0=HfV[matter][i]+VfMSW0;
      k0=k(Hf0);
      deltak0=deltak(Hf0);
      C0[matter][i]=CofactorMatrices(Hf0,k0);
      
      for(int j=0;j<=NF-1;j++){
	if(real(C0[matter][i][j][mu][e]*CV[i][j][mu][e]) < 0.)
	  A0[matter][i][j][e]=-AV[i][j][e];
	else A0[matter][i][j][e]=AV[i][j][e];
	A0[matter][i][j][mu]=AV[i][j][mu];
      }
      U0[matter][i]=U(deltak0,C0[matter][i],A0[matter][i]);
      
      Hf0=HfV[antimatter][i]-VfMSW0;
      k0=kbar(Hf0);
      deltak0=deltakbar(Hf0);
      C0[antimatter][i]=CofactorMatrices(Hf0,k0);
      for(int j=0;j<=NF-1;j++){
	if(real(C0[antimatter][i][j][mu][e]*CV[i][j][mu][e]) < 0.)
	  A0[antimatter][i][j][e]=-AV[i][j][e];
	else A0[antimatter][i][j][e]=AV[i][j][e];
	A0[antimatter][i][j][mu]=AV[i][j][mu];
      }
      U0[antimatter][i]=Conjugate(U(deltak0,C0[antimatter][i],A0[antimatter][i]));
    }
    
    // density matrices at initial point, rhomatrixm0 - but not rhomatrixf0
    // will be updated whenever discontinuities are crossed and/or S is reset
    pmatrixm0matter=vector<MATRIX<complex<double>,NF,NF> >(NE);
    fmatrixf[matter]=fmatrixf[antimatter]=vector<MATRIX<complex<double>,NF,NF> >(NE);

    // yzhu14 density/potential matrices art rmin
    double mixing;
    fin>>mixing;

    // ***************************************
    // quantities needed for the calculation *
    // ***************************************
    double r,r0,dr,drmin;
    double maxerror,increase=3.;
    bool repeat, finish, resetflag, output;
    int counterout,step;
    
    // comment out if not following as a function of r
    fin>>step;

    // do we oscillate and interact?
    fin>>do_oscillate;
    fin>>do_interact;
    initialize(fmatrixf,0,rho,temperature,Ye, mixing);
    
    // ***************************************
    // variables followed as a function of r *
    // ***************************************
    
    vector<vector<vector<vector<double> > > > 
      Y(NM,vector<vector<vector<double> > >(NE,vector<vector<double> >(NS,vector<double>(NY))));
    vector<vector<vector<vector<double> > > > 
      Y0(NM,vector<vector<vector<double> > >(NE,vector<vector<double> >(NS,vector<double>(NY))));
    
    // cofactor matrices
    vector<vector<vector<MATRIX<complex<double>,NF,NF> > > > C=C0;;
    // mixing matrix prefactors
    vector<vector<vector<vector<double> > > > A=A0;
        
    // ************************
    // Runge-Kutta quantities *
    // ************************
    int NRK,NRKOrder;
    const double *AA=NULL,**BB=NULL,*CC=NULL,*DD=NULL;
    RungeKuttaCashKarpParameters(NRK,NRKOrder,AA,BB,CC,DD);
    
    vector<vector<vector<vector<vector<double> > > > > 
      Ks(NRK,vector<vector<vector<vector<double> > > >
	 (NM,vector<vector<vector<double> > >(NE,vector<vector<double> >(NS,vector<double>(NY)))));
    
    // temporaries
    MATRIX<complex<double>,NF,NF> SSMSW,SSSI,SThisStep;
    vector<vector<MATRIX<complex<double>,NF,NF> > > fmatrixf0(NM);
    vector<vector<MATRIX<complex<double>,NF,NF> > > ftmp0, dfdr0, dfdr1;

    // **********************
    // start of calculation *
    // **********************
    
      cout << "t(s)  dt(s)  n_nu(1/ccm)  n_nubar(1/ccm) n_nu-n_nubar(1/ccm)" << endl;
      cout.flush();
      
      // *****************************************
      // initialize at beginning of every domain *
      // *****************************************
      r=0;
      dr=1e-3*cgs::units::cm;
      drmin=4.*r*numeric_limits<double>::epsilon();
      
      for(state m=matter;m<=antimatter;m++)
	for(int i=0;i<=NE-1;i++){
	  Y0[m][i][msw][0] = Y0[m][i][si][0] = M_PI/2.;
	  Y0[m][i][msw][1] = Y0[m][i][si][1] = M_PI/2.;
	  Y0[m][i][msw][2] = Y0[m][i][si][2] = 0.;
	  Y0[m][i][msw][3] = Y0[m][i][si][3] = 1.; // The determinant of the S matrix
	  Y0[m][i][msw][4] = Y0[m][i][si][4] = 0.;
	  Y0[m][i][msw][5] = Y0[m][i][si][5] = 0.;
	}
      Y=Y0;
      
      // *************************************************
      // comment out if not following as a function of r *
      // *************************************************
	
      finish=output=false;
      counterout=1;
      Outputvsr(foutf,r);
	
      for(state m=matter; m<=antimatter; m++)
	for(int i=0; i<NE; i++)
	  for(flavour f1=e; f1<=mu; f1++)
	    for(flavour f2=e; f2<=mu; f2++)
	      assert(fmatrixf[m][i][f1][f2] == fmatrixf[m][i][f1][f2]);
      
      // ***********************
      // start the loop over r *
      // ***********************
      double n0,nbar0;
      do{ 

	// output to stdout
	double intkm = int(r/1e5)*1e5;
	if(r - intkm <= dr){
	  double n=0, nbar=0;
	  double coeff = 4.*M_PI / pow(cgs::constants::c,3);
	  for(int i=0; i<NE; i++){
	    for(flavour f1=e; f1<=mu; f1++){
	      n    += real(fmatrixf[    matter][i][f1][f1]) * nu[i]*nu[i]*dnu[i]*coeff;
	      nbar += real(fmatrixf[antimatter][i][f1][f1]) * nu[i]*nu[i]*dnu[i]*coeff;
	    }
	  }
	  if(r==0){
	    n0=n;
	    nbar0=nbar;
	  }
	  cout << r/cgs::constants::c << " ";
	  cout << dr/cgs::constants::c << " ";
	  cout << n/n0 << " " << nbar/nbar0 << " " << (n-nbar)/(n0-nbar0) << endl;
	  cout.flush();
	}
	  
	// save initial values in case of repeat
	r0=r;
	C0=C;
	A0=A;
	fmatrixf0 = fmatrixf;
	getP(r,U0,pmatrixm0matter);

	// beginning of RK section
	do{ 
	  repeat=false;
	  maxerror=0.;

	  if(do_oscillate){
	    // RK integration for oscillation
	    for(int k=0;k<=NRK-1;k++){
	      r=r0+AA[k]*dr;
	      Y=Y0;
	      for(state m = matter; m <= antimatter; m++)
		for(int i=0;i<=NE-1;i++)
		  for(solution x=msw;x<=si;x++)
		    for(int j=0;j<=NY-1;j++)
		      for(int l=0;l<=k-1;l++)
			Y[m][i][x][j] += BB[k][l] * Ks[l][m][i][x][j];

	      K(r,dr,Y,C,A,Ks[k]);
	    }
	  
	    // increment all quantities from oscillation
	    getP(r0+dr,U0,pmatrixm0matter);
	    Y=Y0;
	    for(state m=matter;m<=antimatter;m++){
	      for(int i=0;i<=NE-1;i++){

		// integrate Y
		for(solution x=msw;x<=si;x++){
		  for(int j=0;j<=NY-1;j++){
		    double Yerror = 0.;
		    for(int k=0;k<=NRK-1;k++){
		      assert(CC[k] == CC[k]);
		      assert(Ks[k][m][i][x][j] == Ks[k][m][i][x][j]);
		      Y[m][i][x][j] += CC[k] * Ks[k][m][i][x][j];
		      Yerror += (CC[k]-DD[k]) * Ks[k][m][i][x][j];
		      assert(Y[m][i][x][j] == Y[m][i][x][j]);
		    }
		    maxerror = max( maxerror, fabs(Yerror) );
		  }
		}
	      
		// convert fmatrix from flavor basis to mass basis, oscillate, convert back
		SSMSW = W(Y[m][i][msw])*B(Y[m][i][msw]);
		SSSI  = W(Y[m][i][si ])*B(Y[m][i][si ]);
		SThisStep = U0[m][i] * SSMSW*SSSI * Adjoint(U0[m][i]);
		fmatrixf[m][i] = MATRIX<complex<double>,NF,NF>
		  (SThisStep * fmatrixf[m][i] * Adjoint(SThisStep));
	      }
	    }
	    Y=Y0;
	    C=UpdateC(r,Ye);
	    A=UpdateA(C,C0,A0);
	  }
	  r=r0+dr;

	  if(do_interact){
	    // interact with the matter
	    // if fluid changes with r, update opacities here, too
	    dfdr0 = my_interact(fmatrixf, rho, temperature, Ye);
	    ftmp0 = fmatrixf;
	    for(state m=matter; m<=antimatter; m++)
	      for(int i=0; i<NE; i++)
		for(flavour f1=e; f1<=mu; f1++)
		  for(flavour f2=e; f2<=mu; f2++)
		    ftmp0[m][i][f1][f2] += dfdr0[m][i][f1][f2] * dr;
	    dfdr1 = my_interact(ftmp0, rho, temperature, Ye);
	    for(state m=matter; m<=antimatter; m++)
	      for(int i=0; i<NE; i++){
		double trace = real(fmatrixf[m][i][e][e]+fmatrixf[m][i][mu][mu]);
		for(flavour f1=e; f1<=mu; f1++)
		  for(flavour f2=e; f2<=mu; f2++){
		    fmatrixf[m][i][f1][f2] += 0.5*dr
		      * (dfdr0[m][i][f1][f2] + dfdr1[m][i][f1][f2]);
		    maxerror = max(fabs(ftmp0[m][i][f1][f2]-fmatrixf[m][i][f1][f2])/trace,
				   maxerror);
		  }
	      }
	  }
	  
	  // decide whether to accept step, if not adjust step size
	  if(maxerror>accuracy){
	    dr *= 0.9 * pow(accuracy/maxerror, 1./(NRKOrder-1.));
	    if(dr > drmin) repeat=true;
	  }
	  if(maxerror>0)
	    dr = min(dr*pow(accuracy/maxerror,1./max(1,NRKOrder)),increase*dr);
	  else dr *= increase;
	  drmin = 4.*r*numeric_limits<double>::epsilon();
	  dr = max(dr,drmin);
	  if(r+dr > rmax) dr = rmax-r;
	  
	  // reset integration variables to those at beginning of step
	  if(repeat==true){
	    r=r0;
	    C=C0;
	    A=A0;
	    fmatrixf = fmatrixf0;
	  }
	  
	}while(repeat==true); // end of RK section


	// comment out if not following as a function of r
	if(r>=rmax){
	  finish=true;
	  output=true;
	}
	if(counterout==step){
	  output=true;
	  counterout=1;
	}
	else counterout++;
	if(output==true || finish==true){
	  Outputvsr(foutf,r);
	  output=false;
	}

      } while(finish==false);

  cout<<"\nFinished\n\a"; cout.flush();

  return 0;
}


//===//
// B //
//===//
MATRIX<complex<double>,NF,NF> B(vector<double> y){
  MATRIX<complex<double>,NF,NF> s;
  double cPsi1=cos(y[0]),sPsi1=sin(y[0]), cPsi2=cos(y[1]),sPsi2=sin(y[1]), cPsi3=cos(y[2]),sPsi3=sin(y[2]);
  
  s[0][1] = cPsi1 + I*sPsi1*cPsi2;
  sPsi1 *= sPsi2;
  s[0][0] = sPsi1 * (cPsi3 + I*sPsi3);

  s[1][0] = -y[3]*conj(s[0][1]);
  s[1][1] =  y[3]*conj(s[0][0]);

  return s;
}

//===//
// K //
//===//
void K(double r,
       double dr,
       vector<vector<vector<vector<double> > > > &Y,
       vector<vector<vector<MATRIX<complex<double>,NF,NF> > > > &C0,
       vector<vector<vector<vector<double> > > > &A0,
       vector<vector<vector<vector<double> > > > &K){

  MATRIX<complex<double>,NF,NF> VfSI,VfSIbar;  // self-interaction potential
  vector<MATRIX<complex<double>,NF,NF> > VfSIE(NE); // contribution to self-interaction potential from each energy
  MATRIX<complex<double>,NF,NF> VfMSW,VfMSWbar;
  MATRIX<complex<double>,NF,NF> Hf,Hfbar, UU,UUbar;
  vector<double> kk,kkbar, dkk,dkkbar;
  vector<MATRIX<complex<double>,NF,NF> > CC;
  vector<vector<double> > AA;
  MATRIX<complex<double>,NF,NF> BB,BBbar;
  MATRIX<complex<double>,NF,NF> Sfm,Sfmbar;
  vector<vector<MATRIX<complex<double>,NF,NF> > > 
    Sa(NE,vector<MATRIX<complex<double>,NF,NF> >(NS)), 
    Sabar(NE,vector<MATRIX<complex<double>,NF,NF> >(NS));
  vector<MATRIX<complex<double>,NF,NF> > UWBW(NE);
  vector<MATRIX<complex<double>,NF,NF> > UWBWbar(NE);
  double drhodr,dYedr;
  MATRIX<double,3,4> JI;
  int i;
  MATRIX<complex<double>,NF,NF> Ha;
  MATRIX<complex<double>,NF,NF> HB;
  vector<double> dvdr(4);
  // *************
  drhodr=0;
  dYedr=0;
  VfMSW[e][e]=Ve(rho,Ye);
  VfMSW[mu][mu]=Vmu(rho,Ye);
  VfMSWbar=-Conjugate(VfMSW);

#pragma omp parallel for schedule(auto) private(Hf,Hfbar,UU,UUbar,kk,kkbar,dkk,dkkbar,AA,CC,BB,BBbar,Sfm,Sfmbar)
  for(i=0;i<=NE-1;i++){
    Hf  = HfV[matter][i]+VfMSW;
    kk  = k(Hf);
    dkk = deltak(Hf);
    CC  = CofactorMatrices(Hf,kk);
    AA  = MixingMatrixFactors(CC,C0[matter][i],A0[matter][i]);
    UU  = U(dkk,CC,AA);
    BB  = B(Y[matter][i][msw]);
    Sa[i][si] = B(Y[matter][i][si]);
    UWBW[i] = UU * W(Y[matter][i][msw]) * BB * W(Y[matter][i][si]);
    
    Hfbar = HfV[antimatter][i] + VfMSWbar;
    kkbar = kbar(Hfbar);
    dkkbar = deltakbar(Hfbar);
    CC = CofactorMatrices(Hfbar,kkbar);
    AA = MixingMatrixFactors(CC,C0[antimatter][i],A0[antimatter][i]);
    UUbar = Conjugate(U(dkkbar,CC,AA));
    BBbar = B(Y[antimatter][i][msw]);
    Sabar[i][si] = B(Y[antimatter][i][si]);
    UWBWbar[i] = UUbar * W(Y[antimatter][i][msw]) *BBbar * W(Y[antimatter][i][si]);
    
    // ****************
    // Matter section *
    // ****************
    for(int j=0;j<=3;j++)
      K[matter][i][msw][j]=0.;
    
    K[matter][i][msw][4] = kk[0]*dr/M_2PI/cgs::constants::hbarc;
    K[matter][i][msw][5] = kk[1]*dr/M_2PI/cgs::constants::hbarc;

    // ********************
    // Antimatter section *
    // ********************
    for(int j=0;j<=3;j++)
      K[antimatter][i][msw][j] = 0.; 

    K[antimatter][i][msw][4] = kkbar[0]*dr/M_2PI/cgs::constants::hbarc;
    K[antimatter][i][msw][5] = kkbar[1]*dr/M_2PI/cgs::constants::hbarc;

    // *****************************************************************
    // contribution to the self-interaction potential from this energy *
    // *****************************************************************
    Sfm    = UWBW   [i]*Sa   [i][si];
    Sfmbar = UWBWbar[i]*Sabar[i][si];
    VfSIE[i] =     Sfm   *pmatrixm0matter[i]*Adjoint(Sfm   );
  }//end for loop over i

  // ************************************
  // compute self-interaction potential *
  // ************************************
  for(i=0;i<=NE-1;i++){
    VfSI[e ][e ]+=VfSIE[i][e ][e ];
    VfSI[e ][mu]+=VfSIE[i][e ][mu];
    VfSI[mu][e ]+=VfSIE[i][mu][e ];
    VfSI[mu][mu]+=VfSIE[i][mu][mu];
  }
  VfSIbar=-Conjugate(VfSI);

  // *********************
  // SI part of solution *
  // *********************

#pragma omp parallel for schedule(auto) private(JI) firstprivate(Ha,HB,dvdr)
  for(i=0;i<=NE-1;i++){
    //*********
    // Matter *
    //*********
    Ha = Adjoint(UWBW[i])*VfSI*UWBW[i];

    K[matter][i][si][4]=dr*real(Ha[0][0])/(M_2PI*cgs::constants::hbarc);
    K[matter][i][si][5]=dr*real(Ha[1][1])/(M_2PI*cgs::constants::hbarc);
    
    HB[0][0]=-I/cgs::constants::hbarc*( Ha[0][1]*Sa[i][si][1][0] );
    HB[0][1]=-I/cgs::constants::hbarc*( Ha[0][1]*Sa[i][si][1][1] );
    
    dvdr[0]=real(HB[0][1]);
    dvdr[1]=imag(HB[0][1]);
    dvdr[2]=real(HB[0][0]);
    dvdr[3]=imag(HB[0][0]);
    
    JI=JInverse(Y[matter][i][si]);
    
    for(int j=0;j<=2;j++){
      K[matter][i][si][j]=0.;
      for(int k=j;k<=3;k++) K[matter][i][si][j]+=JI[j][k]*dvdr[k];
      K[matter][i][si][j]*=dr;
    }
    
    K[matter][i][si][3]=0.;
    
    //*************
    // Antimatter *
    //*************
    Ha=Adjoint(UWBWbar[i])*VfSIbar*UWBWbar[i];

    K[antimatter][i][si][4]=dr*real(Ha[0][0])/(M_2PI*cgs::constants::hbarc);
    K[antimatter][i][si][5]=dr*real(Ha[1][1])/(M_2PI*cgs::constants::hbarc);

    HB[0][0]=-I/cgs::constants::hbarc*( Ha[0][1]*Sabar[i][si][1][0] );
    HB[0][1]=-I/cgs::constants::hbarc*( Ha[0][1]*Sabar[i][si][1][1] );

    dvdr[0]=real(HB[0][1]);
    dvdr[1]=imag(HB[0][1]);
    dvdr[2]=real(HB[0][0]);
    dvdr[3]=imag(HB[0][0]);

    JI = JInverse(Y[antimatter][i][si]);

    for(int j=0;j<=2;j++){
      K[antimatter][i][si][j]=0.;
      for(int k=j;k<=3;k++) K[antimatter][i][si][j]+=JI[j][k]*dvdr[k];
      K[antimatter][i][si][j]*=dr;
    }

    K[antimatter][i][si][3]=0.;
  }

}// end of K function


//===========//
// Outputvsr //
//===========//
void Outputvsr(ofstream &foutf, double r){
  foutf << r << "\t";
  for(int i=0; i<NE; i++)
    for(state m=matter; m<=antimatter; m++)
      for(flavour f1=e; f1<=mu; f1++)
	for(flavour f2=e; f2<=mu; f2++) {
	  foutf << real( fmatrixf[m][i][f1][f2] ) << "\t";
	  foutf << imag( fmatrixf[m][i][f1][f2] ) << "\t";
	}
  foutf << endl;
  foutf.flush();
}
