/*
  bhlight3d model specification routines 
*/

#include "decs.h"

#define NVAR (10)

/*
  bhlight 3d grid functions
*/
double ****bcon;
double ****bcov;
double ****ucon;
double ****ucov;
double ****p;
double ***ne;
double ***thetae;
double ***b;

void interp_fourv(double X[NDIM], double ****fourv, double Fourv[NDIM]) ;
double interp_scalar(double X[NDIM], double ***var) ;
static double poly_norm, poly_xt, poly_alpha, mks_smooth;
static double game, gamp;

static double MBH, Mdotedd;

void set_dxdX(double X[NDIM], double dxdX[NDIM][NDIM])
{
  //memset(dxdX, 0, NDIM*NDIM*sizeof(double));
  MUNULOOP dxdX[mu][nu] = 0.;
  //MULOOP dxdX[mu][mu] = 1.;
  //return;

  /*for (int mu = 0; mu < NDIM; mu++) {
    for (int nu = 0; nu < NDIM; nu++) {
      dxdX[mu][nu] = 0.;
    }
  }*/

  dxdX[0][0] = 1.;
  dxdX[1][1] = exp(X[1]);
  dxdX[2][1] = -exp(mks_smooth*(startx[1]-X[1]))*mks_smooth*(
    M_PI/2. - 
    M_PI*X[2] + 
    poly_norm*(2.*X[2]-1.)*(1+(pow((-1.+2*X[2])/poly_xt,poly_alpha))/(1 + poly_alpha)) - 
    1./2.*(1. - hslope)*sin(2.*M_PI*X[2])
    );
  dxdX[2][2] = M_PI + (1. - hslope)*M_PI*cos(2.*M_PI*X[2]) + 
    exp(mks_smooth*(startx[1]-X[1]))*(
      -M_PI + 
      2.*poly_norm*(1. + pow((2.*X[2]-1.)/poly_xt,poly_alpha)/(poly_alpha+1.)) + 
      (2.*poly_alpha*poly_norm*(2.*X[2]-1.)*pow((2.*X[2]-1.)/poly_xt,poly_alpha-1.))/((1.+poly_alpha)*poly_xt) - 
      (1.-hslope)*M_PI*cos(2.*M_PI*X[2])
      );
  dxdX[3][3] = 1.;
}

void gcov_func(double X[NDIM], double gcov[NDIM][NDIM])
{
  //memset(gcov, 0, NDIM*NDIM*sizeof(double));
  MUNULOOP gcov[mu][nu] = 0.;
  /*for (int mu = 0; mu < NDIM; mu++) {
    for (int nu = 0; nu < NDIM; nu++) {
      gcov[mu][nu] = 0.;
    }
  }*/

  double sth, cth, s2, rho2;
  double r, th;

  bl_coord(X, &r, &th);

  cth = cos(th);
  sth = sin(th);

  s2 = sth*sth;
  rho2 = r*r + a*a*cth*cth;

  gcov[0][0] = -1. + 2.*r/rho2;
  gcov[0][1] = 2.*r/rho2;
  gcov[0][3] = -2.*a*r*s2/rho2;

  gcov[1][0] = gcov[0][1];
  gcov[1][1] = 1. + 2.*r/rho2;
  gcov[1][3] = -a*s2*(1. + 2.*r/rho2);

  gcov[2][2] = rho2;

  gcov[3][0] = gcov[0][3];
  gcov[3][1] = gcov[1][3];
  gcov[3][3] = s2*(rho2 + a*a*s2*(1. + 2.*r/rho2));

  // Apply coordinate transformation to code coordinates X
  double dxdX[NDIM][NDIM];
  set_dxdX(X, dxdX);

  double gcov_ks[NDIM][NDIM];
  //memcpy(gcov_ks, gcov, NDIM*NDIM*sizeof(double));
  //memset(gcov, 0, NDIM*NDIM*sizeof(double));
  /*for (int mu = 0; mu < NDIM; mu++) {
    for (int nu = 0; nu < NDIM; nu++) {
      gcov_ks[mu][nu] = gcov[mu][nu];
      gcov[mu][nu] = 0.;
    }
  }*/
  MUNULOOP {
    gcov_ks[mu][nu] = gcov[mu][nu];
    gcov[mu][nu] = 0.;
  }
 

  for (int mu = 0; mu < NDIM; mu++) {
    for (int nu = 0; nu < NDIM; nu++) {
      for (int lam = 0; lam < NDIM; lam++) {
        for (int kap = 0; kap < NDIM; kap++) {
          gcov[mu][nu] += gcov_ks[lam][kap]*dxdX[lam][mu]*dxdX[kap][nu];
        }
      }
    }
  }
}

