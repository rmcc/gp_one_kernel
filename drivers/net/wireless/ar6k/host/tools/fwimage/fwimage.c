/*
 * ------------------------------------------------------------------------------
 * Copyright (c) 2008 Atheros Corporation.  All rights reserved.
 * 
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//
 * ------------------------------------------------------------------------------
 * ==============================================================================
 * Author(s): ="Atheros"
 * ==============================================================================
 *       
 * fwimage.c - build AR6XXX firmware image
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

#include "crc32.h"
#include "athloader.h"

#define BUFSIZE (4*1024*1024)

struct symbol {
  struct symbol *next;
  char          *name;
  int            offset;
  int            lineno;
  int            defined;
} *shead;

unsigned char fwbuffer[ BUFSIZE ];

char scriptline[2048];
char fname[1024];

int  lineno = 0;
int  offset = 0;

/* Assembler functions and data */
int reg_or_const(void);
int reg_only(void);
int getbinary(void);
int getoffset(void);
struct symbol *lookupsymbol( char *name );

typedef struct ops {
  char          *mnemonic;
  unsigned char  opcode;
  int            (*getargs)(void);
} optable;

optable scriptops[] = {
  {"rl",     0x10, reg_or_const},
  {"ri",     0x10, reg_or_const},   /* Yes, those two are synonyms */
  {"r|",     0x20, reg_or_const},
  {"r&",     0x30, reg_or_const},
  {"add",    0x40, reg_or_const},
  {"rs",     0x50, reg_only},
  {"cla",    0x50, NULL},           /* Clear acc, store command with pseudo hex arg */
  {"shift",  0x60, reg_or_const},
  {"not",    0x70, reg_only},
  {"neg",    0x70, NULL},           /* Negate acc - not opcode but pseudo hex arg   */
  {"trr",    0x80, reg_or_const},
  {"trw",    0x90, reg_or_const},
  {"trx",    0xa0, reg_or_const},
  {"abort",  0xb0, reg_only},
  {"tdone",  0xb0, NULL},
  {"cmpr",   0xc0, reg_or_const},
  {"cmpi",   0xc0, reg_or_const},  /* Those two are also synonyms */
  {"ddump",  0xd0, NULL},
  {"lb",     0xd1, getbinary},
  {"lr",     0xd2, getbinary},
  {"lp",     0xd3, getbinary},
  {"jmp",    0xe0, getoffset},
  {"jne",    0xe1, getoffset},
  {"je",     0xe2, getoffset},
  
  {NULL, 0, NULL }
};

int inc_offset( void )
{
    int oldoffset = offset;
    
    offset++;
    if( offset >= (BUFSIZE-1) )
    {
        printf("Firmware buffer overflow.\n");
        exit(1);
    }
    return( oldoffset );
}

/* The following functions are getting arguments from current
 * script line, convert as necessary and store into image buffer
 *
 * Return: all routines return zero if successful.
 */
/* reg_or_const - expects register or hex constant, aborts otherwise
 */
int reg_or_const(void)
{
    char         *cp = scriptline;
    unsigned int  hexconst;
    
    /* skip opcode and space after it */
    while( cp && *cp && !isspace(*cp) ) cp++;
    while( cp && *cp && isspace(*cp) ) cp++;
    
    if(! *cp)
    {
        printf("This opcode requires an argument.");
        return(1);
    }

    if( (*cp >= '1') && (*cp <= '9') )
    { /* register 1-9 */
        fwbuffer[ offset ] |= (*cp - '0') & 0xF;
        inc_offset();
        return(0);
    }
    
    if( (tolower(*cp) >= 'a') && (tolower(*cp) <= 'f') )
    { /* register a-f */
        fwbuffer[ offset ] |= (tolower(*cp) - 'a' + 10) & 0xF;
        inc_offset();
        return(0);
    }
    
    if( (*cp == '0') && (tolower(*(cp+1)) == 'x') )
    { /* hex constant */
        hexconst = strtol(cp+2, NULL, 16);
        inc_offset();
        fwbuffer[ offset ] = hexconst & 0xFF; inc_offset(); hexconst >>= 8;
        fwbuffer[ offset ] = hexconst & 0xFF; inc_offset(); hexconst >>= 8;
        fwbuffer[ offset ] = hexconst & 0xFF; inc_offset(); hexconst >>= 8;
        fwbuffer[ offset ] = hexconst & 0xFF; inc_offset();
        
        return(0);
    }
    /* None of the above */
    printf("Unrecognised argument. Must be register or hex constant with leading 0x\n");
    return(1);
}

