/*
Bdiff is a diff/patch utility for binary files.
Copyright (C) 2003 Giuliano Pochini <pochini@shiny.it>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


// Search debug messages
//#define DEBUG_SE

// Found blocks debug messages
//#define DEBUG_FO

// Uncomment this to workaround the lack of mmap()/munmap() funzions, so
// you can compile on stupid OS's which don't support them.
//#define NOMMAP


#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "md5.h"

// File header
#define BDIFF_MAGIC "BDIFF"
#define BDIFF_VERSION 'a'

// Block header (uint_8)
#define BLOCK_DUP (0<<6)
#define BLOCK_OTHER (1<<6)
#define BLOCK_COPY (2<<6)
#define BLOCK_UNUSED (3<<6)
#define BLOCK_MODEMASK 0xC0
#define BLOCK_VALUEMASK 0x3F

// BLOCK_OTHER sub-types
#define BLOCK_OLDFMD5 0
#define BLOCK_NEWFMD5 1
#define BLOCK_PERM 2

#define BLOCK_SUBT 5

#define FL_OLDFMD5 (1<<0)
#define FL_NEWFMD5 (1<<1)
#define FL_QUIET   (1<<2)
#define FL_PERM    (1<<3)

#define MIN_MINBSIZE BLOCK_SUBT+1
#define MIN_MAXBSIZE 256
#define MAX_MAXBSIZE (8<<20)    // 8MiB Should be enough.

size_t MaxBlockSize=(1<<20);    // The maximum size of the block to look for
size_t MaxSearchSize=0;         // Must be >= MaxBlockSize*2  It's the maximum size of the region where the block is searched in
size_t SkipSize=1;              // Advance SkipSize bytes when it can't find a matching block


void write32(char *buf, uint32_t n) {

  buf[0]=(n&0xff000000)>>24;
  buf[1]=(n&0x00ff0000)>>16;
  buf[2]=(n&0x0000ff00)>>8;
  buf[3]=n&0x000000ff;
}



uint32_t read32(char *buf) {

  return((((uint32_t)buf[0])<<24) + (((uint32_t)buf[1])<<16) + (((uint32_t)buf[2])<<8) + (uint32_t)buf[3]);
}

// This function finds the start of the longest occurrence of memory area needle in the memory area haystack of length haystacklen
const void *Search(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen, size_t *max) {
  const unsigned char *start, *s, *n, *maxp;
  int i;

  maxp=0;
  *max=0;
  if (needlelen<MIN_MINBSIZE)
    return(0);
  for (start=haystack; start<=(unsigned char*)haystack+haystacklen-needlelen; start++) {
    if (*(int32_t *)start!=*(int32_t*)needle)
      continue;
    i=needlelen-3;
    s=start+4;
    n=needle+4;
    while (--i && *s==*n) {
      s++;
      n++;
    }
    if (*max<s-start) {
      *max=s-start;
      maxp=start;
    }
    if (s-start==needlelen)
      break;
  }
  return(maxp);
}



#ifdef NOMMAP
// There functions emulate mmap() and munmap() by loading the whole file into memory

void  *mmap(void *start, size_t length, int prot ,int flags, int fd, off_t offset) {
  void *buf;

  if (buf=malloc(length)) {
    lseek(fd, offset, SEEK_SET);
    if (read(fd, buf, length)<0) {
      free(buf);
      buf=0;
    }
  }
  if (!buf)
    return((void *)-1);
  else
    return(buf);
}



int munmap(void *start, size_t length) {

  free(start);
  return(0);
}
#endif



// This function is a quicker version of Search() when the length of the memory area needle equals the minimum block size (6 bytes)
__inline const void *Search2(const void *haystack, size_t haystacklen, const void *needle) {
  int32_t a,b;
  int i;

  a=*(int32_t*)needle;
  b=*(int16_t*)(needle+4);
  i=haystacklen-MIN_MINBSIZE+1;
  do {
    if (*(int32_t *)haystack==a && *(int16_t *)(haystack+4)==b)
      return(haystack);
    haystack++;
  } while (--i);
  return(0);
}

char *default_oname = "difffile";
struct stat st;

int Diff(const char *oldfile, const char *newfile, const char *outfile, int Flags) {
  size_t osize, nsize;
  char *of, *nf;
  const char *fnd;
  int ofile, nfile, dfile;
  int i, ret, cbytes, dbytes;
  off_t oPos, nPos, Found, Start, End, nScanOffs;
  size_t BlockSize, SearchSize, bs;
  unsigned char buf[20];         // at least 16 (for md5) + 1 (header) bytes

  if (!outfile)  outfile=default_oname;

  // Get file sizes
  if (ret=stat(oldfile, &st)) {
    printf("Stat file %s: %s\n", oldfile, strerror(errno));
    return(1);
  }
  osize=st.st_size;

  // Important: stat the new file after the old one
  if (ret=stat(newfile, &st)) {
    printf("Stat file %s: %s\n", newfile, strerror(errno));
    return(1);
  }
  nsize=st.st_size;

  // Open files
  if ((ofile=open(oldfile, O_RDONLY))<0) {
    printf("Open file %s: %s\n", oldfile, strerror(errno));
    return(1);
  }
  if ((nfile=open(newfile, O_RDONLY))<0) {
    printf("Open file %s: %s\n", newfile, strerror(errno));
    close(ofile);
    return(1);
  }
  if ((dfile=open(outfile, O_CREAT|O_TRUNC|O_WRONLY, 0664))<0) {
    printf("Open file %s: %s\n", "difffile", strerror(errno));
    close(ofile);
    close(nfile);
    return(1);
  }

  // Mmap files
  if ((of=mmap(0, osize, PROT_READ, MAP_PRIVATE, ofile, 0))<0) {
    printf("Mmap file %s: %s\n", oldfile, strerror(errno));
    close(dfile);
    close(nfile);
    close(ofile);
    return(1);
  }
  if ((nf=mmap(0, nsize, PROT_READ, MAP_PRIVATE, nfile, 0))<0) {
    printf("Mmap file %s: %s\n", newfile, strerror(errno));
    close(dfile);
    close(nfile);
    close(ofile);
    return(1);
  }

  // Write main header
  strcpy(buf, BDIFF_MAGIC);
  buf[5]=BDIFF_VERSION;
  write32(&buf[6], MaxBlockSize);
  write(dfile, buf, 10);

  // Prepend md5sum of oldfile
  if (Flags & FL_OLDFMD5) {
    if (~Flags & FL_QUIET) {
      printf("md5sum of %s: ", oldfile);
      fflush(stdout);
    }
    md5_buffer(of, osize, buf+1);
    if (~Flags & FL_QUIET) {
      for (i=0; i<16; i++)
        printf("%02x", buf[i+1]);
      printf("\n");
    }
    buf[0]=BLOCK_OTHER|BLOCK_OLDFMD5;
    write(dfile, buf, 17);
  }

  // Beginning of the file
  oPos=nPos=0;
  cbytes=dbytes=0;

  while (nPos<nsize) {
    BlockSize=MaxBlockSize;
    if (BlockSize+nPos>nsize)  BlockSize=nsize-nPos;
    SearchSize=MaxSearchSize;
    Start=oPos+(BlockSize>>1)-(SearchSize>>1);
    if (Start<0)  Start=0;
    End=oPos+(BlockSize>>1)+((SearchSize+1)>>1);
    if (End>osize)  End=osize;
    SearchSize=End-Start;
#ifdef DEBUG_SE
    printf("Search from %9d (size %7d) for %9d (length %d)\n", Start, SearchSize, nPos, BlockSize);
#endif

    fnd=(char *)Search(of+Start, SearchSize, nf+nPos, BlockSize, &bs);
    BlockSize=MIN_MINBSIZE;
    if (fnd) {
      if (bs>=MIN_MINBSIZE)
        BlockSize=bs;
      else
        fnd=0;
    }

    if (fnd) {
      Found=fnd-of;
      write32(&buf[1], Found);
      if (BlockSize>BLOCK_SUBT && BlockSize-BLOCK_SUBT<=BLOCK_VALUEMASK) {
        buf[0]=BLOCK_DUP | (BlockSize-BLOCK_SUBT);
        write(dfile, buf, 5);
      } else {
        buf[0]=BLOCK_DUP;
        write32(&buf[5], BlockSize);
        write(dfile, buf, 9);
      }
#ifdef DEBUG_FO
      printf("Block found oldfile offset: %9d  newfile offset: %9d  (length %d)\n", (int)Found, (int)nPos, (int)BlockSize);
#endif
      oPos+=BlockSize;
      nPos+=BlockSize;
      cbytes+=BlockSize;
    } else {  // not found
      nScanOffs=BlockSize-SkipSize;
      while (!fnd && nScanOffs<MaxBlockSize) {
        nScanOffs+=SkipSize;
        if (nPos+nScanOffs+BlockSize>nsize) {      // EOF ?
          nScanOffs=nsize-nPos;
          break;
        } else
          fnd=Search2(of+Start, SearchSize, nf+nPos+nScanOffs);
      }
      BlockSize=nScanOffs;

      // Now I have the smallest non-redundant block. I store it as-is.
      if (BlockSize>BLOCK_SUBT && BlockSize-BLOCK_SUBT<=BLOCK_VALUEMASK) {
        buf[0]=BLOCK_COPY | (BlockSize-BLOCK_SUBT);
        write(dfile, buf, 1);
      } else {
        buf[0]=BLOCK_COPY;
        write32(&buf[1], BlockSize);
        write(dfile, buf, 5);
      }
      write(dfile, nf+nPos, BlockSize);
#ifdef DEBUG_FO
      printf("Not found                              newfile offset: %9d  (length %d)\n", (int)nPos, (int)BlockSize);
#endif
      nPos+=BlockSize;
      oPos=(long long)nPos*(long long)osize/nsize;
      dbytes+=BlockSize;
    }

    // Update progress meter
    if (~Flags & FL_QUIET) {
      putchar('|');
      for (i=(long long)40*nPos/nsize; i>=0; i--)
        putchar('-');
      putchar('>');
      for (i=40-(long long)40*nPos/nsize; i>0; i--)
        putchar(' ');
      printf("| %d/%d (%.2f%%)\r", (int)nPos, nsize, 100.0*nPos/nsize);
      fflush(stdout);
    }
  }
  if (~Flags & FL_QUIET)
    putchar('\n');

  // Append md5sum of newfile
  if (Flags & FL_NEWFMD5) {
    if (~Flags & FL_QUIET) {
      printf("md5sum of %s: ", newfile);
      fflush(stdout);
    }
    md5_buffer(nf, nsize, buf+1);
    if (~Flags & FL_QUIET) {
      for (i=0; i<16; i++)
        printf("%02x", buf[i+1]);
      printf("\n");
    }
    buf[0]=BLOCK_OTHER|BLOCK_NEWFMD5;
    write(dfile, buf, 17);
  }

  // Save file permission (** Some systems might have these values larger than 32bits)
  if (Flags & FL_PERM) {
    buf[0]=BLOCK_OTHER|BLOCK_PERM;
    buf[1]=12;
    write32(&buf[2], st.st_uid);
    write32(&buf[6], st.st_gid);
    write32(&buf[10], st.st_mode);
    write(dfile, buf, 14);
  }

  if (munmap(of, osize)) {
    printf("munmap file %s: %s\n", oldfile, strerror(errno));
  }
  if (munmap(nf, nsize)) {
    printf("munmap file %s: %s\n", newfile, strerror(errno));
  }


  /*
   * Append new file size.  This makes it easy for mkdsetimg
   * to include the post-patch size without having to obtain
   * the original data and apply the patch.
   */
  i = nsize & 0xFF;
  write(dfile, &i, 1);
  i = (nsize >>8) & 0xFF;
  write(dfile, &i, 1);

  close(dfile);
  close(nfile);
  close(ofile);
  if (~Flags & FL_QUIET)
    printf("Common bytes: %d    stored bytes: %d\n", cbytes, dbytes);
  return(0);
}