void get_connection(double X[4], double lconn[4][4][4])
{
  get_connection_num(X, lconn);
}

void init_model(char *args[])
{
  void init_bhlight3d_grid(char *);
  void init_bhlight3d_data(char *);

  fprintf(stderr, "reading data header...\n");
  /* Read in header and allocate space for grid data */
  init_bhlight3d_grid(args[3]);
  fprintf(stderr, "success\n");

  /* find dimensional quantities from black hole
    mass and its accretion rate */
  set_units(args[4]);

  fprintf(stderr, "reading data...\n");
  /* Read in the grid data */
  init_bhlight3d_data(args[3]);
  fprintf(stderr, "success\n");

  /* pre-compute densities, field strengths, etc. */
  init_physical_quantities() ;

  /* horizon radius */
  Rh = 1 + sqrt(1. - a * a) ;

}

/* 

  these supply basic model data to grmonty

*/

void get_model_ucov(double X[NDIM], double Ucov[NDIM])
{
  double gcov[NDIM][NDIM];

  gcov_func(X, gcov);

  if(X[1] < startx[1] || 
     X[1] > stopx[1]  || 
     X[2] < startx[2] || 
     X[2] > stopx[2]) {
     
      /* sensible default value */
    Ucov[0] = -1./sqrt(-gcov[0][0]) ;
    Ucov[1] = 0. ;
    Ucov[2] = 0. ;
    Ucov[3] = 0. ;

    return ;
  }

  //get_model_ucon(X, Ucon);
  //lower(Ucon, gcov, Ucov);

  interp_fourv(X, ucov, Ucov) ;

}

void get_model_ucon(double X[NDIM], double Ucon[NDIM])
{

  double gcov[NDIM][NDIM] ;
  double gcon[NDIM][NDIM] ;
  double tmp[NDIM] ;

  if(X[1] < startx[1] || 
     X[1] > stopx[1]  || 
     X[2] < startx[2] || 
     X[2] > stopx[2]) {
      /* sensible default value */
      gcov_func(X, gcov) ;

    tmp[0] = -1./sqrt(-gcov[0][0]) ;
    tmp[1] = 0. ;
    tmp[2] = 0. ;
    tmp[3] = 0. ;

      gcon_func(gcov, gcon) ;
    Ucon[0] = 
      tmp[0]*gcon[0][0] +
      tmp[1]*gcon[0][1] +
      tmp[2]*gcon[0][2] +
      tmp[3]*gcon[0][3] ;
    Ucon[1] = 
      tmp[0]*gcon[1][0] +
      tmp[1]*gcon[1][1] +
      tmp[2]*gcon[1][2] +
      tmp[3]*gcon[1][3] ;
    Ucon[2] = 
      tmp[0]*gcon[2][0] +
      tmp[1]*gcon[2][1] +
      tmp[2]*gcon[2][2] +
      tmp[3]*gcon[2][3] ;
    Ucon[3] = 
      tmp[0]*gcon[3][0] +
      tmp[1]*gcon[3][1] +
      tmp[2]*gcon[3][2] +
      tmp[3]*gcon[3][3] ;
  
    return ;
  }
     
  interp_fourv(X, ucon, Ucon) ;
}

void get_model_bcov(double X[NDIM], double Bcov[NDIM])
{
  if(X[1] < startx[1] || 
     X[1] > stopx[1]  || 
     X[2] < startx[2] || 
     X[2] > stopx[2]) {

      Bcov[0] = 0. ;
      Bcov[1] = 0. ;
      Bcov[2] = 0. ;
      Bcov[3] = 0. ;

    return ;
  }
  interp_fourv(X, bcov, Bcov) ;
}