/* reg_only - expects register argument, aborts otherwise
*/
int reg_only(void)
{
    char         *cp = scriptline;
    
    /* skip opcode and space after it */
    while( cp && *cp && !isspace(*cp) ) cp++;
    while( cp && *cp && isspace(*cp) ) cp++;
    
    if(! *cp)
    {
        printf("This opcode requires an argument.");
        return(1);
    }

    if( (*cp >= '1') && (*cp <= '9') )
    { /* register 1-9 */
        fwbuffer[ offset ] |= (*cp - '0') & 0xF;
        inc_offset();
        return(0);
    }
    
    if( (tolower(*cp) >= 'a') && (tolower(*cp) <= 'f') )
    { /* register a-f */
        fwbuffer[ offset ] |= (tolower(*cp) - 'a' + 10) & 0xF;
        inc_offset();
        return(0);
    }
    /* None of the above */
    printf("Unrecognised argument. Must be register [1-f]");
    return(1);
}

/* getbinary - get filespec, read the file, embed into image
*/
int getbinary(void)
{
    struct stat   statbuf;
    FILE         *fileni;
    int           ret, byte, size;
    char         *cp = scriptline;
    
    /* skip opcode and space after it */
    while( cp && *cp && !isspace(*cp) ) cp++;
    while( cp && *cp && isspace(*cp) ) cp++;
    
    ret = stat( cp, &statbuf );
    if( ret )
    {
      printf("Cannot stat file \"%s\" -- %s\n", cp, strerror(errno));
      return(1);
    }
    
    /* store binary length into image */
    inc_offset();
    size = statbuf.st_size;
    fwbuffer[ offset ] = statbuf.st_size & 0xFF; inc_offset(); statbuf.st_size >>= 8;
    fwbuffer[ offset ] = statbuf.st_size & 0xFF; inc_offset(); statbuf.st_size >>= 8;
    fwbuffer[ offset ] = statbuf.st_size & 0xFF; inc_offset(); statbuf.st_size >>= 8;
    fwbuffer[ offset ] = statbuf.st_size & 0xFF; inc_offset(); 
    
    /* copy file data into image */
    if( (fileni = fopen(cp, "r")) == NULL)
    {
      printf("Cannot open file \"%s\" -- %s\n", cp, strerror(errno));
      return(1);
    }
    
    ret = 0;
    while( (byte = getc(fileni)) != EOF )
    {
        fwbuffer[ offset ] = byte & 0xFF;
        inc_offset();
        ret++;
    }
    if( ret != size )
    {
        printf("File \"%s\" stated size %d and actual data size %d mismatch\n", size, ret);
        return(1);
    }
    fclose(fileni);
    return(0);
}

/* getoffset - get label, find in symbol table, embed binary offset (jumps)
*/
int getoffset(void)
{
    char          *cp = scriptline;
    struct symbol *tmp;
    unsigned int   labeloffset;
    
    /* skip opcode and space after it */
    while( cp && *cp && !isspace(*cp) ) cp++;
    while( cp && *cp && isspace(*cp) ) cp++;
    
    /* cp must point to label name now */
    tmp = lookupsymbol(cp);
    inc_offset();
    
    if( tmp )
    { /* label is known, store real offset */
        labeloffset = tmp->offset;
        fwbuffer[ offset ] = labeloffset & 0xFF; inc_offset(); labeloffset >>= 8;
        fwbuffer[ offset ] = labeloffset & 0xFF; inc_offset(); labeloffset >>= 8;
        fwbuffer[ offset ] = labeloffset & 0xFF; inc_offset(); labeloffset >>= 8;
        fwbuffer[ offset ] = labeloffset & 0xFF; inc_offset(); 
        return(0);
    }
    else
    { /* Label undefined, allocate space and store undefined label for later 
       * resolving */
       
       tmp = (struct symbol *) malloc( sizeof(struct symbol) );
       if( !tmp )
       {
           printf("No memory for unresolved label %s", cp);
           return(1);
       }
       
       tmp->name = malloc( strlen(cp) + 1 );
       if( !tmp->name )
       {
           printf("No memory for unresolved label %s", cp);
           free(tmp);
           return(1);
       }
       
       strcpy( tmp->name, cp );
       tmp->offset  = offset;
       tmp->lineno  = lineno;
       tmp->defined = 0;
       tmp->next    = shead;
       shead       = tmp;
       
       offset += 4;
    }
    return(0);
}

