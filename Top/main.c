/*
    main.c:

    Copyright (C) 1991-2002 Barry Vercoe, John ffitch

    This file is part of Csound.

    The Csound Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Csound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "cs.h"                 /*                               MAIN.C */
#include "soundio.h"
#include "prototyp.h"
#include "csound.h"
#include "csmodule.h"
#include <ctype.h>              /* For isdigit */

#ifdef mills_macintosh
#include <SIOUX.h>
#include "perf.h"
#include"MacTransport.h"

#define PATH_LEN        128

extern struct Transport transport;
extern Boolean displayText;

extern char sfdir_path[];
extern char sadir_path[];
extern char ssdir_path[];
extern char saved_scorename[];
extern unsigned char mytitle[];
extern Boolean util_perf;
static char *foo;
static char listing_file[PATH_LEN];
static int  vbuf;
static int csNGraphs;
static MYFLT temp;
extern int RescaleFloatFile;
#endif

static  char    *sortedscore = NULL;
static  char    *xtractedscore = "score.xtr";
static  char    scnm[255];
static  char    nme[255];
static  char    *playscore = NULL;     /* unless we extract */
static  FILE    *scorin, *scorout, *xfile;
extern  void    dieu(char *);
extern  OPARMS  O;
extern  ENVIRON cenviron;
extern int argdecode(void*, int, char**, char**, char*);
extern void init_pvsys(void);
extern int csoundYield(void *);

#include <signal.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef MSVC
#include <windows.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if !defined(LINUX) && !defined(SGI) && !defined(__BEOS__) && !defined(__MACH__)
static char *signal_to_string(int sig)
{
    switch(sig) {
#ifdef SIGHUP
    case SIGHUP:        return "Hangup";
#endif
#ifdef SIGINT
    case SIGINT:        return "Interrupt";
#endif
#ifdef SIGQUIT
    case SIGQUIT:       return "Quit";
#endif
#ifdef SIGILL
    case SIGILL:        return "Illegal instruction";
#endif
#ifdef SIGTRAP
    case SIGTRAP:       return "Trace trap";
#endif
#ifdef SIGABRT
    case SIGABRT:       return "Abort";
#endif
#ifdef SIGBUS
    case SIGBUS:        return "BUS error";
#endif
#ifdef SIGFPE
    case SIGFPE:        return "Floating-point exception";
#endif
#ifdef SIGUSR1
    case SIGUSR1:       return "User-defined signal 1";
#endif
#ifdef SIGSEGV
    case SIGSEGV:       return "Segmentation violation";
#endif
#ifdef SIGUSR2
    case SIGUSR2:       return "User-defined signal 2";
#endif
#ifdef SIGPIPE
    case SIGPIPE:       return "Broken pipe";
#endif
#ifdef SIGALRM
    case SIGALRM:       return "Alarm clock";
#endif
#ifdef SIGTERM
    case SIGTERM:       return "Termination";
#endif
#ifdef SIGSTKFLT
    case SIGSTKFLT:     return "???";
#endif
#ifdef SIGCHLD
    case SIGCHLD:       return "Child status has changed";
#endif
#ifdef SIGCONT
    case SIGCONT:       return "Continue";
#endif
#ifdef SIGSTOP
    case SIGSTOP:       return "Stop, unblockable";
#endif
#ifdef SIGTSTP
    case SIGTSTP:       return "Keyboard stop";
#endif
#ifdef SIGTTIN
    case SIGTTIN:       return "Background read from tty";
#endif
#ifdef SIGTTOU
    case SIGTTOU:       return "Background write to tty";
#endif
#ifdef SIGURG
    case SIGURG:        return "Urgent condition on socket ";
#endif
#ifdef SIGXCPU
    case SIGXCPU:       return "CPU limit exceeded";
#endif
#ifdef SIGXFSZ
    case SIGXFSZ:       return "File size limit exceeded ";
#endif
#ifdef SIGVTALRM
    case SIGVTALRM:     return "Virtual alarm clock ";
#endif
#ifdef SIGPROF
    case SIGPROF:       return "Profiling alarm clock";
#endif
#ifdef SIGWINCH
    case SIGWINCH:      return "Window size change ";
#endif
#ifdef SIGIO
    case SIGIO:         return "I/O now possible";
#endif
#ifdef SIGPWR
    case SIGPWR:        return "Power failure restart";
#endif
    default:
      return "???";
    }
}