void get_model_bcon(double X[NDIM], double Bcon[NDIM])
{
  if(X[1] < startx[1] || 
     X[1] > stopx[1]  || 
     X[2] < startx[2] || 
     X[2] > stopx[2]) {

      Bcon[0] = 0. ;
      Bcon[1] = 0. ;
      Bcon[2] = 0. ;
      Bcon[3] = 0. ;

    return ;
  }
  interp_fourv(X, bcon, Bcon) ;
}

double get_model_thetae(double X[NDIM])
{
  if(X[1] < startx[1] || 
     X[1] > stopx[1]  || 
     X[2] < startx[2] || 
     X[2] > stopx[2]) {
      return(0.) ;
  }
  
  return(interp_scalar(X, thetae)) ;
}

//b field strength in Gauss
double get_model_b(double X[NDIM])
{

  if(X[1] < startx[1] || 
     X[1] > stopx[1]  || 
     X[2] < startx[2] || 
     X[2] > stopx[2]) {
      return(0.) ;
  }

  return(interp_scalar(X, b)) ;
}

double get_model_ne(double X[NDIM])
{
  if(X[1] < startx[1] || 
     X[1] > stopx[1]  || 
     X[2] < startx[2] || 
     X[2] > stopx[2]) {
      return(0.) ;
  }
  
  return(interp_scalar(X, ne)) ;
}


/** HARM utilities **/

void Xtoijk(double X[NDIM], int *i, int *j, int *k, double del[NDIM]) ;

/********************************************************************

        Interpolation routines

 ********************************************************************/

/* return fluid four-vector in simulation units */
void interp_fourv(double X[NDIM], double ****fourv, double Fourv[NDIM]){
  double del[NDIM],b1,b2,b3,d1,d2,d3,d4;
  int i, j, k, ip1, jp1, kp1;

  /* find the current zone location and offsets del[0], del[1] */
  Xtoijk(X, &i, &j, &k, del);

  ip1 = i + 1;
  jp1 = j + 1;
  kp1 = k + 1;
  
  //conditions at the x3 periodic boundary (at the last active zone)
  if(k==(N3-1)) kp1=0;   

  b1 = 1.-del[1];
  b2 = 1.-del[2];
  b3 = 1.-del[3];

  d1 = b1*b2;
  d3 = del[1] * b2;
  d2 = del[2] * b1;
  d4 = del[1] * del[2];


  /* Interpolate along x1,x2 first */
  Fourv[0] = d1*fourv[i][j][k][0] + d2*fourv[i][jp1][k][0] + d3*fourv[ip1][j][k][0] + d4*fourv[ip1][jp1][k][0];
  Fourv[1] = d1*fourv[i][j][k][1] + d2*fourv[i][jp1][k][1] + d3*fourv[ip1][j][k][1] + d4*fourv[ip1][jp1][k][1];
  Fourv[2] = d1*fourv[i][j][k][2] + d2*fourv[i][jp1][k][2] + d3*fourv[ip1][j][k][2] + d4*fourv[ip1][jp1][k][2];
  Fourv[3] = d1*fourv[i][j][k][3] + d2*fourv[i][jp1][k][3] + d3*fourv[ip1][j][k][3] + d4*fourv[ip1][jp1][k][3];

  /* Now interpolate above in x3 */
  Fourv[0] = b3*Fourv[0] + del[3]*(d1*fourv[i][j][kp1][0] + d2*fourv[i][jp1][kp1][0] + d3*fourv[ip1][j][kp1][0] + d4*fourv[ip1][jp1][kp1][0]);
  Fourv[1] = b3*Fourv[1] + del[3]*(d1*fourv[i][j][kp1][1] + d2*fourv[i][jp1][kp1][1] + d3*fourv[ip1][j][kp1][1] + d4*fourv[ip1][jp1][kp1][1]);
  Fourv[2] = b3*Fourv[2] + del[3]*(d1*fourv[i][j][kp1][2] + d2*fourv[i][jp1][kp1][2] + d3*fourv[ip1][j][kp1][2] + d4*fourv[ip1][jp1][kp1][2]);
  Fourv[3] = b3*Fourv[3] + del[3]*(d1*fourv[i][j][kp1][3] + d2*fourv[i][jp1][kp1][3] + d3*fourv[ip1][j][kp1][3] + d4*fourv[ip1][jp1][kp1][3]);
  //new

  //no interpolation of vectors at all
 
  Fourv[0]=fourv[i][j][k][0];
  Fourv[1]=fourv[i][j][k][1];
  Fourv[2]=fourv[i][j][k][2];
  Fourv[3]=fourv[i][j][k][3];
  
}