int bdread(int fd, void* buffer, size_t length) {
  ssize_t s;

  s=read(fd, buffer, length);
  if (s==length)
    return(0);

  if (s<0)
    printf("Read error: %s\n", strerror(errno));
  else
    printf("End of file reached. File corrupted ?\n");
  return(-1);
}


#define Read(f, b, l) do {if (bdread(f, b, l))  goto freeandexitpatch;} while(0)
#define Write(f, b, l) if (write(f, b, l)<l) {printf("Write error: %s\n", strerror(errno)); goto freeandexitpatch;}

int Patch(const char *oldfile, const char *difffile, const char *outfile, int Flags) {
  int ofile, nfile, dfile, ret;
  off_t oPos, Pos;
  size_t BlockSize;
  unsigned char *buf, shortbuf[16], md5sum[16];
  FILE *f;

  if (!outfile)  outfile="newfile";

  // Open files
  if ((ofile=open(oldfile, O_RDONLY))<0) {
    printf("Open file %s: %s\n", oldfile, strerror(errno));
    return(1);
  }
  if ((dfile=open(difffile, O_RDONLY))<0) {
    printf("Open file %s: %s\n", difffile, strerror(errno));
    close(ofile);
    return(1);
  }
  if ((nfile=open(outfile, O_CREAT|O_TRUNC|O_RDWR, 0664))<0) {
    printf("Open file %s: %s\n", "newfile", strerror(errno));
    close(ofile);
    close(dfile);
    return(1);
  }
  buf=0;
  ret=0;

  // Check magic and version
  Read(dfile, shortbuf, 10);
  if (strncmp(shortbuf, BDIFF_MAGIC, strlen(BDIFF_MAGIC))) {
    printf("%s is not a bdiff file\n", difffile);
    goto freeandexitpatch;
  }
  if (shortbuf[5]>BDIFF_VERSION) {
    printf("Diff file version %d is not supported. Please upgrade\n", shortbuf[5]-BDIFF_VERSION+1);
    goto freeandexitpatch;
  }

  MaxBlockSize=read32(shortbuf+6);
  if (MaxBlockSize<MIN_MAXBSIZE) {
    printf("MaxBlockSize too small. Corrupted file ?\n");
    goto freeandexitpatch;
  }
  if (MaxBlockSize>MAX_MAXBSIZE) {
    printf("MaxBlockSize too large. Corrupted file ?\n");
    goto freeandexitpatch;
  }
  if (!(buf=(char *)malloc(MaxBlockSize))) {
    printf("Cannot allocate memory\n");
    goto freeandexitpatch;
  }

  while (read(dfile, buf, 1)>0) {
    if ((buf[0] & BLOCK_MODEMASK)==BLOCK_DUP) {
      // Duplicate a block from oldfile
      if (buf[0] & BLOCK_VALUEMASK) {           // Short packet ?
        BlockSize=(buf[0] & BLOCK_VALUEMASK)+BLOCK_SUBT;
        Read(dfile, buf, 4);
      } else {
        Read(dfile, buf, 8);
        BlockSize=read32(&buf[4]);
        if (BlockSize>MaxBlockSize) {
          printf("Block too large. File corrupted ?\n");
          goto freeandexitpatch;
        }
      }
      oPos=read32(buf);
      lseek(ofile, oPos, SEEK_SET);
      Read(ofile, buf, BlockSize);
      Write(nfile, buf, BlockSize);
    } else if ((buf[0] & BLOCK_MODEMASK)==BLOCK_COPY) {
      // Read block from difffile
      if (buf[0] & BLOCK_VALUEMASK) {           // Short packet ?
        BlockSize=(buf[0] & BLOCK_VALUEMASK)+BLOCK_SUBT;
      } else {
        Read(dfile, buf, 4);
        BlockSize=read32(buf);
        if (BlockSize>MaxBlockSize) {
          printf("Block too large. File corrupted ?\n");
          goto freeandexitpatch;
        }
      }
      Read(dfile, buf, BlockSize);
      Write(nfile, buf, BlockSize);
    } else if ((buf[0] & BLOCK_MODEMASK)==BLOCK_OTHER) {
      if ((buf[0] & ~BLOCK_MODEMASK)==BLOCK_OLDFMD5) {
        // Oldfile md5sum
        Read(dfile, buf, 16);
        if (~Flags & FL_QUIET) {
          printf("Checking md5sum of %s: ", oldfile);
          fflush(stdout);
        }
        Pos=lseek(dfile, 0, SEEK_CUR);
        f=fdopen(dup(ofile), "r");
        fseek(f, 0, SEEK_SET);
        md5_stream(f, md5sum);
        fclose(f);
        if (memcmp(md5sum, buf, 16)) {
          printf("Wrong source file. Exitting...\n");
          return(1);
        }
        if (~Flags & FL_QUIET)
          printf("OK\n");
        lseek(dfile, Pos, SEEK_SET);
      } else if ((buf[0] & ~BLOCK_MODEMASK)==BLOCK_NEWFMD5) {
        // Newfile md5sum
        Read(dfile, buf, 16);
        if (~Flags & FL_QUIET) {
          printf("Checking md5sum of %s: ", outfile);
          fflush(stdout);
        }
        f=fdopen(dup(nfile), "r");
        fseek(f, 0, SEEK_SET);
        md5_stream(f, md5sum);
        fclose(f);
        if (memcmp(md5sum, buf, 16)) {
          printf("Noooo! The file is corrupted, sorry.\n");
          ret=1;
        } else {
          if (~Flags & FL_QUIET)
            printf("OK\n");
        }
        lseek(nfile, 0, SEEK_END);
      } else if ((buf[0] & ~BLOCK_MODEMASK)==BLOCK_PERM) {
        Read(dfile, buf, 13);
        if (Flags & FL_PERM) {
          fchown(nfile, read32(buf+1), read32(buf+5));  // Ignore errors
          fchmod(nfile, read32(buf+9));
        }
      } else {
        if (~Flags & FL_QUIET)
          printf("Unknown chunk at offset: %d\n", (int)lseek(dfile, 0, SEEK_CUR)-1);
        Read(dfile, buf, 1);        // Get its length
        Read(dfile, buf, buf[0]);   // Skip it
      }
    } else { // BLOCK_OTHER
      printf("Error in the diff file at offset: %d\n", (int)lseek(dfile, 0, SEEK_CUR)-1);
      ret=1;
      goto freeandexitpatch;
    }
  }
  if (~Flags & FL_QUIET)
    printf("Patch applied successfully.\n");
freeandexitpatch:
  free(buf);
  close(nfile);
  close(ofile);
  close(dfile);
  return(ret);
}