#ifndef __MACH__
void psignal(int sig, char *str)
{
    err_printf( "%s: %s\n", str, signal_to_string(sig));
}
#endif
#endif

#if defined(__BEOS__)
void psignal(int sig, char *str)
{
    err_printf("%s: %s\n", str, strsignal(sig));
}
#endif

extern int cleanup(void *csound);

static void signal_handler(int sig)
{
#if defined(USE_FLTK) && defined(SIGALRM)
    if (sig == SIGALRM) return;
#endif
    psignal(sig, "Csound tidy up");
    fltk_abort = 1;
    /* FIXME: cannot have a csound pointer here, but signal handling */
    /* should be done by the host application anyway, and not by the */
    /* Csound library */
    cleanup((void*) &cenviron);
#ifdef LINUX
    usleep(250000);
#else
#ifndef MSVC /* VL MSVC fix */
    sleep(1);
#else
    Sleep(1000);
#endif
#endif
    exit(1);
}

static void install_signal_handler(void)
{
    int *x;
#if defined(LINUX) || defined(SGI) || defined(sol)
    int sigs[] = { SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGIOT,
                   SIGBUS, SIGFPE, SIGSEGV, SIGPIPE, SIGALRM, SIGTERM, SIGXCPU,
                   SIGXFSZ, -1};
#elif defined(__MACH__)
    int sigs[] = { SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGIOT,
                   SIGBUS, SIGFPE, SIGSEGV, SIGPIPE,          SIGTERM, SIGXCPU,
                   SIGXFSZ, -1};
#elif defined(CSWIN)
    int sigs[] = { SIGHUP, SIGINT, SIGQUIT, -1};
#elif defined(WIN32)
    int sigs[] = { SIGINT, SIGINT, SIGILL, SIGABRT, SIGFPE, SIGSEGV, SIGTERM, -1};
#elif defined(__EMX__)
    int sigs[] = { SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGBUS,
                   SIGFPE, SIGUSR1, SIGSEGV, SIGUSR2, SIGPIPE, SIGALRM, SIGTERM,
                   SIGCHLD, -1 };
#else
    int sigs[] = { -1};
#endif

    for (x = sigs; *x > 0; x++)
      signal(*x, signal_handler);
#if defined(__MACH__)
    signal(SIGALRM, SIG_IGN);
#endif
}

void create_opcodlst(void *csound)
{
    extern OENTRY opcodlst_1[], opcodlst_2[];
    extern long oplength_1, oplength_2;
    long length = 0;

    /* Basic Entry1 stuff */
    if (opcodlst!=NULL) return;
    opcodlst = (OENTRY*) mmalloc(length = oplength_1);
    memcpy(opcodlst, opcodlst_1, oplength_1);
    oplstend = opcodlst +  length/sizeof(OENTRY);
    /* Add entry2 */
    opcodlst = (OENTRY*) mrealloc(opcodlst, length + oplength_2);
    memcpy(opcodlst+length/sizeof(OENTRY), opcodlst_2, oplength_2);
    length += oplength_2;
    oplstend = opcodlst +  length/sizeof(OENTRY);
    csoundLoadExternals(csound);
}

extern  int    musmon(ENVIRON*), musmon2(ENVIRON*);
extern  char  *getstrformat(int);
extern  short  sfsampsize(int);
extern int frsturnon;

/* IV - Jan 28 2005 */
void print_benchmark_info(void *csound, const char *s)
{
    double  rt, ct;
    RTCLOCK *p;

    if ((O.msglevel & 0x80) == 0)
      return;
    p = (RTCLOCK*) csoundQueryGlobalVariable(csound, "csRtClock");
    if (p == NULL)
      return;
    rt = timers_get_real_time(p);
    ct = timers_get_CPU_time(p);
    err_printf(Str("Elapsed time at %s: real: %.3fs, CPU: %.3fs\n"),
               (char*) s, rt, ct);
}