/* return  scalar in cgs units */
double interp_scalar(double X[NDIM], double ***var)
{
  double del[NDIM],b1,b2,interp;
  int i, j, k, ip1, jp1, kp1;

  /* find the current zone location and offsets del[0], del[1] */
  Xtoijk(X, &i, &j, &k, del);

  ip1 = i+1;
  jp1 = j+1;
  kp1 = k+1;
  if(k==(N3-1)) kp1=0;    

  b1 = 1.-del[1];
  b2 = 1.-del[2];

  /* Interpolate in x1,x2 first */

  interp = var[i][j][k]*b1*b2 + 
    var[i][jp1][k]*b1*del[2] + 
    var[ip1][j][k]*del[1]*b2 + 
    var[ip1][jp1][k]*del[1]*del[2];


  /* Now interpolate above in x3 */

  interp = (1.-del[3])*interp + 
        del[3]*(var[i  ][j  ][kp1]*b1*b2 +
      var[i  ][jp1][kp1]*del[2]*b1 +
      var[ip1][j  ][kp1]*del[1]*b2 +
      var[ip1][jp1][kp1]*del[1]*del[2]);
  
  //new, no interpolations what so ever
    interp=var[i][j][k];
  /* use bilinear interpolation to find rho; piecewise constant
     near the boundaries */
  //printf("%i %i %i var = %e\n", i,j,k,interp);
  if (isnan(interp)) printf("INTERP BAD! %i %i %i %e\n", i,j,k,interp);

  return(interp);

}

/***********************************************************************************

          End interpolation routines

 ***********************************************************************************/


void Xtoijk(double X[NDIM], int *i, int *j, int *k, double del[NDIM])
{
  double phi;

  /* Map X[3] into sim range, assume startx[3] = 0 */
  phi = fmod(X[3], stopx[3]);
  //fold it to be positive and find index
  if(phi < 0.0) phi = stopx[3]+phi;
  
  //give index of a zone - zone index is moved to the grid zone center/
  //to account for the fact that all variables are reconstrucuted at zone centers?
  *i = (int) ((X[1] - startx[1]) / dx[1] - 0.5 + 1000) - 1000;
  *j = (int) ((X[2] - startx[2]) / dx[2] - 0.5 + 1000) - 1000;
  *k = (int) ((phi  - startx[3]) / dx[3] - 0.5 + 1000) - 1000;  

  //this makes sense, interpolate with outflow condition
  if(*i < 0) {
    *i = 0 ;
    del[1] = 0. ;
  }
  else if(*i > N1-2) { //OK because del[1]=1 and only terms with ip1=N1-1 will be important in interpolation
    *i = N1-2 ;
    del[1] = 1. ;
  }
  else {
    del[1] = (X[1] - ((*i + 0.5) * dx[1] + startx[1])) / dx[1];
  }
  
  if(*j < 0) {
          *j = 0 ;
          del[2] = 0. ;
        }
        else if(*j > N2-2) {
          *j = N2-2 ;
          del[2] = 1. ;
        }
        else {
          del[2] = (X[2] - ((*j + 0.5) * dx[2] + startx[2])) / dx[2];
        }

        if(*k < 0) {
          *k = 0 ;
          del[3] = 0. ;
        }
        else if(*k > N3-1) {
          *k = N3-1;
          del[3] = 1. ;
        }
        else {
          del[3] = (phi - ((*k + 0.5) * dx[3] + startx[3])) / dx[3];
        }

  // TEMPORARY FIX
  del[3] = 0.;
  *k = 0;

  return;
}

//#define SINGSMALL (1.E-20)
/* return boyer-lindquist coordinate of point */
void bl_coord(double X[NDIM], double *r, double *th)
{
  *r = exp(X[1]);

  double thG = M_PI*X[2] + ((1. - hslope)/2.)*sin(2.*M_PI*X[2]);
  double y = 2*X[2] - 1.;
  double thJ = poly_norm*y*(1. + pow(y/poly_xt,poly_alpha)/(poly_alpha+1.)) + 0.5*M_PI;
  *th = thG + exp(mks_smooth*(startx[1] - X[1]))*(thJ - thG);
}

