/******************************************************************************
 *                     PIP : Parametric Integer Programming                   *
 ******************************************************************************
 *                                integrer.c                                  *
 ******************************************************************************
 *                                                                            *
 * Copyright Paul Feautrier, 1988, 1993, 1994, 1996, 2002                     *
 *                                                                            *
 * This is free software; you can redistribute it and/or modify it under the  *
 * terms of the GNU General Public License as published by the Free Software  *
 * Foundation; either version 2 of the License, or (at your option) any later *
 * version.                                                                   *
 *                                                                            *
 * This software is distributed in the hope that it will be useful, but       *
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License   *
 * for more details.                                                          *
 *                                                                            *
 * You should have received a copy of the GNU General Public License along    *
 * with software; if not, write to the Free Software Foundation, Inc.,        *
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA                     *
 *                                                                            *
 * Written by Paul Feautrier                                                  *
 *                                                                            *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "pip.h"

/*  The routines in this file are used to build a Gomory cut from
    a non-integral row of the problem tableau                             */

extern long int cross_product, limit;
extern int verbose;
extern int deepest_cut;
extern FILE * dump;
char compose[256];

/* mod(x,y) computes the remainder of x when divided by y. The difference
   with x%y is that the result is guaranteed to be positive, which is not
   always true for x%y.  This function is replaced by mpz_fdiv_r for MP. */ 

#if !defined(LINEAR_VALUE_IS_MP)
Entier mod(x, y)
Entier x, y;
{Entier r;
 r = x % y;
 if(r<0) r += y;
 return(r);
}
#endif

/* this routine is useless at present                                     */

int non_borne(tp, nvar, D, bigparm)
Tableau *tp;
int nvar, bigparm;
Entier D;
{int i, ff;
 for(i = 0; i<nvar; i++)
     {ff = Flag(tp, i);
      if(bigparm > 0)
	 {if(ff & Unit)return(Pip_True);
          if(Index(tp, i, bigparm) != D) return(Pip_True);
	 }
      }
 return(Pip_False);
}

/* This routine solve for z in the equation z.y = x (mod delta), provided
   y and delta are mutually prime. Remember that for multiple precision
   operation, the responsibility of creating and destroying <<z>> is the 
   caller's.                                                                */

void bezout(Entier x, Entier y, Entier delta, Entier *z){
  Entier a, b, c, d, e, f, u, v, q, r;
#if defined(LINEAR_VALUE_IS_MP)
  mpz_init(a); mpz_init(b); mpz_init(c); mpz_init(d);
  mpz_init(e); mpz_init(f); mpz_init(u); mpz_init(v);
  mpz_init(q); mpz_init(r);
  mpz_set_ui(a, 1); mpz_set_ui(b, 0); mpz_set_ui(c, 0);
  mpz_set_ui(d, 1); mpz_set(u, y); mpz_set(v, delta);
#else
  a = 1; b = 0; c = 0; d = 1;
  u = y; v = delta;
#endif
  for(;;){
#if defined(LINEAR_VALUE_IS_MP)
    mpz_fdiv_qr(q, r, u, v);
    if(mpz_cmp_ui(r, 0) == 0) break;
    mpz_set(u, v);
    mpz_set(v, r);
    mpz_mul(e, q, c);
    mpz_sub(e, a, e);
    mpz_mul(f, q, d);
    mpz_sub(f, b, f);
    mpz_set(a, c);
    mpz_set(b, d);
    mpz_set(c, e);
    mpz_set(d, f);
#else
    q = u / v;
    r = mod(u, v);
    if(r == 0) break;
    u = v;
    v = r;
    e = a - q*c;
    f = b - q*d;
    a = c;
    b = d;
    c = e;
    d = f;
#endif
  }
#if defined(LINEAR_VALUE_IS_MP)
  if(mpz_cmp_ui(v, 1) != 0)
    mpz_set_ui(*z, 0);
  else {
    mpz_mul(a, c, x);
    mpz_mod(*z, a, delta);
  }
  mpz_clear(a); mpz_clear(b); mpz_clear(c); mpz_clear(d);
  mpz_clear(e); mpz_clear(f); mpz_clear(u); mpz_clear(v);
  mpz_clear(q); mpz_clear(r);
  
#else
  if(v != 1) *z = 0; /* y and delta are not mutually prime */
  else *z = mod(c*x, delta);
#endif
}

