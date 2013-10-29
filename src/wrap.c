/*
**      wrap -- text reformatter
**      wrap.c
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

/* system */
#include <ctype.h>
#include <errno.h>
#include <limits.h>                     /* for INT_MAX */
#include <stdio.h>
#include <stdlib.h>                     /* for exit(), ... */
#include <string.h>
#include <unistd.h>                     /* for STDIN_FILENO, ... */

/* local */
#include "alias.h"
#include "common.h"
#include "config.h"
#include "getopt.h"
#include "pattern.h"

/* global variable definitions */
char        buf[ LINE_BUF_SIZE ];
FILE*       fin = NULL;                 /* file in */
FILE*       fout = NULL;                /* file out */
int         line_length = DEFAULT_LINE_LENGTH;
char const* me;                         /* executable name */
int         newlines_delimit = 2;       /* # newlines that delimit a para */
int         tab_spaces = DEFAULT_TAB_SPACES;

/* option definitions */
char const* opt_alias = NULL;
char const* opt_config_file = NULL;
bool        opt_eos_delimit = false;    /* end-of-sentence delimits para's? */
char const* opt_fin_name = NULL;        /* file in name */
int         opt_hang_spaces = 0;        /* hanging-indent spaces */
int         opt_hang_tabs = 0;          /* hanging-indent tabs */
int         opt_indt_spaces = 0;        /* indent spaces */
int         opt_indt_tabs = 0;          /* indent tabs */
bool        opt_lead_dot_ignore = false;/* ignore lines starting with '.'? */
int         opt_lead_spaces = 0;        /* leading spaces */
int         opt_lead_tabs = 0;          /* leading tabs */
bool        opt_lead_ws_delimit = false;/* leading whitespace delimit para's? */
int         opt_mirror_spaces = 0;
int         opt_mirror_tabs = 0;
bool        opt_no_config = false;      /* do not read config file */
char const* opt_para_delimiters = NULL; /* additional para delimiter chars */

/* local functions */
static void clean_up();
static void init( int argc, char const *argv[] );
static void print_line( int up_to );
static void process_options( int argc, char const *argv[], char const *opts,
                             int line_no );
static void usage();

/*****************************************************************************/