void coord(int i, int j, int k, double *X)
{

  /* returns zone-centered values for coordinates */
  X[0] = startx[0];
  X[1] = startx[1] + (i + 0.5) * dx[1];
  X[2] = startx[2] + (j + 0.5) * dx[2];
  X[3] = startx[3] + (k + 0.5) * dx[3];

  return;
}


void set_units(char *munitstr)
{
  /*FILE *fp ;

  fp = fopen("model_param.dat","r") ;
  if(fp == NULL) {
    fprintf(stderr,"Can't find model_param.dat\n") ;
    exit(1) ;
  }
  fscanf(fp,"%lf",&double MBH;MBH) ;
  fclose(fp) ;*/

  //sscanf(munitstr,"%lf",&M_unit) ;

  /** input parameters appropriate to Sgr A* **/
  MBH = 4.6e6;

  /** from this, calculate units of length, time, mass,
      and derivative units **/
  MBH *= MSUN;
  L_unit = GNEWT * MBH / (CL * CL);
  T_unit = L_unit / CL;
  RHO_unit = M_unit / pow(L_unit, 3);
  U_unit = RHO_unit * CL * CL;
  B_unit = CL * sqrt(4.*M_PI*RHO_unit);
  Mdotedd=4.*M_PI*GNEWT*MBH*MP/CL/0.1/SIGMA_THOMSON;

  fprintf(stderr,"L,T,M units: %g [cm] %g [s] %g [g]\n",L_unit,T_unit,M_unit) ;
  fprintf(stderr,"rho,u,B units: %g [g cm^-3] %g [g cm^-1 s^-2] %g [G] \n",RHO_unit,U_unit,B_unit) ;
}



void init_physical_quantities(void)
{
  int i, j, k;
  double bsq,Thetae_unit,sigma_m;

  for (i = 0; i < N1; i++) {
    for (j = 0; j < N2; j++) {
      for (k = 0; k < N3; k++) {
        ne[i][j][k] = p[KRHO][i][j][k] * RHO_unit/(MP+ME) ;

        bsq= bcon[i][j][k][0] * bcov[i][j][k][0] +
             bcon[i][j][k][1] * bcov[i][j][k][1] +
             bcon[i][j][k][2] * bcov[i][j][k][2] +
             bcon[i][j][k][3] * bcov[i][j][k][3] ;

        b[i][j][k] = sqrt(bsq)*B_unit ;
        sigma_m=bsq/p[KRHO][i][j][k] ;

        // beta presciption
        //beta=p[UU][i][j][k]*(gam-1.)/0.5/bsq;
        //b2=pow(beta,2);
        //trat = trat_d * b2/(1. + b2) + trat_j /(1. + b2);
        //Thetae_unit = (gam - 1.) * (MP / ME) / trat;
        Thetae_unit = MP/ME;
       
        //thetae[i][j][k] = (gam-1.)*MP/ME*p[UU][i][j][k]/p[KRHO][i][j][k];
        //printf("rho = %e thetae = %e\n", p[KRHO][i][j][k], thetae[i][j][k]);

        thetae[i][j][k] = p[KEL][i][j][k]*pow(p[KRHO][i][j][k],game-1.)*Thetae_unit;
        thetae[i][j][k] = MAX(thetae[i][j][k], 1.e-3);
        //thetae[i][j][k] = MP/ME*p[UU][i][j][k]/p[KRHO][i][j][k]/4.;
        //printf("Thetae_unit = %e Thetae = %e\n", Thetae_unit, thetae[i][j][k]);
        
        //strongly magnetized = empty, no shiny spine
        if(sigma_m > 1.0) ne[i][j][k]=0.0;
      }
    }
  }

  return ;
}


void *malloc_rank1(int n1, int size)
{
  void *A;

  if((A = malloc(n1*size)) == NULL){
    fprintf(stderr,"malloc failure in malloc_rank1\n");
    exit(123);
  }

  return A;
}

double **malloc_rank2(int n1, int n2)
{

  double **A;
  double *space;
  int i;

  space = malloc_rank1(n1*n2, sizeof(double));

  A = malloc_rank1(n1, sizeof(double *));

  for(i = 0; i < n1; i++) A[i] = &(space[i*n2]);

  return A;
}


