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
* 1. The appearance of functions as macros, such as putchar and getchar,       *
* should be an option for users that have issues with their being hard         *
* functions.                                                                   *
*                                                                              *
*******************************************************************************/

#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>
#include <stdarg.h>

/* enable this next define for testing mode */

#define L_tmpnam 100
#define L_TMP_MAX 100
#define FOPEN_MAX 100

#define EOF (-1)

/* buffering modes */

#define _IOFBF 1 /* full buffering */
#define _IOLBF 2 /* line buffering */
#define _IONBF 3 /* no buffering */

#define BUFSIZ 512 /* standard buffer size */

/* seek modes */

#define SEEK_SET 1 /* seek relative to start */
#define SEEK_CUR 2 /* seek relative to current position */
#define SEEK_END 3 /* seek relative to end */

typedef long fpos_t;

/* standard file descriptor */

typedef struct {

    int  fid;    /* file logical id, <0 means unused */
    char *name;  /* name holder for error/diagnostics */
    int  text;   /* text/binary mode flag */
    int  mode;   /* r/w mode, 0=read, 1=write, 2=read/write */
    int  append; /* append mode */
    int  pback;  /* pushback character, only a single is implemented */

} FILE;

/* standard in, out and error files */

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

FILE *fopen(const char *filename, const char *mode);
FILE *freopen(const char *filename, const char *mode, FILE *stream);
int fflush(FILE *stream);
int fclose(FILE *stream);
int remove(const char *filename);
int rename(const char *oldname, const char *newname);
FILE *tmpfile(void);
char *tmpnam(char s[]);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
void setbuf(FILE *stream, char *buf);
int fprintf(FILE *stream, const char *format, ...);
int printf(const char *format, ...);
int sprintf(char *s, const char *format, ...);
int vprintf(const char *format, va_list arg);
int vfprintf(FILE *stream, const char *format, va_list arg);
int vsprintf(char *s, const char *format, va_list arg);
int fscanf(FILE *stream, const char *format, ...);
int scanf(const char *format, ...);
int sscanf(char *s, const char *format, ...);
int fgetc(FILE *stream);
char *fgets(char *s, int n, FILE *stream);
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int getc(FILE *stream);
int getchar(void);
char *gets(char *s);
int putc(int c, FILE *stream);
int putchar(int c);
int puts(const char *s);
int ungetc(int c, FILE *stream);
size_t fread(void *ptr, size_t size, size_t nobj, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nobj, FILE *stream);
int fseek(FILE *stream, long offset, int origin);
long ftell(FILE *stream);
void rewind(FILE *stream);
int fgetpos(FILE *stream, fpos_t *ptr);
int fsetpos(FILE *stream, const fpos_t *ptr);
void clearerr(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void perror(const char *s);

#endif /* stdio.h */