int main( int argc, char const *argv[] ) {
  int c, prev_c = '\0';                 /* current/previous character */
  int buf_count = 0;                    /* number of characters in buffer */
  int buf_length = 0;                   /* actual length of buffer */
  int wrap_pos = 0;                     /* position at which we can wrap */
  int i, from, to;                      /* work variables */
  /*
  ** Fold all white-space (perhaps multiple characters) to a single space with
  ** the following exceptions:
  **
  ** 1. Force 2 spaces after the end of a sentence that ends a line; sentences
  **    are ended by an "end-of-sentence" character, that is, a period,
  **    question-mark, or an exclamation-point, optionally followed by a
  **    single-quote, double-quote, or a closing parenthesis or bracket.
  **
  ** 2. Allow 2 spaces after the end of a sentence that does not end a line.
  **    This distinction is made so as not to put 2 spaces after a period that
  **    is an abbreviation and not the end of a sentence; periods at the end of
  **    a line will hopefully not be abbreviations.
  **
  ** 3. If neither the -n or -N option is specified, 2 consecutive newline
  **    characters delimit a paragraph and are not folded; more than 2 are
  **    folded.  If -n is specified, newlines receive no special treatment;
  **    if -N is specified, every newline delimits a paragraph.
  **
  ** 4. If the -b option is specified, a line beginning with white-space
  **    delimits a paragraph.
  */

  /*
  ** Number of consecutive newlines.
  */
  int  consec_newlines = 0;

  /*
  ** True only when opt_lead_dot_ignore = true, the current character is a '.',
  ** and the previous character was a '\n' (i.e., the line starts with a '.').
  */
  bool ignore_lead_dot = false;

  /*
  ** Set to 1 when a newline is encountered; decremented otherwise.  Used and
  ** valid only if opt_lead_ws_delimit is true.
  */
  int  newline_counter = 0;

  /*
  ** Number of spaces to output before the next non-white-space character: set
  ** to 1 when we encounter a white-space character and 2 when was_eos_char is
  ** true so as to put 2 characters after the end of a sentence.
  */
  int  put_spaces = 0;

  /*
  ** True only when we should do indenting: set initially to indent the first
  ** line of a paragraph and after newlines_delimit or more consecutive
  ** newlines for subsequent paragraphs.
  */
  bool indent = true;

  /*
  ** True only if the previous character was an end-of-sentence character.
  */
  bool was_eos_char = false;

  /*
  ** True only if the previous character was a paragraph-delimiter character.
  */
  bool was_para_delim_char = false;

  /***************************************************************************/

  init( argc, argv );

  for ( ; (c = getc( fin )) != EOF; prev_c = c ) {

    /*************************************************************************
     *  HANDLE NEWLINE(s)
     *************************************************************************/

    if ( c == '\n' ) {
      newline_counter = 1;
      if ( ++consec_newlines >= newlines_delimit ) {
        /*
        ** At least newlines_delimit consecutive newlines: print text, if any;
        ** also set up for indenting.
        */
        goto delimit_paragraph;
      }
      if ( opt_eos_delimit && was_eos_char ) {
        /*
        ** If end-of-sentence characters delimit paragraphs and the previous
        ** character was an end-of-sentence character, delimit the paragraph.
        */
        goto delimit_paragraph;
      }
      if ( was_eos_char ) {
        /*
        ** We are joining a line after the end of a sentence: force 2 spaces.
        */
        put_spaces = 2;
        continue;
      }
    } else {
      consec_newlines = 0;
      --newline_counter;
    }

    /*************************************************************************
     *  HANDLE WHITE-SPACE
     *************************************************************************/

    if ( isspace( c ) ) {
      /*
      ** If newline_counter == 0, the previous character was a newline;
      ** therefore, this white-space character is at the beginning of a line.
      */
      if ( opt_lead_ws_delimit && newline_counter == 0 )
        goto delimit_paragraph;
      /*
      ** If end-of-sentence characters delimit paragraphs and the previous
      ** character was an end-of-sentence character, delimit the paragraph.
      */
      if ( opt_eos_delimit && was_eos_char )
        goto delimit_paragraph;
      /*
      ** If the previous character was a paragraph-delimiter character (this is
      ** set only if opt_para_delimiters was set), delimit the paragraph.
      */
      if ( was_para_delim_char )
        goto delimit_paragraph;
      /*
      ** Only if we are not at the beginning of a line, remember to insert 1
      ** space later; allow 2 after the end of a sentence.
      */
      if ( buf_count && put_spaces < 1 + was_eos_char )
        ++put_spaces;
      continue;
    }

    /*************************************************************************
     *  DISCARD CONTROL CHARACTERS
     *************************************************************************/

    if ( iscntrl( c ) )
      continue;

    /*************************************************************************
     *  HANDLE LEADING-DOT, END-OF-SENTENCE, AND PARAGRAPH-DELIMITERS
     *************************************************************************/

    if ( opt_lead_dot_ignore && c == '.' && prev_c == '\n' ) {
      ignore_lead_dot = true;
      goto delimit_paragraph;
    }

    /*
    ** Treat a quote or a closing parenthesis or bracket as an end-of-sentence
    ** character if it was preceded by a regular end-of-sentence character.
    */
    if ( !(was_eos_char && strchr( "'\")]", c )) )
      was_eos_char = (strchr( ".?!", c ) != NULL);

    if ( opt_para_delimiters )
      was_para_delim_char = (strchr( opt_para_delimiters, c ) != NULL);

    /*************************************************************************
     *  INSERT SPACES
     *************************************************************************/

    if ( put_spaces ) {
      if ( buf_count ) {
        /*
        ** Mark position at a space to perform a wrap if necessary.
        */
        wrap_pos = buf_count;
        buf_length += put_spaces;
        do {
          buf[ buf_count++ ] = ' ';
        } while ( --put_spaces );
      } else {
        /*
        ** Never put spaces at the beginning of a line.
        */
        put_spaces = 0;
      }
    }

    /*************************************************************************
     *  PERFORM FIRST-LINE-OF-PARAGRAPH INDENTATION
     *************************************************************************/

    if ( indent ) {
      for ( i = 0; i < opt_indt_tabs; ++i )
        buf[ buf_count++ ] = '\t';
      for ( i = 0; i < opt_indt_spaces; ++i )
        buf[ buf_count++ ] = ' ';
      buf_length += opt_indt_tabs * tab_spaces + opt_indt_spaces;
      indent = false;
    }

    /*************************************************************************
     *  INSERT NON-SPACE CHARACTER
     *************************************************************************/

    buf[ buf_count++ ] = c, ++buf_length;
    if ( buf_length < line_length ) {
      /*
      ** We haven't exceeded the line length yet.
      */
      continue;
    }

    /*************************************************************************
     *  EXCEEDED LINE LENGTH; PRINT LINE OUT
     *************************************************************************/

    print_line( wrap_pos );

    /*
    ** Prepare to slide the partial word to the left where we can pick up from
    ** where we left off the next time around.
    */
    to = 0;

    /*
    ** Perform hang indentation first.
    */
    for ( i = 0; i < opt_hang_tabs; ++i )
      buf[ to++ ] = '\t';
    for ( i = 0; i < opt_hang_spaces; ++i )
      buf[ to++ ] = ' ';
    buf_length = opt_hang_tabs * tab_spaces + opt_hang_spaces;

    /*
    ** Now slide the partial word to the left.
    */
    for ( from = wrap_pos + 1; from < buf_count; ++from ) {
      buf[ to ] = buf[ from ];
      if ( !isspace( buf[ to ] ) )
        ++to, ++buf_length;
    }
    buf_count = to;
    wrap_pos = 0;

    continue;

    /*************************************************************************/

delimit_paragraph:
    if ( buf_count ) {
      print_line( buf_count );
      buf_count = buf_length = 0;
    }
    put_spaces = 0;
    was_eos_char = was_para_delim_char = false;
    if ( ignore_lead_dot ) {
      /*
      ** The line starts with a leading dot and opt_lead_dot_ignore = true:
      ** read/write the line as-is.
      */
      buf[0] = '.';
      if ( !fgets( buf + 1, LINE_BUF_SIZE - 1, fin ) ) {
        if ( ferror( fin ) )
          ERROR( EXIT_READ_ERROR );
        continue;
      }
      fputs( buf, fout );
      if ( ferror( fout ) )
        ERROR( EXIT_WRITE_ERROR );
    } else {
      if ( consec_newlines == 2 || 
          (consec_newlines > 2 && newlines_delimit == 1) ) {
        putc( c, fout );
        if ( ferror( fout ) )
          ERROR( EXIT_WRITE_ERROR );
      }
      indent = true;
    }
  } /* while */

  if ( ferror( fin ) )
    ERROR( EXIT_READ_ERROR );
  if ( buf_count )                      /* print left-over text */
    print_line( buf_count );
  clean_up();
  exit( EXIT_OK );
}