double ***malloc_rank3(int n1, int n2, int n3)
{

  double ***A;
  double *space;
  int i,j;

  space = malloc_rank1(n1*n2*n3, sizeof(double));

  A = malloc_rank1(n1, sizeof(double *));

  for(i = 0; i < n1; i++){
    A[i] = malloc_rank1(n2,sizeof(double *));
    for(j = 0; j < n2; j++){
      A[i][j] = &(space[n3*(j + n2*i)]);
    }
  }

  return A;
}


double ****malloc_rank4(int n1, int n2, int n3, int n4)
{

  double ****A;
  double *space;
  int i,j,k;

  space = malloc_rank1(n1*n2*n3*n4, sizeof(double));

  A = malloc_rank1(n1, sizeof(double *));
  
  for(i=0;i<n1;i++){
    A[i] = malloc_rank1(n2,sizeof(double *));
    for(j=0;j<n2;j++){
      A[i][j] = malloc_rank1(n3,sizeof(double *));
      for(k=0;k<n3;k++){
        A[i][j][k] = &(space[n4*(k + n3*(j + n2*i))]);
      }
    }
  }

  return A;
}

double *****malloc_rank5(int n1, int n2, int n3, int n4, int n5)
{

  double *****A;
  double *space;
  int i,j,k,l;

  space = malloc_rank1(n1*n2*n3*n4*n5, sizeof(double));

  A = malloc_rank1(n1, sizeof(double *));
  
  for(i=0;i<n1;i++){
    A[i] = malloc_rank1(n2, sizeof(double *));
    for(j=0;j<n2;j++){
      A[i][j] = malloc_rank1(n3, sizeof(double *));
      for(k=0;k<n3;k++){
        A[i][j][k] = malloc_rank1(n4, sizeof(double *));
        for(l=0;l<n4;l++){
          A[i][j][k][l] = &(space[n5*(l + n4*(k + n3*(j + n2*i)))]);
        }
      }
    }
  }

  return A;
}

void init_storage(void)
{
  int i;

  bcon = malloc_rank4(N1,N2,N3,NDIM);
  bcov = malloc_rank4(N1,N2,N3,NDIM);
  ucon = malloc_rank4(N1,N2,N3,NDIM);
  ucov = malloc_rank4(N1,N2,N3,NDIM);
  p = (double ****)malloc_rank1(NVAR,sizeof(double *));
  for(i = 0; i < NVAR; i++) p[i] = malloc_rank3(N1,N2,N3);
  ne = malloc_rank3(N1,N2,N3);
  thetae = malloc_rank3(N1,N2,N3);
  b = malloc_rank3(N1,N2,N3);

  return;
}

/* HDF5 v1.6 API */
//#include <H5LT.h>

/* HDF5 v1.8 API */
#include <hdf5.h>
#include <hdf5_hl.h>

/* bhlight3d globals */

/*extern double ****bcon;
extern double ****bcov;
extern double ****ucon;
extern double ****ucov;
extern double ****p;
extern double ***ne;
extern double ***thetae;
extern double ***b;*/

