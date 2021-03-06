/******************************************************************************
 *                     PIP : Parametric Integer Programming                   *
 ******************************************************************************
 *                                piplibMP.h                                  *
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
 * for more details.							      *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with this library; if not, write to the Free Software Foundation,    *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA         *
 *                                                                            *
 * Written by Cedric Bastoul                                                  *
 *                                                                            *
 ******************************************************************************/

/* Ce fichier d'inclusions regroupe tout les elements accessibles aux
 * utilisateurs de la PipLib. Premiere version du 29 juillet 2001.
 */ 

#ifndef PIPLIB_MP_H
#define PIPLIB_MP_H

#undef PIPLIB_INT_SP
#undef PIPLIB_INT_DP
#undef PIPLIB_INT_GMP
#undef PIPLIB_INT_OSL

#define PIPLIB_INT_GMP 1

// Compatibility with old version
#undef LINEAR_VALUE_IS_LONG
#undef LINEAR_VALUE_IS_LONGLONG
#undef LINEAR_VALUE_IS_MP
#define LINEAR_VALUE_IS_MP 1

#include <piplib/piplib.h>

#endif