/*****************************************************************************/

static void clean_up() {
  alias_cleanup();
  pattern_cleanup();
}

static void init( int argc, char const *argv[] ) {
  char const opts[] = "a:bc:Cdef:F:h:H:i:I:l:m:M:nNo:p:s:S:t:v";

  me = base_name( argv[0] );
  process_options( argc, argv, opts, 0 );
  atexit( clean_up );
  argc -= optind, argv += optind;
  if ( argc )
    usage();

  if ( !opt_no_config && (opt_alias || opt_fin_name ) ) {
    alias_t const *alias = NULL;
    opt_config_file = read_config( opt_config_file );
    if ( opt_alias ) {
      if ( !(alias = alias_find( opt_alias )) ) {
        fprintf(
          stderr, "%s: \"%s\": no such alias in %s\n",
          me, opt_alias, opt_config_file
        );
        exit( EXIT_USAGE );
      }
    } else if ( opt_fin_name ) {
      alias = pattern_find( opt_fin_name );
    }
    if ( alias )
      process_options( alias->argc, alias->argv, opts, alias->line_no );
  }

  if ( !fin )
    fin = stdin;
  if ( !fout )
    fout = stdout;

  line_length -=
    2 * (opt_mirror_tabs * tab_spaces + opt_mirror_spaces) +
    opt_lead_tabs * tab_spaces + opt_lead_spaces;

  opt_lead_tabs   += opt_mirror_tabs;
  opt_lead_spaces += opt_mirror_spaces;
}

