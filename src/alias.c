/*
**      wrap -- text reformatter
**      alias.c
**
**      Copyright (C) 2013-2016  Paul J. Lucas
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
**      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// local
#include "alias.h"
#include "common.h"
#include "util.h"

// standard
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

// local constant definitions
static size_t const ALIAS_ALLOC_DEFAULT         = 10;
static size_t const ALIAS_ALLOC_INCREMENT       = 10;
static size_t const ALIAS_ARGV_ALLOC_DEFAULT    = 10;
static size_t const ALIAS_ARGV_ALLOC_INCREMENT  = 10;
static char const   ALIAS_NAME_CHARS[]          = "abcdefghijklmnopqrstuvwxyz"
                                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                  "0123456789_";

// local variable definitions
static alias_t     *aliases = NULL;     // global list of aliases
static size_t       n_aliases = 0;      // number of aliases

////////// local functions ////////////////////////////////////////////////////

/**
 * Allocates a new alias.
 *
 * @return Returns a new, uninitialzed alias.
 */
static alias_t* alias_alloc( void ) {
  static size_t n_aliases_alloc = 0;    // number of aliases allocated

  if ( !n_aliases_alloc ) {
    n_aliases_alloc = ALIAS_ALLOC_DEFAULT;
    aliases = MALLOC( alias_t, n_aliases_alloc );
  } else if ( n_aliases > n_aliases_alloc ) {
    n_aliases_alloc += ALIAS_ALLOC_INCREMENT;
    REALLOC( aliases, alias_t, n_aliases_alloc );
  }
  if ( !aliases )
    PERROR_EXIT( EX_OSERR );
  return &aliases[ n_aliases++ ];
}

/**
 * Checks the most-recently-added alias against all previous aliases for a
 * duplicate name.
 *
 * @param conf_file The configuration file path-name.
 * @param line_no The line-number within \a conf_file.
 */
static void alias_check_dup( char const *conf_file, unsigned line_no ) {
  assert( conf_file );
  if ( n_aliases > 1 ) {
    //
    // The number of aliases is assumed to be small, so linear search is good
    // enough.
    //
    int i = (int)n_aliases - 1;
    char const *const last_name = aliases[i].argv[0];
    while ( --i >= 0 ) {
      if ( strcmp( aliases[i].argv[0], last_name ) == 0 )
        PMESSAGE_EXIT( EX_CONFIG,
          "%s:%u: \"%s\": duplicate alias name (first is on line %u)\n",
          conf_file, line_no, last_name, aliases[i].line_no
        );
    } // while
  }
}

/**
 * Frees all memory used by an alias.
 *
 * @param alias The alias to free.
 */
static void alias_free( alias_t *alias ) {
  assert( alias );
  while ( alias->argc > 0 )
    free( (void*)alias->argv[ --alias->argc ] );
  free( alias->argv );
}

////////// extern functions ///////////////////////////////////////////////////

void alias_cleanup( void ) {
  while ( n_aliases )
    alias_free( &aliases[ --n_aliases ] );
  free( aliases );
}

alias_t const* alias_find( char const *name ) {
  assert( name );
  for ( size_t i = 0; i < n_aliases; ++i )
    if ( strcmp( aliases[i].argv[0], name ) == 0 )
      return &aliases[i];
  return NULL;
}

void alias_parse( char const *line, char const *conf_file, unsigned line_no ) {
  assert( line );
  assert( conf_file );
  assert( line_no );

  size_t n_argv_alloc = ALIAS_ARGV_ALLOC_DEFAULT;
  alias_t *const alias = alias_alloc();
  alias->line_no = line_no;
  alias->argc = 1;
  alias->argv = MALLOC( char const*, n_argv_alloc );

  // alias line: <name>[<ws>]={[<ws>]<options>}...
  //      parts:   1     2   3   4       5        

  // part 1: name
  size_t span = strspn( line, ALIAS_NAME_CHARS );
  alias->argv[0] = strndup( line, span );
  alias_check_dup( conf_file, line_no );
  line += span;

  // part 2: whitespace
  SKIP_WS( line );
  if ( !*line )
    PMESSAGE_EXIT( EX_CONFIG, "%s:%u: '=' expected\n", conf_file, line_no );

  // part 3: =
  if ( *line != '=' )
    PMESSAGE_EXIT( EX_CONFIG,
      "%s:%u: '%c': unexpected character; '=' expected\n",
      conf_file, line_no, *line
    );
  ++line;                               // skip '='

  // parts 4 & 5: whitespace, options
  for ( ;; ) {
    SKIP_WS( line );
    if ( !*line ) {
      if ( alias->argc == 1 )
        PMESSAGE_EXIT( EX_CONFIG,
          "%s:%u: option(s) expected after '='\n",
          conf_file, line_no
        );
      break;
    }
    if ( alias->argc == n_argv_alloc ) {
      n_argv_alloc += ALIAS_ARGV_ALLOC_INCREMENT;
      REALLOC( alias->argv, char const*, n_argv_alloc );
    }
    span = strcspn( line, " \t" );
    alias->argv[ alias->argc++ ] = strndup( line, span );
    line += span;
  } // for

  if ( n_argv_alloc > alias->argc + 1 )
    REALLOC( alias->argv, char const*, alias->argc + 1 );
  alias->argv[ alias->argc ] = NULL;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
