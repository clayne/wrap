/*
**      wrap -- text reformatter
**      common.h -- Common declarations.
**
**      Copyright (C) 1996-2013  Paul J. Lucas
**
**      This program is free software; you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation; either version 2 of the Licence, or
**      (at your option) any later version.
** 
**      This program is distributed in the hope that it will be useful,
**      but WITHOUT ANY WARRANTY; without even the implied warranty of
**      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**      GNU General Public License for more details.
** 
**      You should have received a copy of the GNU General Public License
**      along with this program; if not, write to the Free Software
**      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef wrap_common_H
#define wrap_common_H

/*****************************************************************************/

#define WRAP_VERSION  "2.12"

#ifndef __cplusplus
# ifdef bool
#   undef bool
# endif
# ifdef true
#   undef true
# endif
# ifdef false
#   undef false
# endif
# define bool   int
# define true   1
# define false  0
#endif /* __cplusplus */

/*****************************************************************************/

#endif /* wrap_common_H */
/* vim:set et sw=2 ts=2: */