static void print_line( int up_to ) {
  int i;
  for ( i = 0; i < opt_lead_tabs; ++i )
    putc( '\t', fout );
  for ( i = 0; i < opt_lead_spaces; ++i )
    putc( ' ', fout );
  buf[ up_to ] = '\0';
  fprintf( fout, "%s\n", buf );
  if ( ferror( fout ) )
    ERROR( EXIT_WRITE_ERROR );
}

static void process_options( int argc, char const *argv[], char const *opts,
                             int line_no ) {
  int opt;
  optind = 1;
  opterr = 1;

  while ( (opt = pjl_getopt( argc, argv, opts )) != EOF ) {
    if ( line_no && strchr( "acCfFov", opt ) ) {
      fprintf(
        stderr, "%s: %s:%d: '%c': option not allowed in config file\n",
        me, opt_config_file, line_no, opt
      );
      exit( EXIT_CONFIG_ERROR );
    }
    switch ( opt ) {
      case 'a': opt_alias           = optarg;               break;
      case 'b': opt_lead_ws_delimit = true;                 break;
      case 'c': opt_config_file     = optarg;               break;
      case 'C': opt_no_config       = true;                 break;
      case 'd': opt_lead_dot_ignore = true;                 break;
      case 'e': opt_eos_delimit     = true;                 break;
      case 'f':
        if ( !(fin = fopen( optarg, "r" )) ) {
          fprintf( stderr, "%s: \"%s\": %s\n", me, optarg, strerror( errno ) );
          exit( EXIT_READ_OPEN );
        }
        /* no break; */
      case 'F': opt_fin_name        = base_name( optarg );  break;
      case 'h': opt_hang_tabs       = check_atou( optarg ); break;
      case 'H': opt_hang_spaces     = check_atou( optarg ); break;
      case 'i': opt_indt_tabs       = check_atou( optarg ); break;
      case 'I': opt_indt_spaces     = check_atou( optarg ); break;
      case 'l': line_length         = check_atou( optarg ); break;
      case 'm': opt_mirror_tabs     = check_atou( optarg ); break;
      case 'M': opt_mirror_spaces   = check_atou( optarg ); break;
      case 'n': newlines_delimit    = INT_MAX;              break;
      case 'N': newlines_delimit    = 1;                    break;
      case 'o':
        if ( !(fout = fopen( optarg, "w" )) ) {
          fprintf( stderr, "%s: \"%s\": %s\n", me, optarg, strerror( errno ) );
          exit( EXIT_WRITE_OPEN );
        }
        break;
      case 'p': opt_para_delimiters = optarg;               break;
      case 's': tab_spaces          = check_atou( optarg ); break;
      case 'S': opt_lead_spaces     = check_atou( optarg ); break;
      case 't': opt_lead_tabs       = check_atou( optarg ); break;
      case 'v':
        fprintf( stderr, "%s %s\n", me, WRAP_VERSION );
        exit( EXIT_OK );
      default:
        usage();
    } /* switch */
  } /* while */
}

static void usage() {
  fprintf( stderr, "usage: %s [-bCdenNv] [-l line-length] [-p para-delim-chars] [-s tab-spaces]\n", me );
  fprintf( stderr, "\t[-{fF} input-file] [-o output-file]    [-c config-file]\n" );
  fprintf( stderr, "\t[-t leading-tabs]  [-S leading-spaces]\n" );
  fprintf( stderr, "\t[-m mirror-tabs]   [-M mirror-spaces]\n" );
  fprintf( stderr, "\t[-i indent-tabs]   [-I indent-spaces]\n" );
  fprintf( stderr, "\t[-h hanging-tabs]  [-H hanging-spaces]\n" );
  exit( EXIT_USAGE );
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */