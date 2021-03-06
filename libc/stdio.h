/*******************************************************************************
*                                                                              *
*                            STANDARD I/O HEADER                               *
*                                                                              *
*                          COPYRIGHT 2007 (C) S. A. MOORE                      *
*                                                                              *
* FILE NAME: stdio.h                                                           *
*                                                                              *
* DESCRIPTION:                                                                 *
*                                                                              *
* Defines the serial I/O platform for whitebook C.                             *
*                                                                              *
* BUGS/ISSUES:                                                                 *
*                                                                              *
*******************************************************************************/

#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>
#include <stdarg.h>

/*
 * This block of defines will "coin" all of the plain labeled stdio calls and
 * make them pa_ coined names. This allows the stio calls to exist in parallel
 * with the built-in stio package.
 */

#ifdef PA_COIN_NAMES

#define fopen       pa_fopen
#define freopen     pa_freopen
#define fdopen      pa_fdopen
#define fflush      pa_fflush
#define fclose      pa_fclose
#define remove      pa_remove
#define rename      pa_rename
#define tmpfile     pa_tmpfile
#define tmpnam      pa_tmpnam
#define setvbuf     pa_setvbuf
#define setbuf      pa_setbuf
#define fprintf     pa_fprintf
#define printf      pa_printf
#define sprintf     pa_sprintf
#define vprintf     pa_vprintf
#define vfprintf    pa_vfprintf
#define vsprintf    pa_vsprintf
#define scanf       pa_scanf
#define scanf       pa_scanf
#define sscanf      pa_sscanf
#define fgetc       pa_fgetc
#define getc        pa_getc
#define fgets       pa_fgets
#define fputc       pa_fputc
#define fputs       pa_fputs
#define putc        pa_putc
#define getchar     pa_getchar
#define gets        pa_gets
#define putc        pa_putc
#define putchar     pa_putchar
#define puts        pa_puts
#define ungetc      pa_ungetc
#define fread       pa_fread
#define fwrite      pa_fwrite
#define fseek       pa_fseek
#define ftell       pa_ftell
#define rewind      pa_rewind
#define fgetpos     pa_fgetpos
#define fsetpos     pa_fsetpos
#define clearerr    pa_clearerr
#define feof        pa_feof
#define ferror      pa_ferror
#define perror      pa_perror
#define fileno      pa_fileno

#endif

/* enable this next define for testing mode */

#define L_tmpnam 9
#define L_TMP_MAX 100
#define FOPEN_MAX 100

#define EOF (-1)

/* buffering modes */

#define _IOFBF 1 /* full buffering */
#define _IOLBF 2 /* line buffering */
#define _IONBF 3 /* no buffering */

#define BUFSIZ 512 /* standard buffer size */

/* this switch allows getc() and putc() to be macros. This is the way LIBC does
   it, and the ability is included here for possible compatability issues */
#define USEMACRO 1 /* use macros for getc and putc */

typedef long fpos_t;

/* standard file descriptor */

typedef struct {

    int  fid;    /* file logical id, <0 means unused */
    char *name;  /* name holder for error/diagnostics */
    int  text;   /* text/binary mode flag */
    int  mode;   /* r/w mode, 0=read, 1=write, 2=read/write */
    int  append; /* append mode */
    int  pback;  /* pushback character, only a single is implemented */
    int  flags;  /* state flags, 0=EOF */

} FILE;

/* error/status flags */

#define _EFEOF 0x0001 /* stream EOF */

/* standard in, out and error files */

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE *fopen(const char* filename, const char* mode);
FILE *freopen(const char* filename, const char* mode, FILE* stream);
FILE *fdopen(int fd, const char *mode);
int fflush(FILE* stream);
int fclose(FILE* stream);
int remove(const char *filename);
int rename(const char *oldname, const char *newname);
FILE *tmpfile(void);
char *tmpnam(char s[]);
int setvbuf(FILE* stream, char *buf, int mode, size_t size);
void setbuf(FILE* stream, char *buf);
int fprintf(FILE* stream, const char *format, ...);
int printf(const char* format, ...);
int sprintf(char* s, const char *format, ...);
int vprintf(const char* format, va_list arg);
int vfprintf(FILE* stream, const char *format, va_list arg);
int vsprintf(char* s, const char *format, va_list arg);
int fscanf(FILE* stream, const char *format, ...);
int scanf(const char* format, ...);
int sscanf(const char* s, const char *format, ...);
int fgetc(FILE *stream);

#ifdef USEMACRO
#ifdef PA_COIN_NAMES
#define pa_getc(fp) (pa_fgetc(fp))
#else
#define getc(fp) (fgetc(fp))
#endif
#else
int getc(FILE *stream);
#endif

char *fgets(char *s, int n, FILE *stream);
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);

#ifdef USEMACRO
#ifdef PA_COIN_NAMES
#define pa_putc(c, fp) pa_fputc(c, fp)
#else
#define putc(c, fp) fputc(c, fp)
#endif
#else
int putc(int c, FILE *stream);
#endif

int getchar(void);
char *gets(char *s);
int putc(int c, FILE *stream);
int putchar(int c);
int puts(const char *s);
int ungetc(int c, FILE *stream);
size_t fread(void *ptr, size_t size, size_t nobj, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nobj, FILE *stream);
int fseek(FILE* stream, long offset, int origin);
long ftell(FILE* stream);
void rewind(FILE* stream);
int fgetpos(FILE* stream, fpos_t *ptr);
int fsetpos(FILE* stream, const fpos_t *ptr);
void clearerr(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void perror(const char *s);
int fileno(FILE* stream);

#endif /* stdio.h */