/* addsymbol - allocate symbol and add it to symbol table linklist
 *
 * return zero if no memory left
 */
int addsymbol( char *name )
{
  struct symbol *tmp = (struct symbol *) malloc( sizeof(struct symbol) );
  
  if( !tmp )
    return(0);
    
  tmp->name = malloc( strlen(name) + 1 );
  
  if( !tmp->name )
  {
    free(tmp);
    return(0);
  }
  
  strcpy( tmp->name, name );
  tmp->offset  = offset;
  tmp->lineno  = lineno;
  tmp->defined = 1;
  tmp->next    = shead;
  shead        = tmp;     /* Yes, it's a stack. */
  
  return(1);
}

/* lookupsymbol - finds symbol by name, returns pointer to it,
 *                returns NULL if symbol not found
 */
struct symbol *lookupsymbol( char *name )
{
  struct symbol *tmp = shead;
  
  while( tmp )
  {
      if( !strcmp( tmp->name, name ) )
          if( tmp->defined == 1 )
              return( tmp );
      tmp = tmp->next;
  }
  return( tmp );
}
 
/* lookup_undefined - finds undefined symbol, returns pointer to it,
 *                returns NULL if symbol not found
 */
struct symbol *lookup_undefined(void)
{
  struct symbol *tmp = shead;
  
  while( tmp )
  {
      if( !tmp->defined )
          return( tmp );
      tmp = tmp->next;
  }
  return( tmp );
}

/* resolve_undefined - looks for undefined references to labels
 *                     and resolves them
 * Return:
 *         0 - all references resolved
 *         1 - some references are not resolved
 */
int resolve_undefined( void )
{
    struct symbol *unresolved, *definition;
    int            cant_resolve = 0;
    unsigned int   resolved, where;
    
    while( unresolved = lookup_undefined() )
    {
        definition = lookupsymbol( unresolved->name );
        
        if( definition )
        { /* Found, resolve reference */
            resolved = definition->offset;
            where    = unresolved->offset;
            fwbuffer[ where ] = resolved & 0xFF; where++; resolved >>= 8;
            fwbuffer[ where ] = resolved & 0xFF; where++; resolved >>= 8;
            fwbuffer[ where ] = resolved & 0xFF; where++; resolved >>= 8;
            fwbuffer[ where ] = resolved & 0xFF; 
            
            /* mark this entry as resolved */
            unresolved->defined = 2;
        }
        else
        { /* no definition found, complain and leave unresolved */
            printf("Reference to undefined label %s at line %d offset in the image 0x%x\n",
                    unresolved->name, unresolved->lineno, unresolved->offset);
            /* Mark as left unresolved */
            unresolved->defined = 3;
            /* flag error */
            cant_resolve = 1;
        }
    }
    return( cant_resolve );
}
 
/* usage - um, no comment... */
void usage( char *progname )
{
  printf("Usage:\n\t%s <ctrl_file_name> [output_file_name]\n\n", basename( progname ));
  printf("If output file name is omitted it will be set as <ctrl_file_name>.img\n");
  exit(1);
}

/* trim - remove whitespace at the beginning 
 * and end of the string, removes '#' comments too
 * 
 * returns number of characters left in string
 */
int trim( char *s )
{
  char *cp = s;
  char *cd = s;
  
  while( cp && isspace( *cp ) ) cp++;
  if( cp > s )
      while( *cd++ = *cp++ ) ;
  
  cp = s;
  while( cp && *cp )
  {
      if( *cp == '#' )
      {
          *cp = '\0';
          break;
      }
      cp++;
  }
  
  if( cp > s ) --cp;
  while( (cp >= s) && isspace( *cp ) ) *cp-- = '\0';
  
  return( cp - s );
}

