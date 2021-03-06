/*
//  Copyright (c) 2018, James Kneller and Sherwood Richers
//
//  This file is part of IsotropicSQA.
//
//  IsotropicSQA is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  IsotropicSQA is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with IsotropicSQA.  If not, see <http://www.gnu.org/licenses/>.
//
*/

void evolve_oscillations(State& s, const double rmax, const double accuracy, const double increase){
    
  //********************
  // evolved variables *
  //********************
  array<array<array<array<double,NY>,NS>,NE>,NM> Y;
  for(state m=matter;m<=antimatter;m++)
    for(int i=0;i<NE;i++)
      Y[m][i] = YIdentity;
    
  // temporaries
  array<array<MATRIX<complex<double>,NF,NF>,NE>,NM> SSMSW, SSSI, pmatrixm0;
  array<array<array<array<double,NY>,NS>,NE>,NM>  Ytmp(Y);
  array<array<array<array<array<double,NY>,NS>,NE>,NM>,NRK> Ks;
  
  bool finish=false;
  
  do{ // loop over radii
    getP(s,pmatrixm0);
    
    double dr = s.dr_osc;
    bool repeat=false;
    do{
      double r = s.r;
      if(r+dr>=rmax){
	finish=true;
	dr = rmax-r;
      }
      else finish=false;
      
      // RK integration for oscillation
      for(int k=0;k<=NRK-1;k++){
	Ytmp=Y;
	for(int l=0;l<=k-1;l++)
	  for(int m=0; m<=1; m++) // 0=matter 1=antimatter
	    for(int i=0;i<NE;i++)
	      for(int x=0;x<=1;x++) // 0=msw 1=si
		for(int j=0;j<=NY-1;j++)
		  Ytmp[m][i][x][j] += BB[k][l] * Ks[l][m][i][x][j];
	  
	K(dr,s,pmatrixm0,Ytmp,Ks[k]);
      }
	  
      // increment all quantities from oscillation
      Ytmp = Y;
      double maxerror=0.;
      #pragma omp parallel for collapse(2) reduction(max:maxerror)
      for(int m=matter; m<=antimatter; m++){
	for(int i=0;i<NE;i++){
	  for(int x=msw; x<=si; x++){
	    for(int j=0;j<=NY-1;j++){
	      double Yerror = 0.;
	      for(int k=0;k<=NRK-1;k++){
		Ytmp[m][i][x][j] += CC[k] * Ks[k][m][i][x][j];
		Yerror += (CC[k]-DD[k]) * Ks[k][m][i][x][j];
	      }
	      maxerror = max( maxerror, fabs(Yerror) );
	    } // j
	  } // x
	} // i
      } // m
      
      //==============//
      // TIMESTEPPING //
      //==============//
      r += dr;
      if(maxerror>accuracy){
	dr *= 0.9 * pow(accuracy/maxerror, 1./(NRKOrder-1.));
	repeat=true;
      }
      else{
	dr *= increase;
	repeat = false;
	if(maxerror>0) dr *= min( 1.0, pow(accuracy/maxerror,1./max(1,NRKOrder))/increase );
	Y = Ytmp;
	s.r = r;
	if(!finish) s.dr_osc = dr;
      }
	
    }while(repeat==true); // end of RK section
  
    //========================//
    // RESETTING/ACCUMULATING //
    //========================//
#pragma omp parallel for collapse(2)
    for(int m=matter;m<=antimatter;m++){
      for(int i=0;i<NE;i++){
	
	// get oscillated f
	SSMSW[m][i] = W(Y[m][i][msw])*B(Y[m][i][msw]);
	SSSI [m][i] = W(Y[m][i][si ])*B(Y[m][i][si ]);
	
	// test that the S matrices are close to diagonal
	if(norm(SSMSW[m][i][0][0])+0.1<norm(SSMSW[m][i][0][1]) or
	   norm(SSSI [m][i][0][0])+0.1<norm(SSSI [m][i][0][1]) or
	   finish){
	  MATRIX<complex<double>,NF,NF> SThisStep = s.U0[m][i] * SSMSW[m][i]*SSSI[m][i] * Adjoint(s.U0[m][i]);
	  s.fmatrixf[m][i] = SThisStep * s.fmatrixf[m][i] * Adjoint(SThisStep);
	  Y[m][i] = YIdentity;
	}
	else{ // take modulo 2 pi of phase angles
	  Y[m][i][msw][2]=fmod(Y[m][i][msw][2],M_2PI);
	  Y[m][i][msw][4]=fmod(Y[m][i][msw][4],1.0);
	  Y[m][i][msw][5]=fmod(Y[m][i][msw][5],1.0);
	  
	  Y[m][i][si ][2]=fmod(Y[m][i][si ][2],M_2PI);
	  Y[m][i][si ][4]=fmod(Y[m][i][si ][4],1.0);
	  Y[m][i][si ][5]=fmod(Y[m][i][si ][5],1.0);
	}
      }
    }

  } while(finish==false);
}


void evolve_interactions(State& s, const double rmax, const double accuracy, const double increase){

  array<array<MATRIX<complex<double>,NF,NF>,NE>,NM> ftmp;
  array<array<array<MATRIX<complex<double>,NF,NF>,NE>,NM>,NRK> dfdr;

  bool finish=false;
  do{ // loop over radii
    
    double dr = s.dr_int;
    bool repeat=false;
    do{
      double r = s.r;
      if(r+dr>=rmax){
	finish=true;
	dr = rmax-r;
      }
      else finish=false;

      // RK integration
      for(int k=0;k<=NRK-1;k++){
	ftmp=s.fmatrixf;
	for(int l=0;l<=k-1;l++)
	  for(int m=0; m<=1; m++) // 0=matter 1=antimatter
	    for(int i=0;i<NE;i++)
	      ftmp[m][i] += dfdr[l][m][i] * BB[k][l] * dr;
	
	dfdr[k] = my_interact(ftmp, s.rho, s.T, s.Ye, s.eas);
      }
      
      // increment all quantities from oscillation
      double maxerror=0;
      ftmp = s.fmatrixf;
      #pragma omp parallel for collapse(2) reduction(max:maxerror)
      for(int m=matter; m<=antimatter; m++){
	for(int i=0;i<NE;i++){
	  MATRIX<complex<double>,NF,NF> err, df;
	  for(int k=0;k<=NRK-1;k++){
	    df += dfdr[k][m][i] * CC[k] * dr;
	    err += dfdr[k][m][i] * (CC[k]-DD[k]) * dr;
	  }
	  ftmp[m][i] += df;
	  
	  for(flavour f1=e; f1<=mu; f1++){
	    for(flavour f2=e; f2<=mu; f2++){
	      maxerror = max(maxerror, abs(err[f1][f2])/abs(s.fmatrixf[m][i][f1][f2]));//IsospinL(s.fmatrixf[m][i]));
	    }
	  }
	} // i
      } // m
      
      //==============//
      // TIMESTEPPING //
      //==============//
      r+=dr;
      if(maxerror>accuracy){
	dr *= 0.9 * pow(accuracy/maxerror, 1./(NRKOrder-1.));
	repeat=true;
      }
      else{
	dr *= increase;
	repeat = false;
	if(maxerror>0) dr *= min( 1.0, pow(accuracy/maxerror,1./max(1,NRKOrder))/increase );
	s.r = r;
	s.fmatrixf = ftmp;
	if(!finish) s.dr_int = dr;
      }

    }while(repeat==true); // end of RK section
    
  }while(finish==false);

}