void init_bhlight3d_grid(char *fname)
{
  hid_t file_id;
  //double th_end,th_cutout;
  printf("init grid\n");

  file_id = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);
  if(file_id < 0){
    fprintf(stderr,"file %s does not exist, aborting...\n",fname);
    exit(1234);
  }
  printf("Opened file!\n");
  H5LTread_dataset_int(file_id,   "N1",   &N1);
  H5LTread_dataset_int(file_id,   "N2",   &N2);
  H5LTread_dataset_int(file_id,   "N3",   &N3);
  H5LTread_dataset_double(file_id, "startx1",  &startx[1]);
  H5LTread_dataset_double(file_id, "startx2",  &startx[2]);
  H5LTread_dataset_double(file_id, "startx3",  &startx[3]);
  H5LTread_dataset_double(file_id, "dx1",  &dx[1]);
  H5LTread_dataset_double(file_id, "dx2",  &dx[2]);
  H5LTread_dataset_double(file_id, "dx3",  &dx[3]);
  H5LTread_dataset_double(file_id, "a", &a);
  H5LTread_dataset_double(file_id, "gam", &gam);
  H5LTread_dataset_double(file_id, "game", &game);
  H5LTread_dataset_double(file_id, "gamp", &gamp);
  H5LTread_dataset_double(file_id, "Rin", &Rin);
  H5LTread_dataset_double(file_id, "Rout", &Rout);
  H5LTread_dataset_double(file_id, "hslope", &hslope);
  H5LTread_dataset_double(file_id, "poly_xt", &poly_xt);
  H5LTread_dataset_double(file_id, "poly_alpha", &poly_alpha);
  H5LTread_dataset_double(file_id, "mks_smooth", &mks_smooth);

  // Set polylog grid normalization
  poly_norm = 0.5*M_PI*1./(1. + 1./(poly_alpha + 1.)*1./pow(poly_xt, poly_alpha));

  
  //startx[1] += 3*dx[1];
  //startx[2] += 3*dx[2];
  //      startx[3] += 3*dx[3];
  
  fprintf(stdout,"start: %g %g %g \n",startx[1],startx[2],startx[3]);
  //below is equivalent to the above
  /*
  startx[1] = log(Rin-R0);       
        startx[2] = th_cutout/M_PI ; 
  */

  //fprintf(stdout,"th_cutout: %g  %d x %d x %d\n",th_cutout,N1,N2,N3);
  

  //th_beg=th_cutout;
  //th_end=M_PI-th_cutout;
  //th_len = th_end-th_beg;

  // Ignore radiation interactions within one degree of polar axis
  th_beg = 0.0174;
  //th_end = 3.1241;

  stopx[0] = 1.;
  stopx[1] = startx[1]+N1*dx[1];
  stopx[2] = startx[2]+N2*dx[2];
  stopx[3] = startx[3]+N3*dx[3];

  fprintf(stdout,"stop: %g %g %g \n",stopx[1],stopx[2],stopx[3]);

  init_storage();

  H5Fclose(file_id);
}

