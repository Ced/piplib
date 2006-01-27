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
 * version.							              *
 *                                                                            *
 * This software is distributed in the hope that it will be useful, but       *
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License   *
 * for more details.							      *
 *                                                                            *
 * You should have received a copy of the GNU General Public License along    *
 * with software; if not, write to the Free Software Foundation, Inc.,        *
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA                     *
 *                                                                            *
 * Written by Paul Feautrier                                                  *
 *                                                                            *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <piplib/piplib.h>

/*  The routines in this file are used to build a Gomory cut from
    a non-integral row of the problem tableau                             */

extern long int cross_product, limit;
extern int verbose;
extern FILE * dump;

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
	 {if(ff & Unit)return(True);
          if(Index(tp, i, bigparm) != D) return(True);
	 }
      }
 return(False);
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

#if defined(LINEAR_VALUE_IS_MP)
int integrer(ptp, pcontext, pnvar, pnparm, pni, pnc)
Tableau **ptp, **pcontext;
int *pnvar, *pnparm, *pni, *pnc;
#else
int integrer(ptp, pcontext, D, pnvar, pnparm, pni, pnc)
Tableau **ptp, **pcontext;
int *pnvar, *pnparm, *pni, *pnc;
Entier D;
#endif
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
 #if defined(LINEAR_VALUE_IS_MP)
 Entier D;
 #endif
 
 #if defined(LINEAR_VALUE_IS_MP)
 for(i=0; i<=ncol; i++)
   mpz_init(coupure[i]);

 for(i=0; i<=nparm+1; i++){
   mpz_init(discrp[i]);
   mpz_init(discrm[i]);
 }

 mpz_init(x); mpz_init(d); mpz_init(D);
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
      ok_var = False;
      for(j = 0; j<nvar; j++) {
         #if defined(LINEAR_VALUE_IS_MP)
         mpz_fdiv_r(x, Index(*ptp, i, j), D);
         mpz_set(coupure[j], x);
         #else
         x = coupure[j] = mod(Index(*ptp, i, j), D);
         #endif
          if(x > 0) ok_var = True;
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
      ok_parm = False;
      for(j = nvar+1; j<ncol; j++) {
         #if defined(LINEAR_VALUE_IS_MP)
         mpz_neg(x, Index(*ptp, i, j));
         mpz_fdiv_r(x, x, D);
         mpz_neg(x, x);
         mpz_set(coupure[j], x);
         if(mpz_cmp_ui(x, 0) != 0) ok_parm = True;
         #else
         x = coupure[j] = - mod(- Index(*ptp, i, j), D);         /* (1) */
         if(x != 0) ok_parm = True;
         #endif
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
             #if defined(LINEAR_VALUE_IS_MP)
             goto clear;
             #else
             return(nligne);
             #endif
              }
          else                               /*   case (b)    */
          #if defined(LINEAR_VALUE_IS_MP)
          { nligne = -1; 
            goto clear;
          }
          #else
          return -1;  
          #endif
/* In cases (c) and (e), one has to introduce a new parameter and
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
      if(nc+2 > (*pcontext)->height || nparm+1 > (*pcontext)->width) {
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

      if(ok_var) {                                 /*   case (e)         */
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
                                                  /*  case (c)          */
                        /* The new parameter has already been defined as a
			   quotient. It remains to express that the
			   remainder of that division is zero           */
      sol_if();
      sol_forme(nparm + 2);
      for (j = 0; j < nparm+1 ; j++)
	  sol_val(discrm[j], UN);
          #if defined(LINEAR_VALUE_IS_MP)
          mpz_neg(x, UN);
          sol_val(x, UN);
          #else
          sol_val(-UN, UN);
          #endif
      sol_nil();    /* No solution if the division is not even      */
			/* Add a new column */
      if(ncol+1 >= (*ptp)-> width) {
	  int dtw;
          #if defined(LINEAR_VALUE_IS_MP)
          dtw = mpz_sizeinbase(D, 2);
          #else
	  dtw = llog(D);
          #endif
	  *ptp = expanser(*ptp, *pnvar, *pni, ncol, 0, 0, dtw);
	  }
	  /* The new column is zeroed out by <<expanser>>          */
/* Let c be the coefficient of parameter p in the i row. In <<coupure>>,
   this parameter has coefficient  - mod(-c, D). In <<discrp>>, this same
   parameter has coefficient mod(-c, D). The sum c + mod(-c, D) is obviously
   divisible by D.                                                      */

      for (j = 0; j <= nparm; j++)
          #if defined(LINEAR_VALUE_IS_MP)
          mpz_add(Index(*ptp, i, j + nvar + 1), 
                  Index(*ptp, i, j + nvar + 1), discrp[j]);
          #else
	  Index(*ptp, i, j + nvar + 1) += discrp[j];
          #endif
      tab_display(*ptp, stderr);
      exit(0);
      continue;
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
 return(nligne);
 #else
 return 0;
 #endif
}