#ifdef MSVC
_declspec(dllexport) /* VL linkage fix 11-04 */
#endif
int csoundCompile(void *csound_, int argc, char **argv)
{
    char  *s;
    char  *filnamp, *envoutyp = NULL;
    int   n;
    ENVIRON *csound = (ENVIRON *)csound_;

    if ((n=setjmp(cenviron.exitjmp_))) {
      /*
       * Changed from exit(-1) for re-entrancy.
       */
      return -1;
    }
    csound->hostdata_ = hostdata;
    /* allow selecting real time audio module */
    {
      char  *s;
      int   max_len = 21;
      csoundCreateGlobalVariable(csound, "_RTAUDIO", (size_t) max_len);
      s = csoundQueryGlobalVariable(csound, "_RTAUDIO");
      strcpy(s, "PortAudio");
      csoundCreateConfigurationVariable(csound, "rtaudio", s,
                                        CSOUNDCFG_STRING, 0, NULL, &max_len,
                                        "Real time audio module name", NULL);
    }
    /* now load and pre-initialise external modules for this instance */
    /* this function returns an error value that may be worth checking */
    csoundLoadModules(csound);
    /* IV - Jan 28 2005 */
    csoundCreateGlobalVariable(csound, "csRtClock", sizeof(RTCLOCK));
    csoundCreateGlobalVariable(csound, "#CLEANUP", (size_t) 1);
    frsturnon = 0;
/*  init_getstring(argc, argv);     should be done by host application as */
/*                                string database is global to all instances */
    init_pvsys();
    /* utilities depend on this as well as orchs */
    e0dbfs = DFLT_DBFS;
    dbfs_init(e0dbfs);
    timers_struct_init((RTCLOCK*)
                        csoundQueryGlobalVariable(csound, "csRtClock"));
    /* may get changed by an orch */
    if (sizeof(MYFLT)==sizeof(float)) {
#ifdef BETA
      err_printf("Csound version %s beta (float samples) %s\n",
                 PACKAGE_VERSION, __DATE__);
#else
      err_printf("Csound version %s (float samples) %s\n",
                 PACKAGE_VERSION, __DATE__);
#endif
    }
    else {
#ifdef BETA
      err_printf("Csound version %s beta (double samples) %s\n",
                 PACKAGE_VERSION, __DATE__);
#else
      err_printf("Csound version %s (double samples) %s\n",
                 PACKAGE_VERSION, __DATE__);
#endif
    }
    {
      char buffer[128];
#include <sndfile.h>
      sf_command (NULL, SFC_GET_LIB_VERSION, buffer, 128);
      err_printf("%s\n", buffer);
    }
#if !defined(mills_macintosh) && !defined(SYMANTEC)
    {
      char *getenv(const char*);
      if ((envoutyp = getenv("SFOUTYP")) != NULL) {
        if (strcmp(envoutyp,"AIFF") == 0)
          O.filetyp = TYP_AIFF;
        else if (strcmp(envoutyp,"WAV") == 0)
          O.filetyp = TYP_WAV;
        else if (strcmp(envoutyp,"IRCAM") == 0)
          O.filetyp = TYP_IRCAM;
        else {
          sprintf(errmsg,
                  Str("%s not a recognised SFOUTYP env setting"),
                  envoutyp);
          dieu(errmsg);
        }
      }
    }
#endif
    install_signal_handler();
    O.filnamspace = filnamp = mmalloc((long)1024);
    peakchunks = 1;
    {
      FILE *csrc = fopen(".csoundrc", "r");
      if (csrc!=NULL) {
        readOptions(csound, csrc);
        fclose(csrc);
      }
    }
    if (--argc == 0) {
      dieu(Str("insufficient arguments"));
    }
    if (argdecode(csound, argc, argv, &filnamp, envoutyp)==0) {
#ifndef mills_macintosh
      longjmp(cenviron.exitjmp_,1);
#else
      return(0);
#endif
    }

    if (orchname == NULL)
      dieu(Str("no orchestra name"));
    else if ((strcmp(orchname+strlen(orchname)-4, ".csd")==0 ||
              strcmp(orchname+strlen(orchname)-4, ".CSD")==0) &&
             (scorename==NULL || strlen(scorename)==0)) {
      int read_unified_file(void*, char **, char **);
      err_printf("UnifiedCSD:  %s\n", orchname);
      if (!read_unified_file(csound, &orchname, &scorename)) {
        err_printf(Str("Decode failed....stopping\n"));
        longjmp(cenviron.exitjmp_,1);
      }
    }
    if (scorename==NULL || strlen(scorename)==0) { /* No scorename yet */
      char *p;
      FILE *scof;
      extern char sconame[];
      void deleteScore(void);
      tmpnam(sconame);              /* Generate score name */
      if ((p=strchr(sconame, '.')) != NULL) *p='\0'; /* with extention */
      strcat(sconame, ".sco");
      scof = fopen(sconame, "w");
      fprintf(scof, "f0 86400\n");
      fclose(scof);
      scorename = sconame;
      add_tmpfile(csound, sconame);     /* IV - Feb 03 2005 */
    }
    if (O.Linein || O.Midiin || O.FMidiin)
      O.RTevents = 1;
    if (O.RTevents || O.sfread)
      O.ksensing = 1;
    if (O.rewrt_hdr && !O.sfheader)
      dieu(Str("cannot rewrite header if no header requested"));
    if (O.sr_override || O.kr_override) {
      long ksmpsover;
      if (!O.sr_override || !O.kr_override)
        dieu(Str("srate and krate overrides must occur jointly"));
      ksmpsover = O.sr_override / O.kr_override;
      if (ksmpsover * O.kr_override != O.sr_override)
        dieu(Str("command-line srate / krate not integral"));
    }
    if (!O.outformat)                       /* if no audioformat yet  */
      O.outformat = AE_SHORT;             /*  default to short_ints */
    O.sfsampsize = sfsampsize(O.outformat);
    O.informat = O.outformat; /* informat defaults; resettable by readinheader */
    O.insampsiz = O.sfsampsize;
    if (O.filetyp == TYP_AIFF ||
        O.filetyp == TYP_WAV) {
      if (!O.sfheader)
        dieu(Str("cannot write AIFF/WAV soundfile with no header"));
      /* WAVE format supports only unsigned bytes for 1- to 8-bit
         samples and signed short integers for 9 to 16-bit samples.
         -- Jonathan Mohr  1995 Oct 17  */
      /* Also seems that type 3 is floats */
      if (
          (
           O.outformat == AE_ALAW ||
           O.outformat == AE_ULAW ||
#ifdef mills_macintosh
           (O.outformat == AE_FLOAT && !RescaleFloatFile)
#else
           O.outformat == AE_FLOAT
#endif
           ))
        printf(Str("WARNING: %s encoding information cannot\n"
                   "be contained in the header...\n"),
               getstrformat(O.outformat));
    }
    err_printf(Str("orchname:  %s\n"), orchname);
    if (scorename != NULL)
      err_printf(Str("scorename: %s\n"), scorename);
    if (xfilename != NULL)
      err_printf(Str("xfilename: %s\n"), xfilename);
#if defined(WIN32) || defined(__EMX__)
    {
      if (O.odebug) setvbuf(stdout,0,_IOLBF,0xff);
    }
#else
#if !defined(SYMANTEC) && !defined(mac_classic)
    if (O.odebug) setlinebuf(stdout);
#endif
#endif
    /* IV - Oct 31 2002: moved orchestra compilation here, so that named */
    /* instrument numbers are known at the score read/sort stage */
    create_opcodlst(&cenviron); /* create initial opcode list (if not done yet) */
    /* IV - Jan 31 2005: initialise external modules */
    /* FIXME: check return value */
    csoundInitModules(csound);
    otran();                 /* read orcfile, setup desblks & spaces     */
    /* IV - Jan 28 2005 */
    print_benchmark_info(csound, Str("end of orchestra compile"));
    if (!csoundYield(&cenviron)) return (-1);
    /* IV - Oct 31 2002: now we can read and sort the score */
    if (scorename == NULL || scorename[0]=='\0') {
      if (O.RTevents) {
        err_printf(Str("realtime performance using dummy "
                       "numeric scorefile\n"));
        goto perf;
      }
      else {
        if (keep_tmp) {
          scorename = "score.srt";
        }
        else {
          scorename = tmpnam(scnm);
          add_tmpfile(csound, scorename);       /* IV - Feb 03 2005 */
        }
      }
    }
#ifdef mills_macintosh
    {
      char *c;
      strcpy(saved_scorename,scorename);
      strcpy((char *)mytitle,scorename);
      c = (char *)&mytitle[0] + strlen((char *)mytitle);
      while (*c != DIRSEP && c != (char *)mytitle) c -= 1;
      if (c != (char *) mytitle) c += 1;
      strcpy((char *)mytitle,c);
      strcat((char *)mytitle," listing");
      SIOUXSetTitle((unsigned char *)CtoPstr((char *)mytitle));
    }
#endif
    if ((n = strlen(scorename)) > 4            /* if score ?.srt or ?.xtr */
        && (!strcmp(scorename+n-4,".srt") ||
            !strcmp(scorename+n-4,".xtr"))) {
      err_printf(Str("using previous %s\n"),scorename);
      playscore = sortedscore = scorename;            /*   use that one */
    }
    else {
      if (keep_tmp) {
        playscore = sortedscore = "score.srt";
      }
      else {
        playscore = sortedscore = tmpnam(nme);
        add_tmpfile(csound, playscore);         /* IV - Feb 03 2005 */
      }
      if (!(scorin = fopen(scorename, "r")))          /* else sort it   */
        dies(Str("cannot open scorefile %s"), scorename);
      if (!(scorout = fopen(sortedscore, "w")))
        dies(Str("cannot open %s for writing"), sortedscore);
      err_printf(Str("sorting score ...\n"));
      scsort(scorin, scorout);
      fclose(scorin);
      fclose(scorout);
    }
    if (xfilename != NULL) {                        /* optionally extract */
      if (!strcmp(scorename,"score.xtr"))
        dies(Str("cannot extract %s, name conflict"),scorename);
      if (!(xfile = fopen(xfilename, "r")))
        dies(Str("cannot open extract file %s"), xfilename);
      if (!(scorin = fopen(sortedscore, "r")))
        dies(Str("cannot reopen %s"), sortedscore);
      if (!(scorout = fopen(xtractedscore, "w")))
        dies(Str("cannot open %s for writing"), xtractedscore);
      err_printf(Str("  ... extracting ...\n"));
      scxtract(scorin, scorout, xfile);
      fclose(scorin);
      fclose(scorout);
      playscore = xtractedscore;
    }
    err_printf(Str("\t... done\n"));
    s = playscore;
    O.playscore = filnamp;
    while ((*filnamp++ = *s++));    /* copy sorted score name */
    /* IV - Jan 28 2005 */
    print_benchmark_info(csound, Str("end of score sort"));
 perf:
    O.filnamsize = filnamp - O.filnamspace;
    return musmon(csound);
}