void init_bhlight3d_data(char *fname)
{
  hid_t file_id;
  int i,j,k,l,m;
  double X[NDIM],UdotU,ufac,udotB;
  double gcov[NDIM][NDIM],gcon[NDIM][NDIM], g;
  double dMact, Ladv;
  double r,th;
  /*FILE *fp;

  fp = fopen("model_param.dat","r") ;
        if(fp == NULL) {
    fprintf(stderr,"Can't find model_param.dat\n") ;
    exit(1) ;
        }
        fscanf(fp,"%lf",&MBH) ;
        fclose(fp) ;*/

  //MBH = 4.6e6;
  //MBH=MBH*MSUN;
  //      L_unit = GNEWT * MBH / (CL * CL);
  //      T_unit = L_unit / CL;

  
  file_id = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);
  if(file_id < 0){
    fprintf(stderr,"file %s does not exist, aborting...\n",fname);
    exit(12345);
  }


  //  fprintf(stderr,"data incoming...");
  H5LTread_dataset_double(file_id, "RHO", &(p[KRHO][0][0][0]));
  H5LTread_dataset_double(file_id, "UU",  &(p[UU][0][0][0]));
  H5LTread_dataset_double(file_id, "U1",  &(p[U1][0][0][0]));
  H5LTread_dataset_double(file_id, "U2",  &(p[U2][0][0][0]));
  H5LTread_dataset_double(file_id, "U3",  &(p[U3][0][0][0]));
  H5LTread_dataset_double(file_id, "B1",  &(p[B1][0][0][0]));
  H5LTread_dataset_double(file_id, "B2",  &(p[B2][0][0][0]));
  H5LTread_dataset_double(file_id, "B3",  &(p[B3][0][0][0]));
  H5LTread_dataset_double(file_id, "KEL", &(p[KEL][0][0][0]));
  H5LTread_dataset_double(file_id, "KTOT",  &(p[KTOT][0][0][0]));

  H5Fclose(file_id);
  X[0] = 0.;
  //X[3] = 0.;

  //fprintf(stderr,"reconstructing 4-vectors...\n");
  dMact = Ladv = 0.;

  //reconstruction of variables at the zone center!
  for(i = 0; i < N1; i++){
    X[1] = startx[1] + ( i + 0.5)*dx[1];
    for(j = 0; j < N2; j++){
      X[2] = startx[2] + (j+0.5)*dx[2];
      gcov_func(X, gcov); // in system with cut off
      gcon_func(gcov, gcon);
      g = gdet_func(gcov);

      bl_coord(X, &r, &th);

      for(k = 0; k < N3; k++){
        UdotU = 0.;
        X[3] = startx[3] + (k+0.5)*dx[3];
        
        //the four-vector reconstruction should have gcov and gcon and gdet using the modified coordinates
        //interpolating the four vectors to the zone center !!!!
        for(l = 1; l < NDIM; l++) for(m = 1; m < NDIM; m++) UdotU += gcov[l][m]*p[U1+l-1][i][j][k]*p[U1+m-1][i][j][k];
        ufac = sqrt(-1./gcon[0][0]*(1 + fabs(UdotU)));
        ucon[i][j][k][0] = -ufac*gcon[0][0];
        for(l = 1; l < NDIM; l++) ucon[i][j][k][l] = p[U1+l-1][i][j][k] - ufac*gcon[0][l];
        lower(ucon[i][j][k], gcov, ucov[i][j][k]);

        //reconstruct the magnetic field three vectors
        udotB = 0.;
        for(l = 1; l < NDIM; l++) udotB += ucov[i][j][k][l]*p[B1+l-1][i][j][k];
        bcon[i][j][k][0] = udotB;
        for(l = 1; l < NDIM; l++) bcon[i][j][k][l] = (p[B1+l-1][i][j][k] + ucon[i][j][k][l]*udotB)/ucon[i][j][k][0];
        lower(bcon[i][j][k], gcov, bcov[i][j][k]);

        if(i <= 20) dMact += g * p[KRHO][i][j][k] * ucon[i][j][k][1] ;
        if(i >= 20 && i < 40) Ladv += g * p[UU][i][j][k] * ucon[i][j][k][1] * ucov[i][j][k][0] ;

      }
    }
  }
  
  dMact *= dx[3]*dx[2] ;
  dMact /= 21. ;
  Ladv *= dx[3]*dx[2] ;
  Ladv /= 21. ;

  fprintf(stderr,"dMact: %g [code]\n",dMact) ;
  fprintf(stderr,"Ladv: %g [code]\n",Ladv) ;
  fprintf(stderr,"Mdot: %g [g/s] \n",-dMact*M_unit/T_unit) ;
  fprintf(stderr,"Mdot: %g [MSUN/YR] \n",-dMact*M_unit/T_unit/(MSUN / YEAR)) ;
        //double Mdotedd=4.*M_PI*GNEWT*MBH*MP/CL/0.1/SIGMA_THOMSON;
        fprintf(stderr,"Mdot: %g [Mdotedd]\n",-dMact*M_unit/T_unit/Mdotedd) ;
        fprintf(stderr,"Mdotedd: %g [g/s]\n",Mdotedd) ;
        fprintf(stderr,"Mdotedd: %g [MSUN/YR]\n",Mdotedd/(MSUN/YEAR)) ;
  
}

double root_find(double x[NDIM])
{
    double th = x[2];
    double thb, thc;
    double dtheta_func(double X[NDIM]), theta_func(double X[NDIM]);

    double Xa[NDIM], Xb[NDIM], Xc[NDIM];
    Xa[1] = log(x[1]);
    Xa[3] = x[3];
    Xb[1] = Xa[1];
    Xb[3] = Xa[3];
    Xc[1] = Xa[1];
    Xc[3] = Xa[3];

    if (x[2] < M_PI / 2.) {
      Xa[2] = 0. - SMALL;
      Xb[2] = 0.5 + SMALL;
    } else {
      Xa[2] = 0.5 - SMALL;
      Xb[2] = 1. + SMALL;
    }

    //tha = theta_func(Xa);
    thb = theta_func(Xb);

    /* bisect for a bit */
    double tol = 1.e-6;
    for (int i = 0; i < 100; i++) {
      Xc[2] = 0.5 * (Xa[2] + Xb[2]);
      thc = theta_func(Xc);

      if ((thc - th) * (thb - th) < 0.)
        Xa[2] = Xc[2];
      else
        Xb[2] = Xc[2];

      double err = theta_func(Xc) - th;
      if (fabs(err) < tol) break;
    }

    return (Xa[2]);
}

/*this does not depend on theta cut-outs there is no squizzing*/
double theta_func(double X[NDIM])
{
  double r, th;
  bl_coord(X, &r, &th);
  return th;
}