int main( int argc, char *argv[] )
{

  struct stat   statbuf;
  int           ret, fileno;
  FILE         *fileni;
  char         *fptr, *cp, *cpe;
  struct tm    *tmp;
  time_t        curtime;
  optable      *optptr;
  unsigned int  crc;
  
  shead = NULL;
  
  /* Check invocation */
  if( argc <= 0 )
  {
      usage( argv[0] );
  }
  
  /* Check input file */
  ret = stat( argv[1], &statbuf );
  if( ret )
  {
      perror("Cannot access script file ");
      exit(1);
  }
  if( !statbuf.st_size )
  {
      printf("%s - empty script file, cannot make image\n", basename( argv[0] ));
      exit(1);
  }
  
  strcpy( scriptline, argv[1] );
  /*  snprintf( scriptline, 2047, "gcc -E %s", argv[1] ); */
  /*  For now let's make gcc -E by external script and    */
  /*  leave intermediate file in place for debugging      */
  /*  Later, uncomment snprintf line, replace fopen with  */
  /*  popen et voila! :)                                  */
  fileni = fopen( scriptline, "r" );
  if( !fileni )
  {
      snprintf( scriptline, 2047, "Cannot process input %s ", argv[1]);
      perror( scriptline );
      exit(1);
  }
  
  /* Take care of the output file */
  if( (argc > 2) && *argv[2] )
      fptr = argv[2];
  else
  {
      strcpy( fname, argv[1] );
      cpe = fname;
      while( cpe && *cpe ) cpe++;
      cp = cpe;
      while( (cp > fname) && (*cp != '.') ) cp--;
      if( cp > fname )
          strcat( cp, ".img" );
      else
          strcat( cpe, ".img" );
      fptr = fname;
  }
  fileno = open( fptr, O_WRONLY | O_CREAT | O_TRUNC );
  if( fileno < 0 )
  {
      perror( "Cannot create output file " );
      exit(1);
  }
  
  /* descriptive part of the image */
  offset = 0;
  fwbuffer[ offset ] = 0xFF;       inc_offset();
  
  fwbuffer[ offset ] = VERSION;    inc_offset();
  
  cp = getenv("HOSTNAME");
  
  if( !cp )
      cp = "localhost";
  
  while( cp && *cp )
  {
      fwbuffer[ inc_offset() ] = *cp++;
  }
  fwbuffer[ offset ] = '\0';      inc_offset();
  
  curtime = time(NULL);
  tmp = gmtime(&curtime);
  
  fwbuffer[ offset ] = tmp->tm_mon + 1;    inc_offset();
  fwbuffer[ offset ] = tmp->tm_mday;       inc_offset();
  /* Year is returned as # after 1900, fix it */
  fwbuffer[ offset ] = (tmp->tm_year - 108) > 0 ?
                        tmp->tm_year - 108:0; inc_offset();
  fwbuffer[ offset ] = tmp->tm_hour;       inc_offset();
  fwbuffer[ offset ] = tmp->tm_min;        inc_offset();
  
  /* Now it's time to read the script and convert it to image */
  lineno = 0;
  while( fgets( scriptline, 2047, fileni) )
  {
      lineno++;
//      printf("lineno = %d\n", lineno);
      scriptline[2047] = '\0'; /* enforce line ending */
      
      ret = trim( scriptline );
      
      /* First, empty lines, comments and pseudoops */
      if( ! ret )
          continue;
          
      if( scriptline[0] == '#' )
          continue;
          
      if( scriptline[0] == '!' )
      {
          if( ! addsymbol( &scriptline[1] ) )
          {
              printf("Cannot add symbol %s to symbol table at line: %d\n",
                      &scriptline[1], lineno );
              exit(1);
          }
          continue;
      }
      
      /* Now real ops */
      optptr = scriptops;
      while( optptr->mnemonic )
      {
          if( !strncasecmp( scriptline, optptr->mnemonic, strlen(optptr->mnemonic) ) )
              break;
          optptr++;
      }
      
      if( !optptr->mnemonic )
      {
          printf("Syntax error, unknown opcode at line %d\n", lineno );
          exit(1);
      }
      
      fwbuffer[ offset ] = optptr->opcode;
      
      if( optptr->getargs )
      {
          ret = (*(optptr->getargs))();
          if( ret )
          {
              printf(" Error at line %d\n", lineno);
          }
      }
      else
          inc_offset();
  }
  /* resolve possible forward references left */
  if( resolve_undefined() )
  {
      printf("There are undefined label references left.\n");
  }
  
  /* Image done, attach crc32 */
  init_crc32();
  crc = crc32( ~0, fwbuffer, offset );
  
  crc = ~crc;
  
  fwbuffer[ offset ] = crc & 0xFF; inc_offset(); crc >>= 8;
  fwbuffer[ offset ] = crc & 0xFF; inc_offset(); crc >>= 8;
  fwbuffer[ offset ] = crc & 0xFF; inc_offset(); crc >>= 8;
  fwbuffer[ offset ] = crc & 0xFF; inc_offset(); 
  
  /* write the whole thing to the image file */
  ret = write( fileno, fwbuffer, offset );
  
  if( ret != offset )
  {
      perror("Error writing image file\n");
      exit(1);
  }
  fclose(fileni);
  close( fileno );
  exit(0);
}