int csoundMain(void *csound, int argc, char **argv)
{
    jmp_buf lj;
    int returnvalue;
    extern void csoundMessage(void *, const char *, ...);
    if ((returnvalue = setjmp(lj))) {
      csoundMessage(csound, "Error return.");
      return returnvalue;
    }

    returnvalue = csoundCompile(csound, argc, argv);
    printf("Compile returns %d\n", returnvalue);
    if (returnvalue) return returnvalue;
    printf("musmon returns %d\n", returnvalue);
    if (returnvalue) return returnvalue;
    return musmon2(csound);
}

/* #ifdef mills_macintosh        /\* comment out - 062404, akozar *\/ */
/* #else */
extern void csoundMessageV(void *, const char *, va_list);
void err_printf(char *fmt, ...)
{
    va_list a;
    va_start(a, fmt);
    csoundMessageV(cenviron.hostdata_, fmt, a);
    va_end(a);
}
/* #endif */

void mainRESET(ENVIRON *p)
{
    void adsynRESET(void);
    void argdecodeRESET(void);
    void cscoreRESET(void);
    void disprepRESET(void);
    void expRESET(void);
    void ftRESET(ENVIRON *);
    void fftRESET(void);
    void insertRESET(void);
    void lpcRESET(void);
    void memRESET(void);
    void musRESET(void);
    void oloadRESET(void);
    void orchRESET(void);
    void soundinRESET(void);
    void tranRESET(void);

    cleanup((void*) p);                 /* IV - Feb 03 2005 */

#if defined(USE_FLTK) && defined(never)        /* IV - Nov 30 2002 */
    void widgetRESET(void);     /* N.B. this is not used yet, */
                                /* because it was not fully tested, */
    widgetRESET();              /* and may crash on some systems */
#endif
    cscoreRESET();
    expRESET();
    ftRESET(p);
    fftRESET();
    disprepRESET();
    insertRESET();
    musRESET();
    tranRESET();
    orchRESET();
    soundinRESET();
    adsynRESET();
    lpcRESET();
    argdecodeRESET();
    while (reset_list) {
      RESETTER *x = reset_list->next;
      (*reset_list->fn)(p);
      mfree(reset_list);
      reset_list = x;
    }
    scoreRESET(p);
    oloadRESET();               /* should be called last but changed!! */
/*  remove_tmpfiles((void*) p);    IV - Feb 03 2005 */
    memRESET();
    p->spoutactive_ = 0;
    O.Midiin = 0;
    p->nrecs_ = 0;
    p->orchname_ = NULL;
    p->scorename_ = NULL;
}

/**
* For re-entrancy.
*/

void csoundMainCleanup(void *csound)
{
    cleanup(csound);            /* IV - Feb 03 2005 */
}