int main(int argc, char *argv[]) {
  char *outf;
  int parm, ret;
  int Flags;
  int optimize = 0;
  size_t omaxb = MIN_MAXBSIZE;
  size_t oosize = SIZE_MAX;
  char *usage="BDiff v1.0.4\n" \
              "Usage: %s <options> [outfile]\n" \
              "Options:\n" \
              "  -d oldfile newfile     // Creates a diff file between given files.\n" \
              "  -p oldfile difffile    // Patches oldfile with difffile\n" \
              "  -perm                  // Saves/restores file permissions\n" \
              "  -q                     // Suppress useless info messages\n" \
              "  -h                     // Displays this help message\n" \
              "-d related options, must preceed -d itself:\n" \
              "  -O                     // Find optimal maxb w/respect to patch size\n" \
              "  -maxb <bytes>          // Sets maximum block size (default 1MiB)\n" \
              "  -maxs <bytes>          // Sets the size of the block where the\n" \
              "                            data is searched in (default is 3*maxb\n" \
              "                            and it should be >= 2*maxb)\n" \
              "  -adv <bytes>           // Skips adv bytes when is cannot find a\n" \
              "                            matching block (default 1)\n" \
              "  -oldmd5                // Create md5 checksum for oldfile\n" \
              "  -newmd5                // Create md5 checksum for newfile\n" \
              "  -nooldmd5              // Don't create md5 checksum for oldfile (default)\n" \
              "  -nonewmd5              // Don't create md5 checksum for newfile (default)\n";

/*
  {
  int i, i1, i2, c;
  char h[64], n[8];

  srand(100);
  for (c=10000; c; c--) {
    for (i=0; i<60; i++)  h[i]='a'+(rand()&3);
    h[60]='a';h[61]=0;
    for (i=0; i<4; i++)  n[i]='a'+(rand()&3);
    n[4]='a';n[5]=0;
//    if (Search(h, 60, n, 4) != memmem(h, 60, n, 4))
//      printf("h=%s n=%s      (S=%d m=%d\n",h,n,i1,i2);

    for (i=100; i; i--) Search(h, 60, n, 4, &i2);
//    for (i=100; i; i--) memmem(h, 60, n, 4);
  }
  printf("tc=%d\n",totcicli);
  }

  Search2("sbababbbababbuq", 17, "babbu", 5);
  Search("abcabcabcabdabc", 15, "abcabcabd", 9, &Flags);
  exit(0);
*/

//  Flags=FL_OLDFMD5 | FL_NEWFMD5;
  Flags = 0;
  ret=0;
  outf=0;
  parm=1;
  while (parm<argc) {
    if (!strcmp(argv[parm], "-p")) {
      if (parm+4==argc && argv[parm+3][0]!='-')
        outf=argv[parm+3];
      if (parm+2>=argc)
        printf("Missing -p parameter\n");
      else
        ret=Patch(argv[parm+1], argv[parm+2], outf, Flags);
      exit(ret);
    } else if (!strcmp(argv[parm], "-d")) {
      if (MaxBlockSize<MIN_MAXBSIZE) {
        printf("-maxb must be >= %d. Fixed.\n", MIN_MAXBSIZE);
        MaxBlockSize=MIN_MAXBSIZE;
      }
      if (MaxBlockSize>MAX_MAXBSIZE) {
        printf("-maxb must be <= %d. Fixed.\n", MAX_MAXBSIZE);
        MaxBlockSize=MAX_MAXBSIZE;
      }
      if (!MaxSearchSize)
        MaxSearchSize=MaxBlockSize*3;
      if (MaxSearchSize<MaxBlockSize) {
        printf("-maxs must be >= maxb. Fixed.\n");
        MaxSearchSize=MaxBlockSize;
      }

      if (parm+4==argc && argv[parm+3][0]!='-')
        outf=argv[parm+3];
      if (parm+2>=argc) {
        printf("Missing -d parameter\n");
      } else {
        if( optimize )
        {
          for( MaxBlockSize = MIN_MAXBSIZE; MaxBlockSize <= MAX_MAXBSIZE; MaxBlockSize <<= 1 )
          {
            ret=Diff(argv[parm+1], argv[parm+2], 0, Flags);
            
            if (ret=stat(default_oname, &st)) 
              continue;
            
            if (!ret && (~Flags & FL_QUIET))
              printf("Resulting diff size is: %ld\n", st.st_size);
              
            if( st.st_size < oosize )
            {
              oosize = st.st_size;
              omaxb  = MaxBlockSize;
            }
            unlink(default_oname);
          }
          MaxBlockSize = omaxb;
        }
        ret=Diff(argv[parm+1], argv[parm+2], outf, Flags);
        if (!ret && (~Flags & FL_QUIET))
        { 
          if( optimize )
            printf("Optimal block size was: %ld\n", MaxBlockSize );
          printf("Diff'ed\n");
        }
        exit(ret);
      }
      exit(0);
    } else if (!strcmp(argv[parm], "-maxb")) {
      MaxBlockSize=atoi(argv[++parm]);
    } else if (!strcmp(argv[parm], "-maxs")) {
      MaxSearchSize=atoi(argv[++parm]);
    } else if (!strcmp(argv[parm], "-adv")) {
      SkipSize=atoi(argv[++parm]);
      if (SkipSize>6)  SkipSize=6;
    } else if (!strcmp(argv[parm], "-nooldmd5")) {
      Flags&=~FL_OLDFMD5;
    } else if (!strcmp(argv[parm], "-nonewmd5")) {
      Flags&=~FL_NEWFMD5;
    } else if (!strcmp(argv[parm], "-oldmd5")) {
      Flags|=FL_OLDFMD5;
    } else if (!strcmp(argv[parm], "-newmd5")) {
      Flags|=FL_NEWFMD5;
    } else if (!strcmp(argv[parm], "-perm")) {
      Flags|=FL_PERM;
    } else if (!strcmp(argv[parm], "-q")) {
      Flags|=FL_QUIET;
    } else if (!strcmp(argv[parm], "-O")) {
      optimize++;
    } else if (!strcmp(argv[parm], "-h") || !strcmp(argv[parm], "--help")) {
      printf(usage, argv[0]);
      exit(0);
    } else {
      printf("Wrong parameter: \"%s\"\n", argv[parm]);
    }
    parm++;
  }
  printf("Nothing done. Try -h for parameter list.\n");
  return(0);
}

