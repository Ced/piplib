/******************************************************************************
 *                     PIP : Parametric Integer Programming                   *
 ******************************************************************************
 *                                integrer.c                                  *
 ******************************************************************************
 *                                                                            *
 * Copyright Paul Feautrier, 1988, 1993, 1994, 1996, 2002                     *
 *                                                                            *
 * This library is free software; you can redistribute it and/or modify it    *
 * under the terms of the GNU Lesser General Public License as published by   *
 * the Free Software Foundation; either version 2.1 of the License, or (at    *
 * your option) any later version.                                            *
 *                                                                            *
 * This software is distributed in the hope that it will be useful, but       *
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License   *
 * for more details.                                                          *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with this library; if not, write to the Free Software Foundation,    *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA         *
 *                                                                            *
 * Written by Paul Feautrier                                                  *
 *                                                                            *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "pip.h"

/*  The routines in this file are used to build a Gomory cut from
    a non-integral row of the problem tableau                             */

extern int PIPLIB_NAME(verbose);
extern int PIPLIB_NAME(deepest_cut);
extern FILE * PIPLIB_NAME(dump);

#if defined(PIPLIB_INT_SP) || defined(PIPLIB_INT_DP)
// From osl_int
long long int PIPLIB_NAME(piplib_llgcd)(long long int const a, long long int const b) {
  return (b ? PIPLIB_NAME(piplib_llgcd)(b, a % b) : a);
}
long long int PIPLIB_NAME(piplib_llgcd_llabs)(long long int const a, long long int const b) {
  return llabs(PIPLIB_NAME(piplib_llgcd)(a, b));
}
size_t PIPLIB_NAME(piplib_lllog2)(long long int x) {
  size_t n = 0;

  x = llabs(x);

  while (x) { x >>= 1; ++n; }

  return ((n == 0) ? 1 : n);
}
size_t PIPLIB_NAME(piplib_lllog10)(long long int x) {
  size_t n = 0;

  x = llabs(x);

  while (x) { x /= 10; ++n; }

  return n;
}
long long int PIPLIB_NAME(piplib_llmod)(long long int const a, long long int const b) {
  long long mod = a % b;
   if (mod < 0) { mod += llabs(b); }
  return mod;
}
long long int piplib_ll_floor_div_q(long long int const a,
                                    long long int const b) {
  long long int q = a / b;
  if (q < 0) { if (a % b != 0) --q; }
  else if (q == 0) {
    if ((a > 0 && b < 0) || (a < 0 && b > 0)) { --q; }
  }
  return q;
}
long long int piplib_ll_floor_div_r(long long int const a,
                                    long long int const b) {
  long long int q = piplib_ll_floor_div_q(a, b);
  return (a - q * b);
}
#endif


/* This routine solve for z in the equation z.y = x (mod delta), provided
   y and delta are mutually prime. Remember that for multiple precision
   operation, the responsibility of creating and destroying <<z>> is the 
   caller's.                                                                */

void PIPLIB_NAME(bezout)(PIPLIB_NAME(piplib_int_t) x, /*PIPLIB_NAME(piplib_int_t) y,*/
            PIPLIB_NAME(piplib_int_t) delta, PIPLIB_NAME(piplib_int_t)* z) {
  PIPLIB_NAME(piplib_int_t) a, b, c, d, e, f, u, v, q, r;

  piplib_int_init_set_si(a, 1);
  piplib_int_init(b);
  piplib_int_init(c);
  piplib_int_init_set_si(d, 1);
  piplib_int_init(e);
  piplib_int_init(f);
  piplib_int_init(v);
  piplib_int_init_set(u, v);
  piplib_int_init_set(v, delta);
  piplib_int_init(q);
  piplib_int_init(r);

  for(;;) {
    piplib_int_floor_div_q_r(q, r, u, v);

    if(piplib_int_zero(r) == 0) break;

    piplib_int_assign(u, v);
    piplib_int_assign(v, r);
    piplib_int_mul(e, q, c);
    piplib_int_sub(e, a, e);
    piplib_int_mul(f, q, d);
    piplib_int_sub(f, b, f);
    piplib_int_assign(a, c);
    piplib_int_assign(b, d);
    piplib_int_assign(c, e);
    piplib_int_assign(d, f);
  }

  // v != 1
  if (piplib_int_one(v) == 0) {
    piplib_int_set_si(*z, 0);
  } else {
    piplib_int_mul(a, c, x);
    piplib_int_mod(*z, a, delta);
  }

  piplib_int_clear(a);
  piplib_int_clear(b);
  piplib_int_clear(c);
  piplib_int_clear(d);
  piplib_int_clear(e);
  piplib_int_clear(f);
  piplib_int_clear(u);
  piplib_int_clear(v);
  piplib_int_clear(q);
  piplib_int_clear(r);
}

PIPLIB_NAME(Tableau) *PIPLIB_NAME(expanser)();

/* cut: constant parameters denominator */
static void PIPLIB_NAME(add_parm)(PIPLIB_NAME(Tableau) **pcontext, int nr, int *pnparm, int *pni, int *pnc,
		    PIPLIB_NAME(piplib_int_t) *cut)
{
    int nparm = *pnparm;
    int j, k;
    PIPLIB_NAME(piplib_int_t) x;

    piplib_int_init(x);

/*        Build the definition of the new parameter into the solution :
      p_{nparm} = -(sum_{j=0}^{nparm-1} c_{nvar + 1 + j} p_j 
                     + c_{nvar})/D                             (3)
         The minus sign is there to compensate the one in (1)     */

    PIPLIB_NAME(sol_new)(nparm);
    PIPLIB_NAME(sol_div)();
    PIPLIB_NAME(sol_forme)(nparm+1);
    for (j = 0; j < nparm; j++) {
	piplib_int_oppose(x, cut[1+j]);
        PIPLIB_NAME(sol_val_one)(x);
    }
    piplib_int_oppose(x, cut[0]);
    PIPLIB_NAME(sol_val_one)(x);
    PIPLIB_NAME(sol_val_one)(cut[1+nparm]);		    /* The divisor                */

    if (nr+2 > (*pcontext)->height || nparm+1+1 > (*pcontext)->width) {
	int dcw, dch;
	dcw = piplib_int_size_in_base_2(cut[1+nparm]);
	dch = 2 * dcw + *pni;
	*pcontext = PIPLIB_NAME(expanser)(*pcontext, 0, nr, nparm+1, 0, dch, dcw);
    }

/* Since a new parameter is to be added, the constant term has to be moved
   right and a zero has to be inserted in all rows of the old context    */

    for (k = 0; k < nr; k++) {
	piplib_int_assign(Index(*pcontext, k, nparm+1), Index(*pcontext, k, nparm));
	piplib_int_set_si(Index(*pcontext, k, nparm), 0);
    }

/* The value of the new parameter is specified by applying the definition of
   Euclidean division to (3) :

 0<= - sum_{j=0}^{nparm-1} c_{nvar+1+j} p_j - c_{nvar} - D * p_{nparm} < D (4)

   This formula gives two inequalities which are stored in the context    */

    for (j = 0; j < nparm; j++) {
	piplib_int_oppose(Index(*pcontext, nr, j), cut[1+j]);
	piplib_int_assign(Index(*pcontext, nr+1, j), cut[1+j]);
    }
    piplib_int_oppose(Index(*pcontext, nr, nparm), cut[1+nparm]);
    piplib_int_assign(Index(*pcontext, nr+1, nparm), cut[1+nparm]);
    piplib_int_assign(x, cut[0]);
    piplib_int_oppose(Index(*pcontext, nr, nparm+1), x);
    piplib_int_decrement(x, x);
    piplib_int_add(Index(*pcontext, nr+1, nparm+1), x, cut[1+nparm]);

    Flag(*pcontext, nr) = Unknown;
    Flag(*pcontext, nr+1) = Unknown;
    piplib_int_set_si(Denom(*pcontext, nr), 1);
    piplib_int_set_si(Denom(*pcontext, nr+1), 1);
    (*pnparm)++;
    (*pnc) += 2;
    if (PIPLIB_NAME(verbose) > 0) {
	fprintf(PIPLIB_NAME(dump), "enlarged context %d x %d\n", *pnparm, *pnc);
	fflush(PIPLIB_NAME(dump));
    }

    piplib_int_clear(x);
}

static int PIPLIB_NAME(has_cut)(PIPLIB_NAME(Tableau) *context, int nr, int nparm, int p, PIPLIB_NAME(piplib_int_t) *cut)
{
    int row, col;

    for (row = 0; row < nr; ++row) {
	if (piplib_int_ne(Index(context, row, p), cut[1+nparm]))
	    continue;
	if (piplib_int_ne(Index(context, row, nparm), cut[0]))
	    continue;
	for (col = p+1; col < nparm; ++col)
	    if (piplib_int_zero(Index(context, row, col)) == 0)
		break;
	if (col < nparm)
	    continue;
	for (col = 0; col < p; ++col)
	    if (piplib_int_ne(Index(context, row, col), cut[1+col]))
		break;
	if (col < p)
	    continue;
	return 1;
    }
    return 0;
}

/* cut: constant parameters denominator */
static int PIPLIB_NAME(find_parm)(PIPLIB_NAME(Tableau) *context, int nr, int nparm, PIPLIB_NAME(piplib_int_t) *cut)
{
    int p;
    int col;
    int found;

    if (piplib_int_zero(cut[1+nparm-1]) == 0)
	return -1;

    piplib_int_add(cut[0], cut[0], cut[1+nparm]);
    piplib_int_decrement(cut[0], cut[0]);
    for (p = nparm-1; p >= 0; --p) {
	if (piplib_int_zero(cut[1+p]) == 0)
	    break;
	if (!PIPLIB_NAME(has_cut)(context, nr, nparm, p, cut))
	    continue;
	piplib_int_increment(cut[0], cut[0]);
	piplib_int_sub(cut[0], cut[0], cut[1+nparm]);
	for (col = 0; col < 1+nparm+1; ++col)
	    piplib_int_oppose(cut[col], cut[col]);
	found = PIPLIB_NAME(has_cut)(context, nr, nparm, p, cut);
	for (col = 0; col < 1+nparm+1; ++col)
	    piplib_int_oppose(cut[col], cut[col]);
	if (found)
	    return p;
	piplib_int_add(cut[0], cut[0], cut[1+nparm]);
	piplib_int_decrement(cut[0], cut[0]);
    }
    piplib_int_increment(cut[0], cut[0]);
    piplib_int_sub(cut[0], cut[0], cut[1+nparm]);
    return -1;
}

/**
 * @brief integrer
 * 
 * Add a cut to the problem tableau, or return 0 when an integral solution has
 * been found, or -1 when no integral solution exists.
 * 
 * Since integrer may add rows and columns to the problem tableau, its
 * arguments are pointers rather than values. If a cut is constructed, ni
 * increases by 1. If the cut is parametric, nparm increases by 1 and nc
 * increases by 2.
 */

int PIPLIB_NAME(integrer)(PIPLIB_NAME(Tableau)** ptp, PIPLIB_NAME(Tableau)** pcontext,
             int* pnvar, int* pnparm, int* pni, int* pnc, int bigparm) {
  int ncol = *pnvar + *pnparm + 1;
  int nligne = *pnvar + *pni;
  int nparm = *pnparm;
  int nvar = *pnvar;
  int ni = *pni;
  int nc = *pnc;
  PIPLIB_NAME(piplib_int_t) coupure[MAXCOL];
  int i, j, k, ff;
  PIPLIB_NAME(piplib_int_t) x, d;
  int ok_var, ok_const, ok_parm;
  PIPLIB_NAME(piplib_int_t) D;
  int parm;

  PIPLIB_NAME(piplib_int_t) t, delta, /*tau,*/ lambda;

  if (ncol >= MAXCOL) {
    fprintf(stderr, "Too many variables: %d\n", ncol);
    exit(3);
  }
 
 for(i=0; i<=ncol; i++)
   piplib_int_init(coupure[i]);

 piplib_int_init(x); piplib_int_init(d); piplib_int_init(D);
 piplib_int_init(t); piplib_int_init(delta); /*piplib_int_init(tau);*/ piplib_int_init(lambda);


/* search for a non-integral row */
 for(i = 0; i<nvar; i++) {
      piplib_int_assign(D, Denom(*ptp, i));

      // D == 1
      if (piplib_int_one(D)) continue;

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
         piplib_int_floor_div_r(x, Index(*ptp, i, j), D);
         piplib_int_assign(coupure[j], x);
	    if (piplib_int_pos(x))
		ok_var = Pip_True;
          }
/*                          Done for the coefficient of the variables.  */
      piplib_int_oppose(x, Index(*ptp, i, nvar));
      piplib_int_floor_div_r(x, x, D);
      piplib_int_oppose(x, x);
      piplib_int_assign(coupure[nvar], x);
      ok_const = (piplib_int_zero(x) == 0); // x != 0
/*                          This is the constant term                   */
      ok_parm = Pip_False;
      for(j = nvar+1; j<ncol; j++) {
	 /* We assume that the big parameter is divisible by any number. */
	 if (j == bigparm) {
	    piplib_int_set_si(coupure[j], 0);
	    continue;
	 }
	 piplib_int_oppose(x, Index(*ptp, i, j));
	 piplib_int_mod(x, x, D);
	 piplib_int_oppose(coupure[j], x);
	 if (piplib_int_zero(coupure[j]) == 0)
	    ok_parm = Pip_True;
      }
/*                          These are the parametric terms              */

      piplib_int_assign(coupure[ncol], D); /* Just in case */

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
      if(!ok_parm) {
          if(ok_var) {                                   /*     case (d)  */
              if(nligne >= (*ptp)->height) {
		  int d, dth;
	          d = piplib_int_size_in_base_2(D);
                  dth = d;
		  *ptp = PIPLIB_NAME(expanser)(*ptp, nvar, ni, ncol, 0, dth, 0);
                  }
	      /* Find the deepest cut*/
	      if(PIPLIB_NAME(deepest_cut)){
	      piplib_int_oppose(t, coupure[nvar]);
          piplib_int_gcd(delta, t, D);
	      /*piplib_int_div_exact(tau, t, delta);*/
	      piplib_int_div_exact(d, D, delta);
          piplib_int_decrement(t, d);
          PIPLIB_NAME(bezout)(t, /*tau,*/ d, &lambda);
	      piplib_int_gcd(t, lambda, D);
              // t != 1
              while(piplib_int_one(t) == 0) {
		piplib_int_add(lambda, lambda, d);
		piplib_int_gcd(t, lambda, D);
	      }
	      for(j=0; j<nvar; j++){
		piplib_int_mul(t, lambda, coupure[j]);
		piplib_int_floor_div_r(coupure[j], t, D);
	      }
	      piplib_int_mul(t, coupure[nvar], lambda);
	      piplib_int_mod(t, t, D);
	      piplib_int_sub(t, D, t);
	      piplib_int_oppose(coupure[nvar], t);
	      }
                         /* The cut has a negative <<constant>> part      */
              Flag(*ptp, nligne) = Minus; 
              piplib_int_assign(Denom(*ptp, nligne), D);
                         /* Insert the cut */
	      for(j = 0; j<ncol; j++)
	          piplib_int_assign(Index(*ptp, nligne, j), coupure[j]);
                      /* A new row has been added to the problem tableau. */
	      (*pni)++;
              if(PIPLIB_NAME(verbose) > 0) {
		fprintf(PIPLIB_NAME(dump), "just cut ");
                if(PIPLIB_NAME(deepest_cut)){
		  fprintf(PIPLIB_NAME(dump), "Bezout multiplier ");
		  piplib_int_print(PIPLIB_NAME(dump), lambda);
		}
                fprintf(PIPLIB_NAME(dump), "\n");
		k=0;
                for(i=0; i<nvar; i++){
                  if(Flag(*ptp, i) & Unit){
		    fprintf(PIPLIB_NAME(dump), "0 ");
		    k += 2;
		  }
		  else {
		    piplib_int_print(PIPLIB_NAME(dump), Index(*ptp, i, nvar));
		    k += piplib_int_size_in_base_10(Index(*ptp, i, nvar));
		    fprintf(PIPLIB_NAME(dump), "/");
		    k++;
		    piplib_int_print(PIPLIB_NAME(dump), Denom(*ptp, i));
		    k += piplib_int_size_in_base_10(Denom(*ptp, i));
		    fprintf(PIPLIB_NAME(dump), " ");
		    k++;
		    if(k > 60){
		      putc('\n', PIPLIB_NAME(dump));
		      k = 0;
		    }
		  }
		}
		putc('\n', PIPLIB_NAME(dump));
	      }
	      if(PIPLIB_NAME(verbose) > 2) PIPLIB_NAME(tab_display)(*ptp, PIPLIB_NAME(dump));
	      goto clear;
              }
          else {                                         /*   case (b)    */
            nligne = -1; 
            goto clear;
          }
 }
/* In case (e), one has to introduce a new parameter and
   introduce its defining inequalities into the context.
   
   Let the cut be    sum_{j=0}^{nvar-1} c_j x_j + c_{nvar} +             (2)
                     sum_{j=0}^{nparm-1} c_{nvar + 1 + j} p_j >= 0.       */
           
	parm = PIPLIB_NAME(find_parm)(*pcontext, nc, nparm, coupure+nvar);
	if (parm == -1) {
	    PIPLIB_NAME(add_parm)(pcontext, nc, pnparm, pni, pnc, coupure+nvar);
	    parm = nparm;
	}

	assert(ok_var);
          if(nligne >= (*ptp)->height || ncol >= (*ptp)->width) {
              int d, dth, dtw;
             d = piplib_int_size_in_base_2(D);
              dth = d + ni;
	      dtw = d;
	      *ptp = PIPLIB_NAME(expanser)(*ptp, nvar, ni, ncol, 0, dth, dtw);
              }
                         /* Zeroing out the new column seems to be useless
			    since <<expanser>> does it anyway            */
                            
			 /* The cut has a negative <<constant>> part    */
	  Flag(*ptp, nligne) = Minus;
          piplib_int_assign(Denom(*ptp, nligne), D);
              	 /* Insert the cut */
	for (j = 0; j < ncol; j++)
              piplib_int_assign(Index(*ptp, nligne, j), coupure[j]);
	piplib_int_add(Index(*ptp, nligne, nvar+1+parm),
		    Index(*ptp, nligne, nvar+1+parm), coupure[ncol]);
		 /* A new row has been added to the problem tableau.    */
	  (*pni)++;
          goto clear;
      }
 /* The solution is integral.                              */
    nligne = 0;
clear: 
   for(i=0; i <= ncol; i++)
	piplib_int_clear(coupure[i]);
    piplib_int_clear(x); piplib_int_clear(d); piplib_int_clear(D);
    piplib_int_clear(t); /*piplib_int_clear(tau);*/ piplib_int_clear(lambda); piplib_int_clear(delta);
    return nligne;
}