Tableau *expanser();

/* integrer(.....) add a cut to the problem tableau, or return 0 when an
   integral solution has been found, or -1 when no integral solution
   exists.

   Since integrer may add rows and columns to the problem tableau, its
   arguments are pointers rather than values. If a cut is constructed,
   ni increases by 1. If the cut is parametric, nparm increases by 1 and
   nc increases by 2.
									 */

int integrer(Tableau **ptp, Tableau **pcontext, 
	     int *pnvar, int *pnparm, int *pni, int *pnc, int bigparm)
{int ncol = *pnvar+*pnparm+1;
 int nligne = *pnvar + *pni;
 int nparm = *pnparm;
 int nvar = *pnvar;
 int ni = *pni;
 int nc = *pnc;
 Entier coupure[MAXCOL];
 int i, j, k, ff;
 Entier x, d;
 int ok_var, ok_const, ok_parm;
 Entier discrp[MAXPARM], discrm[MAXPARM];
 int llog();
 Entier D;

 Entier t, delta, tau, lambda;
 
 #if defined(LINEAR_VALUE_IS_MP)
 for(i=0; i<=ncol; i++)
   mpz_init(coupure[i]);

 for(i=0; i<=nparm+1; i++){
   mpz_init(discrp[i]);
   mpz_init(discrm[i]);
 }

 mpz_init(x); mpz_init(d); mpz_init(D);
 mpz_init(t); mpz_init(delta); mpz_init(tau); mpz_init(lambda);
 #endif


 if(ncol+1 >= MAXCOL) {
      fprintf(stderr, "Too much variables : %d\n", ncol);
      exit(3);
      }

/* search for a non-integral row */
 for(i = 0; i<nvar; i++) {
      #if defined(LINEAR_VALUE_IS_MP)
      mpz_set(D, Denom(*ptp, i));
      if(mpz_cmp_ui(D, 1) == 0) continue;
      #else
      D = Denom(*ptp,i);
      if(D == 1) continue;
      #endif
/*                          If the common denominator of the row is 1
                            the row is integral                         */
      ff = Flag(*ptp, i);
      if(ff & Unit)continue;
/*                          If the row is a Unit, it is integral        */

/*                          Here a portential candidate has been found.
                            Build the cut by reducing each coefficient
                            modulo D, the common denominator            */
      ok_var = Pip_False;
      for(j = 0; j<nvar; j++) {
         #if defined(LINEAR_VALUE_IS_MP)
         mpz_fdiv_r(x, Index(*ptp, i, j), D);
         mpz_set(coupure[j], x);
         #else
         x = coupure[j] = mod(Index(*ptp, i, j), D);
         #endif
	    if (value_pos_p(x))
		ok_var = Pip_True;
          }
/*                          Done for the coefficient of the variables.  */

      #if defined(LINEAR_VALUE_IS_MP)
      mpz_neg(x, Index(*ptp, i, nvar));
      mpz_fdiv_r(x, x, D);
      mpz_neg(x, x);
      mpz_set(coupure[nvar], x);
      ok_const = mpz_cmp_ui(x, 0);
      #else
      x = coupure[nvar] = - mod(-Index(*ptp, i, nvar), D);
      ok_const = (x != 0);
      #endif
/*                          This is the constant term                   */
      ok_parm = Pip_False;
      for(j = nvar+1; j<ncol; j++) {
	 /* We assume that the big parameter is divisible by any number. */
	 if (j == bigparm) {
	    value_set_si(coupure[j], 0);
	    continue;
	 }
	 value_oppose(x, Index(*ptp, i, j));
	 value_pmodulus(x, x, D);
	 value_oppose(coupure[j], x);
	 if (value_notzero_p(coupure[j]))
	    ok_parm = Pip_True;
      }
/*                          These are the parametric terms              */

      #if defined(LINEAR_VALUE_IS_MP)
      mpz_set(coupure[ncol], D);
      #else
      coupure[ncol] = D;    /* Just in case                             */
      #endif

/* The question now is whether the cut is valid. The answer is given
by the following decision table:

ok_var   ok_parm   ok_const

  F        F         F       (a) continue, integral row
  F        F         T       (b) return -1, no solution
  F        T         F       
                             (c) if the <<constant>> part is not divisible
                             by D then bottom else ....
  F        T         T
  T        F         F       (a) continue, integral row
  T        F         T       (d) constant cut
  T        T         F
                             (e) parametric cut
  T        T         T

                                                                case (a)  */

      if(!ok_parm && !ok_const) continue;
      if(!ok_parm)
          if(ok_var) {                                   /*     case (d)  */
              if(nligne >= (*ptp)->height) {
		  int d, dth, dtw;
                  #if defined(LINEAR_VALUE_IS_MP)
	          d = mpz_sizeinbase(D, 2);
                  #else
                  d = llog(D);
                  #endif
                  dth = d;
		  *ptp = expanser(*ptp, nvar, ni, ncol, 0, dth, 0);
                  }
	      /* Find the deepest cut*/
	      if(deepest_cut){
#if defined(LINEAR_VALUE_IS_MP)
	      mpz_neg(t, coupure[nvar]);
              mpz_gcd(delta, t, D);
	      mpz_divexact(tau, t, delta);
	      mpz_divexact(d, D, delta);
              mpz_sub_ui(t, d, 1);
              bezout(t, tau, d, &lambda);
	      mpz_gcd(t, lambda, D);
              while(mpz_cmp_ui(t, 1) != 0){
		mpz_add(lambda, lambda, d);
		mpz_gcd(t, lambda, D);
	      }
	      for(j=0; j<nvar; j++){
		mpz_mul(t, lambda, coupure[j]);
		mpz_fdiv_r(coupure[j], t, D);
	      }
	      mpz_mul(t, coupure[nvar], lambda);
	      mpz_mod(t, t, D);
	      mpz_sub(t, D, t);
	      mpz_neg(coupure[nvar], t);
#else
	      t = -coupure[nvar];
	      delta = pgcd(t,D);
	      tau = t/delta;
	      d = D/delta;
	      bezout(d-1, tau, d, &lambda);
	      while(pgcd(lambda, D) != 1)
		lambda += d;
	      for(j=0; j<nvar; j++)
		coupure[j] = mod(lambda*coupure[j], D);
	      coupure[nvar] = -mod(-lambda*coupure[nvar], D);
#endif
	      }
                         /* The cut has a negative <<constant>> part      */
              Flag(*ptp, nligne) = Minus; 
              #if defined(LINEAR_VALUE_IS_MP)
              mpz_set(Denom(*ptp, nligne), D);
              #else
              Denom(*ptp, nligne) = D;
              #endif
                         /* Insert the cut */
	      for(j = 0; j<ncol; j++)
                  #if defined(LINEAR_VALUE_IS_MP)
	          mpz_set(Index(*ptp, nligne, j), coupure[j]);
                  #else
                  Index(*ptp, nligne, j) = coupure[j];
                  #endif
                      /* A new row has been added to the problem tableau. */
	      (*pni)++;
              if(verbose > 0) {
		fprintf(dump, "just cut ");
                if(deepest_cut){
		  fprintf(dump, "Bezout multiplier ");
#if defined(LINEAR_VALUE_IS_MP)
		  mpz_out_str(dump, 10, lambda);
#else
		  fprintf(dump, FORMAT, lambda);
#endif
		}
                fprintf(dump, "\n");
		k=0;
                for(i=0; i<nvar; i++){
                  if(Flag(*ptp, i) & Unit){
#if defined(LINEAR_VALUE_IS_MP)
		    fprintf(dump, "0 ");
#else
		    sprintf(compose+k, "0 ");
#endif
		    k += 2;
		  }
		  else {
#if defined(LINEAR_VALUE_IS_MP)
		    k += mpz_out_str(dump, 10, Index(*ptp, i, nvar));
		    fprintf(dump, "/");
		    k++;
		    k += mpz_out_str(dump, 10, Denom(*ptp, i));
		    fprintf(dump, " ");
		    k++;
		    if(k > 60){
		      putc('\n', dump);
		      k = 0;
		    }
#else
		    sprintf(compose+k, FORMAT, Index(*ptp, i, nvar));
		    k = strlen(compose);
		    sprintf(compose+k, "/");
		    k++;
		    sprintf(compose+k, FORMAT, Denom(*ptp, i));
		    k = strlen(compose);
		    sprintf(compose+k, " ");
		    k++;
		    if(k>60)  {
		      fputs(compose, dump);
		      putc('\n', dump);
		      k=0;
		    }
#endif
		  }
		}
		fputs(compose, dump);
		putc('\n', dump);
	      }
	      if(verbose > 2) tab_display(*ptp, dump);
#if defined(LINEAR_VALUE_IS_MP)
	      goto clear;
#else
	      return(nligne);
#endif
              }
          else                                                               /*   case (b)    */
#if defined(LINEAR_VALUE_IS_MP)
          { nligne = -1; 
            goto clear;
          }
#else
          return -1;  
#endif
/* In case (e), one has to introduce a new parameter and
   introduce its defining inequalities into the context.
   
   Let the cut be    sum_{j=0}^{nvar-1} c_j x_j + c_{nvar} +             (2)
                     sum_{j=0}^{nparm-1} c_{nvar + 1 + j} p_j >= 0.       */
           
                               
      if(nparm >= MAXPARM) {
          fprintf(stderr, "Too much parameters : %d\n", *pnparm);
          exit(4);
          }
/*        Build the definition of the new parameter into the solution :
      p_{nparm} = -(sum_{j=0}^{nparm-1} c_{nvar + 1 + j} p_j 
                     + c_{nvar})/D                             (3)
         The minus sign is there to compensate the one in (1)     */

      sol_new(nparm);
      sol_div();
      sol_forme(nparm+1);
      for(j = 0; j<nparm; j++)
      #if defined(LINEAR_VALUE_IS_MP)
      { mpz_neg(x, coupure[j+nvar+1]);
        sol_val(x, UN);
      }
      mpz_neg(x, coupure[*pnvar]);
      sol_val(x, UN);
      #else
      sol_val(-coupure[j+nvar+1], UN); /* loop body. */
      sol_val(-coupure[*pnvar], UN);
      #endif
      sol_val(D, UN);                     /* The divisor                */

/* The value of the new parameter is specified by applying the definition of
   Euclidean division to (3) :

 0<= - sum_{j=0}^{nparm-1} c_{nvar+1+j} p_j - c_{nvar} - D * p_{nparm} < D (4)

   This formula gives two inequalities which are stored in the context    */
             
      for(j = 0; j<nparm; j++) {
          #if defined(LINEAR_VALUE_IS_MP)
          mpz_set(x, coupure[j+nvar+1]);
          mpz_neg(discrp[j], x);
          mpz_set(discrm[j], x);
          #else
	  x = coupure[j+nvar+1];
          discrp[j] = -x;
          discrm[j] = x;
          #endif
          }
      #if defined(LINEAR_VALUE_IS_MP)
      mpz_neg(discrp[nparm], D);
      mpz_set(discrm[nparm], D);
      mpz_set(x, coupure[nvar]);
      mpz_neg(discrp[nparm+1], x);
      mpz_sub_ui(x, x, 1);
      mpz_add(discrm[nparm+1], x, D);
      #else
      discrp[nparm] = -D;
      discrm[nparm] = D;
      x = coupure[nvar];
      discrp[(nparm)+1] = -x;
      discrm[(nparm)+1] = x + D -1;
      #endif
      if (nc+2 > (*pcontext)->height || nparm+1+1 > (*pcontext)->width) {
          int dcw, dch;
          #if defined(LINEAR_VALUE_IS_MP)
          dcw = mpz_sizeinbase(D, 2);
          #else
          dcw = llog(D);
          #endif
	  dch = 2 * dcw + *pni;
          *pcontext = expanser(*pcontext, 0, nc, nparm+1, 0, dch, dcw);
	  }
      /* Flag(*pcontext, *pnc) = 0; Probably useless see line A */

/* Since a new parameter is to be added, the constant term has to be moved
   right and a zero has to be inserted in all rows of the old context    */

      for(k = 0; k < nc; k++) {
          #if defined(LINEAR_VALUE_IS_MP)
          mpz_set(Index(*pcontext, k, nparm+1), Index(*pcontext, k, nparm));
          mpz_set_ui(Index(*pcontext, k, nparm), 0);
          #else
          Index(*pcontext, k, nparm+1) = Index(*pcontext, k, nparm);
          Index(*pcontext, k, nparm) = 0;
          #endif
          }
/* Now, insert the new rows                                              */

      for(j = 0; j <= nparm+1; j++) {
          #if defined(LINEAR_VALUE_IS_MP)
          mpz_set(Index(*pcontext, nc, j), discrp[j]); 
          mpz_set(Index(*pcontext, nc+1, j), discrm[j]);
          #else
          Index(*pcontext, nc, j) = discrp[j];
          Index(*pcontext, nc+1, j) = discrm[j];
          #endif
          }
      Flag(*pcontext, nc) = Unknown;                                /* A */
      Flag(*pcontext, nc+1) = Unknown;
      #if defined(LINEAR_VALUE_IS_MP)
      mpz_set(Denom(*pcontext, nc), UN);
      mpz_set(Denom(*pcontext, nc+1), UN);
      #else
      Denom(*pcontext, nc) = UN;
      Denom(*pcontext, nc+1) = UN;
      #endif
      (*pnparm)++;
      (*pnc) += 2;
      if(verbose > 0){
        fprintf(dump, "enlarged context %d x %d\n", *pnparm, *pnc);
        fflush(dump);
      }
                         /* end of the construction of the new parameter */

	assert(ok_var);
          if(nligne >= (*ptp)->height || ncol >= (*ptp)->width) {
              int d, dth, dtw;
             #if defined(LINEAR_VALUE_IS_MP)
             d = mpz_sizeinbase(D, 2);
             #else
             d = llog(D);
             #endif
              dth = d + ni;
	      dtw = d;
	      *ptp = expanser(*ptp, nvar, ni, ncol, 0, dth, dtw);
              }
                         /* Zeroing out the new column seems to be useless
			    since <<expanser>> does it anyway            */
                            
			 /* The cut has a negative <<constant>> part    */
	  Flag(*ptp, nligne) = Minus;
          #if defined(LINEAR_VALUE_IS_MP)
          mpz_set(Denom(*ptp, nligne), D);
          #else
	  Denom(*ptp, nligne) = D;
          #endif
              	 /* Insert the cut */
	  for(j = 0; j<ncol+1; j++)
              #if defined(LINEAR_VALUE_IS_MP)
              mpz_set(Index(*ptp, nligne, j), coupure[j]);
              #else
	      Index(*ptp, nligne, j) = coupure[j];
              #endif
		 /* A new row has been added to the problem tableau.    */
	  (*pni)++;
          #if defined(LINEAR_VALUE_IS_MP)
          goto clear;
          #else
	  return(nligne);
          #endif
      }
 /* The solution is integral.                              */
 #if defined(LINEAR_VALUE_IS_MP)
 nligne = 0;
 clear : 
   for(i=0; i <= ncol; i++)
     mpz_clear(coupure[i]);
   for(i=0; i <= nparm+1; i++){
     mpz_clear(discrp[i]);
     mpz_clear(discrm[i]);
   }
   mpz_clear(x); mpz_clear(d); mpz_clear(D);
   mpz_clear(t); mpz_clear(tau); mpz_clear(lambda); mpz_clear(delta);
 return(nligne);
 #else
   return 0;
 #endif
}

