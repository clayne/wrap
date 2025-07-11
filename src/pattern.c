/*
**      wrap -- text reformatter
**      src/pattern.c
**
**      Copyright (C) 2013-2024  Paul J. Lucas
**
**      This program is free software: you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation, either version 3 of the License, or
**      (at your option) any later version.
**
**      This program is distributed in the hope that it will be useful,
**      but WITHOUT ANY WARRANTY; without even the implied warranty of
**      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**      GNU General Public License for more details.
**
**      You should have received a copy of the GNU General Public License
**      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file
 * Defines functions to manipulate configuration file filename patterns.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "pattern.h"
#include "common.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <fnmatch.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// @endcond

/**
 * @addtogroup patterns-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

// local constant definitions
static size_t const PATTERN_ALLOC_SIZE = 10;

// local variable definitions
static pattern_t   *patterns = NULL;    // global list of patterns
static size_t       patterns_size = 0;  // number of patterns

// local functions
static void   pattern_cleanup( void );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Frees all memory used by a pattern.
 *
 * @param pattern The pattern to free.
 */
static inline void pattern_free( pattern_t *pattern ) {
  FREE( pattern->pattern );
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Allocates a new pattern.
 *
 * @return Returns a new, uninitialzed pattern.
 */
NODISCARD
static pattern_t* pattern_alloc( void ) {
  RUN_ONCE ATEXIT( &pattern_cleanup );

  static size_t patterns_cap = 0;       // number of patterns allocated

  if ( patterns_size + 1 > patterns_cap ) {
    patterns_cap += PATTERN_ALLOC_SIZE;
    REALLOC( patterns, pattern_t, patterns_cap );
  }
  PERROR_EXIT_IF( patterns == NULL, EX_OSERR );
  return &patterns[ patterns_size++ ];
}

/**
 * Cleans-up all pattern data.
 */
static void pattern_cleanup( void ) {
  while ( patterns_size > 0 )
    pattern_free( &patterns[ --patterns_size ] );
  free( patterns );
}

////////// extern functions ///////////////////////////////////////////////////

#ifndef NDEBUG
void dump_patterns( void ) {
  for ( size_t i = 0; i < patterns_size; ++i ) {
    if ( i == 0 )
      printf( "[PATTERNS]\n" );
    pattern_t const *const pattern = &patterns[i];
    printf( "%s = %s\n", pattern->pattern, pattern->alias->argv[0] );
  } // for
}
#endif /* NDEBUG */

alias_t const* pattern_find( char const *file_name ) {
  assert( file_name != NULL );
  for ( size_t i = 0; i < patterns_size; ++i )
    if ( fnmatch( patterns[i].pattern, file_name, 0 ) == 0 )
      return patterns[i].alias;
  return NULL;
}

void pattern_parse( char const *line, char const *conf_file,
                    unsigned line_no ) {
  assert( line != NULL );
  assert( conf_file != NULL );
  assert( line_no > 0 );

  pattern_t *const pattern = pattern_alloc();

  // pattern line: <pattern>[<ws>]=[<ws>]<alias>
  //        parts:     1      2   3  4      5   

  // part 1: pattern
  size_t const span = strcspn( line, " \t=" );
  assert( span > 0 );
  pattern->pattern = strndup( line, span );
  line += span;

  // part 2: whitespace
  SKIP_CHARS( line, WS_STR );
  if ( *line == '\0' )
    fatal_error( EX_CONFIG, "%s:%u: '=' expected\n", conf_file, line_no );

  // part 3: =
  if ( *line != '=' ) {
    fatal_error( EX_CONFIG,
      "%s:%u: '%c': unexpected character; '=' expected\n",
      conf_file, line_no, *line
    );
  }
  ++line;                               // skip '='

  // part 4: whitespace
  SKIP_CHARS( line, WS_STR );
  if ( *line == '\0' ) {
    fatal_error( EX_CONFIG,
      "%s:%u: alias name expected after '='\n",
      conf_file, line_no
    );
  }

  // part 5: alias
  alias_t const *const alias = alias_find( line );
  if ( alias == NULL ) {
    fatal_error( EX_CONFIG,
      "%s:%u: \"%s\": no such alias\n", conf_file, line_no, line
    );
  }
  pattern->alias = alias;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
