/*
    namedins.c:

    Copyright (C) 2002, 2005 Istvan Varga

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

/* namedins.c -- written by Istvan Varga, Oct 2002, Jan 2005 */

#include "csoundCore.h"
#include "namedins.h"
#include <ctype.h>

typedef struct namedInstr {
    long        instno;
    char        *name;
    INSTRTXT    *ip;
    struct namedInstr   *prv;
} INSTRNAME;

typedef struct CsoundOpcodePluginFile_s {
    /* file name (base name only) */
    char                        *fname;
    /* pointer to next link in chain */
    struct CsoundOpcodePluginFile_s *nxt;
    /* file name (full path) */
    char                        *fullName;
    /* is this file already loaded ? (0: no, 1: yes, -1: does not exist) */
    int                         isLoaded;
} CsoundOpcodePluginFile_t;

typedef struct CsoundPluginOpcode_s {
    /* opcode name */
    char                        *opname;
    /* pointer to next link in chain */
    struct CsoundPluginOpcode_s *nxt;
    /* associated plugin library */
    CsoundOpcodePluginFile_t    *fp;
} CsoundPluginOpcode_t;

/* do not touch this ! */

const unsigned char strhash_tabl_8[256] = {
        230,  69,  87,  14,  21, 133, 233, 229,  54, 111, 196,  53, 128,  23,
         66, 225,  67,  79, 173, 110, 116,  56,  48, 129,  89, 188,  29, 251,
        186, 159, 102, 162, 227,  57, 220, 244, 165, 243, 215, 216, 214,  33,
        172, 254, 247, 241, 121, 197,  83,  22, 142,  61, 199,  50, 140, 192,
          6, 237, 183,  46, 206,  81,  18, 105, 147, 253,  15,  97, 179, 163,
        108, 123,  59, 198,  19, 141,   9,  95,  25, 219, 222,   1,   5,  52,
         90, 138,  11, 234,  55,  60, 209,  39,  80, 203, 120,   4,  64, 146,
        153, 157, 194, 134, 174, 100, 107, 125, 236, 160, 150,  41,  12, 223,
        135, 189, 122, 171,  10, 221,  71,  68, 106,  73, 218, 115,   2, 152,
        132, 190, 185, 113, 139, 104, 151, 154, 248, 117, 193, 118, 136, 204,
         17, 239, 158,  77, 103, 182, 250, 191, 170,  13,  75,  85,  62,   0,
        164,   8, 178,  93,  47,  42, 177,   3, 212, 255,  35, 137,  31, 224,
        242,  88, 161, 145,  49, 119, 143, 245, 201,  38, 211,  96, 169,  98,
         78, 195,  58, 109,  40, 238, 114,  20,  99,  24, 175, 200, 148, 112,
         45,   7,  28, 168,  27, 249,  94, 205, 156,  44,  37,  82, 217,  36,
         30,  16, 101,  72,  43, 149, 144, 187,  65, 131, 184, 166,  51,  32,
        226, 202, 231, 213, 126, 210, 235,  74, 208, 252, 181, 155, 246,  92,
         63, 228, 180, 176,  76, 167, 232,  91, 130,  84, 124,  86,  34,  26,
        207, 240, 127,  70
};

static inline unsigned char name_hash(const char *s)
{
    unsigned char *c = (unsigned char*) &(s[0]);
    unsigned char h = (unsigned char) 0;
    for ( ; *c != (unsigned char) 0; c++)
      h = strhash_tabl_8[*c ^ h];
    return h;
}

/* faster version that assumes non-empty string */

static inline unsigned char name_hash_2(const char *s)
{
    unsigned char *c = (unsigned char*) &(s[0]);
    unsigned char h = (unsigned char) 0;
    do {
      h = strhash_tabl_8[*c ^ h];
    } while (*(++c) != (unsigned char) 0);
    return h;
}

static inline int sCmp(const char *x, const char *y)
{
    int tmp = 0;
    while (x[tmp] == y[tmp] && x[tmp] != (char) 0)
      tmp++;
    return (x[tmp] != y[tmp]);
}

/* check if the string s is a valid instrument or opcode name */
/* return value is zero if the string is not a valid name */

int check_instr_name(char *s)
{
    char    *c = s;

    if (!*c) return 0;  /* empty */
    if (!isalpha(*c) && *c != '_') return 0;    /* chk if 1st char is valid */
    while (*++c)
      if (!isalnum(*c) && *c != '_') return 0;
    return 1;   /* no errors */
}

/* find the instrument number for the specified name */
/* return value is zero if none was found */

long named_instr_find(CSOUND *csound, char *s)
{
    INSTRNAME     *inm;
    unsigned char h = name_hash(s);     /* calculate hash value */

    if (!csound->instrumentNames) return 0L;  /* no named instruments defined */
    /* now find instrument */
    inm = ((INSTRNAME**) csound->instrumentNames)[h];
    while (inm) {
      if (!sCmp(inm->name, s))
        return (long) inm->instno;
      inm = inm->prv;
    }
    return 0L;  /* not found */
}

/* allocate entry for named instrument ip with name s (s must not be freed */
/* after the call, because only the pointer is stored); instrument number */
/* is set to insno */
/* returns zero if the named instr entry could not be allocated */
/* (e.g. because it already exists) */

int named_instr_alloc(CSOUND *csound, char *s, INSTRTXT *ip, long insno)
{
    INSTRNAME   **inm_base = (INSTRNAME**) csound->instrumentNames, *inm, *inm2;
    unsigned char h = name_hash(s);     /* calculate hash value */

    if (!inm_base)
      /* no named instruments defined yet */
      inm_base = csound->instrumentNames =
                 (void*) mcalloc(csound, sizeof(INSTRNAME*) * 258);
    /* now check if instrument is already defined */
    if ((inm = inm_base[h])) {
      while (inm && sCmp(inm->name, s)) inm = inm->prv;
      if (inm) return 0;        /* error: instr exists */
    }
    /* allocate entry, */
    inm = (INSTRNAME*) mcalloc(csound, sizeof(INSTRNAME));
    inm2 = (INSTRNAME*) mcalloc(csound, sizeof(INSTRNAME));
    /* and store parameters */
    inm->name = s; inm->ip = ip;
    inm2->instno = insno;
    inm2->name = (char*) inm;   /* hack */
    /* link into chain */
    inm->prv = inm_base[h];
    inm_base[h] = inm;
    /* temporary chain for use by named_instr_assign_numbers() */
    if (!inm_base[256])
      inm_base[256] = inm2;
    else
      inm_base[257]->prv = inm2;
    inm_base[257] = inm2;
    if (csound->oparms->odebug)
      csound->Message(csound,
                      "named instr name = \"%s\", hash = %d, txtp = %p\n",
                      s, (int) h, (void*) ip);
    return 1;
}

/* assign instrument numbers to all named instruments */
/* called by otran */

void named_instr_assign_numbers(CSOUND *csound)
{
    INSTRNAME   *inm, *inm2, **inm_first, **inm_last;
    int     num = 0, insno_priority = 0;

    if (!csound->instrumentNames) return;       /* no named instruments */
    inm_first = (INSTRNAME**) csound->instrumentNames + 256;
    inm_last = inm_first + 1;
    while (--insno_priority > -3) {
      if (insno_priority == -2) {
        num = csound->maxinsno;         /* find last used instr number */
        while (!csound->instrtxtp[num] && --num);
      }
      for (inm = *inm_first; inm; inm = inm->prv) {
        if ((int) inm->instno != insno_priority) continue;
        /* the following is based on code by Matt J. Ingalls */
        /* find an unused number and use it */
        while (++num <= csound->maxinsno && csound->instrtxtp[num]);
        /* we may need to expand the instrument array */
        if (num > csound->maxinsno) {
          int m = csound->maxinsno;
          csound->maxinsno += MAXINSNO; /* Expand */
          csound->instrtxtp = (INSTRTXT**)
            mrealloc(csound, csound->instrtxtp,
                             (1 + csound->maxinsno) * sizeof(INSTRTXT*));
          /* Array expected to be nulled so.... */
          while (++m <= csound->maxinsno) csound->instrtxtp[m] = NULL;
        }
        /* hack: "name" actually points to the corresponding INSTRNAME */
        inm2 = (INSTRNAME*) (inm->name);    /* entry in the table */
        inm2->instno = (long) num;
        csound->instrtxtp[num] = inm2->ip;
        if (csound->oparms->msglevel)
          csound->Message(csound, Str("instr %s uses instrument number %d\n"),
                                  inm2->name, num);
      }
    }
    /* clear temporary chains */
    inm = *inm_first;
    while (inm) {
      INSTRNAME *nxtinm = inm->prv;
      mfree(csound, inm);
      inm = nxtinm;
    }
    *inm_first = *inm_last = NULL;
}

/* convert opcode string argument to instrument number */
/* return value is -1 if the instrument cannot be found */
/* (in such cases, csoundInitError() is also called) */

long strarg2insno(CSOUND *csound, void *p, int is_string)
{
    long    insno;

    if (is_string) {
      if ((insno = named_instr_find(csound, (char*) p)) <= 0) {
        csound->InitError(csound, "instr %s not found", (char*) p);
        return -1;
      }
    }
    else {      /* numbered instrument */
      insno = (long) *((MYFLT*) p);
      if (insno < 1 || insno > csound->maxinsno || !csound->instrtxtp[insno]) {
        csound->InitError(csound, "Cannot Find Instrument %d", (int) insno);
        return -1;
      }
    }
    return insno;
}

/* same as strarg2insno, but runs at perf time, */
/* and does not support numbered instruments */
/* (used by opcodes like event or schedkwhen) */

long strarg2insno_p(CSOUND *csound, char *s)
{
    long    insno;

    if (!(insno = named_instr_find(csound, s))) {
      csound->PerfError(csound, "instr %s not found", s);
      return -1;
    }
    return insno;
}

/* convert opcode string argument to instrument number */
/* (also allows user defined opcode names); if the integer */
/* argument is non-zero, only opcode names are searched */
/* return value is -1 if the instrument cannot be found */
/* (in such cases, csoundInitError() is also called) */

long strarg2opcno(CSOUND *csound, void *p, int is_string, int force_opcode)
{
    long    insno = 0;

    if (!force_opcode) {        /* try instruments first, if enabled */
      if (is_string) {
        insno = named_instr_find(csound, (char*) p);
      }
      else {      /* numbered instrument */
        insno = (long) *((MYFLT*) p);
        if (insno < 1 || insno > csound->maxinsno ||
            !csound->instrtxtp[insno]) {
          csound->InitError(csound, "Cannot Find Instrument %d", (int) insno);
          return -1;
        }
      }
    }
    if (!insno && is_string) {              /* if no instrument was found, */
      OPCODINFO *inm = csound->opcodeInfo;  /* search for user opcode */
      while (inm && sCmp(inm->name, (char*) p)) inm = inm->prv;
      if (inm) insno = (long) inm->instno;
    }
    if (insno < 1) {
      csound->InitError(csound,
                        Str("cannot find the specified instrument or opcode"));
      insno = -1;
    }
    return insno;
}

/* create file name from opcode argument (string or MYFLT)      */
/*   CSOUND *csound:                                            */
/*      pointer to Csound instance                              */
/*   char *s:                                                   */
/*      output buffer, should have enough space; if NULL, the   */
/*      required amount of memory is allocated and returned     */
/*   void *p:                                                   */
/*      opcode argument, is interpreted as char* or MYFLT*,     */
/*      depending on the 'is_string' parameter                  */
/*   const char *baseName:                                      */
/*      name prefix to be used if the 'p' argument is MYFLT,    */
/*      and it is neither SSTRCOD, nor a valid index to strset  */
/*      space.                                                  */
/*      For example, if "soundin." is passed as baseName, file  */
/*      names in the format "soundin.%d" will be generated.     */
/*      baseName may be an empty string, but should not be NULL */
/*   int is_string:                                             */
/*      if non-zero, 'p' is interpreted as a char* pointer and  */
/*      is used as the file name. Otherwise, it is expected to  */
/*      point to a MYFLT value, and the following are tried:    */
/*        1. if the value is SSTRCOD, the string argument of    */
/*           the current score event is used (string p-field)   */
/*        2. if the value, rounded to the nearest integer, is a */
/*           valid index to strset space, the strset string is  */
/*           used                                               */
/*        3. the file name is generated using baseName and the  */
/*           value rounded to the nearest integer, as described */
/*           above                                              */
/*      'is_string' is usually p->XSTRCODE for an opcode with   */
/*      only one string argument, otherwise it is               */
/*      p->XSTRCODE & (1 << (argno - 1))                        */
/*   return value:                                              */
/*      pointer to the output string; if 's' is not NULL, it is */
/*      always the same as 's', otherwise it is allocated with  */
/*      mmalloc() and the caller is responsible for freeing the */
/*      allocated memory with mfree() or csound->Free()         */

char *strarg2name(CSOUND *csound, char *s, void *p, const char *baseName,
                                  int is_string)
{
    if (is_string) {
      /* opcode string argument */
      if (s == NULL)
        s = mmalloc(csound, strlen((char*) p) + 1);
      strcpy(s, (char*) p);
    }
    else if (*((MYFLT*) p) == SSTRCOD) {
      /* p-field string, unquote and copy */
      char  *s2 = csound->currevent->strarg;
      int   i = 0;
      if (s == NULL)
        s = mmalloc(csound, strlen(csound->currevent->strarg) + 1);
      if (*s2 == '"')
        s2++;
      while (*s2 != '"' && *s2 != '\0')
        s[i++] = *(s2++);
      s[i] = '\0';
    }
    else {
      int   i = (int) ((double) *((MYFLT*) p)
                       + (*((MYFLT*) p) >= FL(0.0) ? 0.5 : -0.5));
      if (i >= 0 && i <= (int) csound->strsmax &&
          csound->strsets != NULL && csound->strsets[i] != NULL) {
        if (s == NULL)
          s = mmalloc(csound, strlen(csound->strsets[i]) + 1);
        strcpy(s, csound->strsets[i]);
      }
      else {
        if (s == NULL)
          /* allocate +20 characters, assuming sizeof(int) <= 8 */
          s = mmalloc(csound, strlen(baseName) + 21);
        sprintf(s, "%s%d", baseName, i);
      }
    }
    return s;
}

/* ----------------------------------------------------------------------- */
/* the following functions are for efficient management of the opcode list */

static CS_NOINLINE int loadPluginOpcode(CSOUND *csound,
                                        CsoundOpcodePluginFile_t *fp,
                                        const char *opname, unsigned char h)
{
    int     n;

    if (fp->isLoaded != 0)
      return 0;
    n = csoundLoadAndInitModule(csound, fp->fullName);
    if (n != 0) {
      fp->isLoaded = -1;
      if (n != CSOUND_ERROR)
        csound->LongJmp(csound, (n == CSOUND_MEMORY ? n : CSOUND_ERROR));
      return 0;
    }
    fp->isLoaded = 1;
    n = ((int*) csound->opcode_list)[h];
    while (n && sCmp(csound->opcodlst[n].opname, opname))
      n = csound->opcodlst[n].prvnum;

    return n;
}

/* find opcode with the specified name in opcode list */
/* returns index to opcodlst[], or zero if the opcode cannot be found */

int find_opcode(CSOUND *csound, char *opname)
{
    int           n;
    unsigned char h;

    if (opname[0] == (char) 0)
      return 0;
    /* calculate hash value */
    h = name_hash_2(opname);
    /* now find entry in opcode chain */
    n = ((int*) csound->opcode_list)[h];
    while (n && sCmp(csound->opcodlst[n].opname, opname))
      n = csound->opcodlst[n].prvnum;
    if (!n && csound->pluginOpcodeDB != NULL) {
      CsoundPluginOpcode_t  *p;
      /* not found, check for deferred plugin loading */
      p = ((CsoundPluginOpcode_t**) csound->pluginOpcodeDB)[h];
      while (p) {
        if (!sCmp(p->opname, opname))
          return loadPluginOpcode(csound, p->fp, opname, h);
        p = p->nxt;
      }
    }

    return n;
}

/* ----------------------------------------------------------------------- */
/* These functions replace the functionality of strsav() in rdorch.c.      */

#define STRSPACE    (8000)      /* number of bytes in a buffer  */

typedef struct strsav_t {
        struct strsav_t *nxt;   /* pointer to next structure    */
        char    s[1];           /* the string stored            */
} STRSAV;

typedef struct strsav_space_t {
        char    sp[STRSPACE];           /* string space                */
        int     splim;                  /* number of bytes allocated   */
        struct strsav_space_t   *prv;   /* ptr to previous buffer      */
} STRSAV_SPACE;

#define STRSAV_STR_     ((STRSAV**) (csound->strsav_str))
#define STRSAV_SPACE_   ((STRSAV_SPACE*) (csound->strsav_space))

/* allocate space for strsav (called once from rdorchfile()) */

void strsav_create(CSOUND *csound)
{
    if (csound->strsav_space != NULL) return;   /* already allocated */
    csound->strsav_space = mcalloc(csound, sizeof(STRSAV_SPACE));
    csound->strsav_str = mcalloc(csound, sizeof(STRSAV*) * 256);
}

/* Locate string s in database, and return address of stored string (not */
/* necessarily the same as s). If the string is not defined yet, it is   */
/* copied to the database (in such cases, it is allowed to free s after  */
/* the call).                                                            */

char *strsav_string(CSOUND *csound, char *s)
{
    STRSAV        *ssp;
    int           n;
    unsigned char h = name_hash(s);     /* calculate hash value */

    /* now find entry in database */
    ssp = STRSAV_STR_[h];
    while (ssp) {
      if (!sCmp(ssp->s, s)) return (ssp->s);    /* already defined */
      ssp = ssp->nxt;
    }
    /* not found, so need to allocate a new entry */
    n = (int) sizeof(STRSAV) + (int) strlen(s); /* number of bytes */
    n = (n + 7) & (~7);           /* round up for 8 byte alignment */
    if ((STRSAV_SPACE_->splim + n) > STRSPACE) {
      STRSAV_SPACE  *sp;
      /* not enough space, allocate new buffer */
      if (n > STRSPACE) {
        /* this should not happen */
        csound->Message(csound,
                        "internal error: strsav: string length > STRSPACE\n");
        return NULL;
      }
      sp = (STRSAV_SPACE*) mcalloc(csound, sizeof(STRSAV_SPACE));
      sp->prv = STRSAV_SPACE_;
      csound->strsav_space = sp;
    }
    /* use space from buffer */
    ssp = (STRSAV*) ((char*) STRSAV_SPACE_->sp + STRSAV_SPACE_->splim);
    STRSAV_SPACE_->splim += n;
    strcpy(ssp->s, s);          /* save string */
    /* link into chain */
    ssp->nxt = STRSAV_STR_[h];
    STRSAV_STR_[h] = ssp;
    return (ssp->s);
}

/* -------- IV - Jan 29 2005 -------- */

/* #define CSGLOBALS_USE_TREE  1 */

#ifdef CSGLOBALS_USE_TREE

static void fix_pointers_in_db(void **p,
                               unsigned char *oldp, unsigned char *newp)
{
    void **pp;
    int  i, j;
    /* hack to fix pointers in globals database after realloc() */
    for (i = 0; i < 16; i++) {
      if (p[i] == (void*) NULL)
        continue;       /* nothing here */
      pp = (void**) ((unsigned char*) (p[i]) + (newp - oldp));
      p[i] = (void*) pp;
      for (j = 0; j < 16; j += 2) {
        if (pp[j] != (void*) NULL) {
          /* recursively search entire database */
          pp[j] = (void*) ((unsigned char*) (pp[j]) + (newp - oldp));
          fix_pointers_in_db((void**) pp[j], oldp, newp);
        }
      }
    }
}

static void **extendNamedGlobals(CSOUND *p, int n, int storeIndex)
{
    void  **ptr = NULL;
    int   i, oldlimit;

    oldlimit = p->namedGlobalsCurrLimit;
    p->namedGlobalsCurrLimit += n;
    if (p->namedGlobalsCurrLimit > p->namedGlobalsMaxLimit) {
      p->namedGlobalsMaxLimit = p->namedGlobalsCurrLimit;
      p->namedGlobalsMaxLimit += (p->namedGlobalsMaxLimit >> 3);
      p->namedGlobalsMaxLimit = (p->namedGlobalsMaxLimit + 15) & (~15);
      ptr = p->namedGlobals;
      p->namedGlobals = (void**) realloc((void*) p->namedGlobals,
                                         sizeof(void*)
                                         * (size_t) p->namedGlobalsMaxLimit);
      if (p->namedGlobals == NULL) {
        p->namedGlobalsCurrLimit = p->namedGlobalsMaxLimit = 0;
        return NULL;
      }
      if (p->namedGlobals != ptr && ptr != NULL) {
        /* realloc() moved the data, need to fix pointers */
        fix_pointers_in_db(p->namedGlobals, (unsigned char*) ptr,
                                            (unsigned char*) p->namedGlobals);
      }
      /* clear new allocated space to zero */
      for (i = oldlimit; i < p->namedGlobalsMaxLimit; i++)
        p->namedGlobals[i] = (void*) NULL;
    }
    ptr = (void**) p->namedGlobals + (int) oldlimit;
    if (storeIndex >= 0) {
      /* if requested, store pointer to new area at specified array index */
      p->namedGlobals[storeIndex] = (void*) ptr;
    }
    return ptr;
}

/**
 * Allocate nbytes bytes of memory that can be accessed later by calling
 * csoundQueryGlobalVariable() with the specified name; the space is
 * cleared to zero.
 * Returns CSOUND_SUCCESS on success, CSOUND_ERROR in case of invalid
 * parameters (zero nbytes, invalid or already used name), or
 * CSOUND_MEMORY if there is not enough memory.
 */
PUBLIC int csoundCreateGlobalVariable(CSOUND *csnd,
                                      const char *name, size_t nbytes)
{
    void    **p = NULL;
    int     i, j, k, len;
    /* create new empty database if it does not exist yet */
    if (csnd->namedGlobals == NULL) {
      if (extendNamedGlobals(csnd, 16, -1) == (void**) NULL)
        return CSOUND_MEMORY;
    }
    /* check for a valid name */
    if (name == NULL)
      return CSOUND_ERROR;
    if (name[0] == '\0')
      return CSOUND_ERROR;
    len = (int) strlen(name);
    for (i = 0; i < len; i++)
      if ((unsigned char) name[i] >= (unsigned char) 0x80)
        return CSOUND_ERROR;
    /* cannot allocate zero bytes */
    if ((int) nbytes < 1)
      return CSOUND_ERROR;
    /* store in tree */
    i = -1;
    p = csnd->namedGlobals;
    while (++i < (len - 1)) {
      j = ((int) name[i] & 0x78) >> 3;  /* bits 3-6 */
      k = ((int) name[i] & 0x07) << 1;  /* bits 0-2 */
      if (p[j] == (void*) NULL) {
        p = extendNamedGlobals(csnd, 16, (int) ((void**) &(p[j])
                                                - csnd->namedGlobals));
        if (p == NULL)
          return CSOUND_MEMORY;
      }
      else
        p = (void**) (p[j]);
      if (p[k] == (void*) NULL) {
        p = extendNamedGlobals(csnd, 16, (int) ((void**) &(p[k])
                                                - csnd->namedGlobals));
        if (p == NULL)
          return CSOUND_MEMORY;
      }
      else
        p = (void**) (p[k]);
    }
    j = ((int) name[i] & 0x78) >> 3;    /* bits 3-6 */
    k = ((int) name[i] & 0x07) << 1;    /* bits 0-2 */
    if (p[j] == (void*) NULL) {
      p = extendNamedGlobals(csnd, 16, (int) ((void**) &(p[j])
                                              - csnd->namedGlobals));
      if (p == NULL)
        return CSOUND_MEMORY;
    }
    else
      p = (void**) (p[j]);
    if (p[k + 1] != (void*) NULL)
      return CSOUND_ERROR;              /* name is already defined */
    /* allocate memory and store pointer */
    p[k + 1] = (void*) malloc(nbytes);
    if (p[k + 1] == (void*) NULL)
      return CSOUND_MEMORY;
    memset(p[k + 1], 0, nbytes);        /* clear space to zero */
    /* successfully finished */
    return CSOUND_SUCCESS;
}

/**
 * Get pointer to space allocated with the name "name".
 * Returns NULL if the specified name is not defined.
 */
PUBLIC void *csoundQueryGlobalVariable(CSOUND *csnd, const char *name)
{
    void    **p = NULL;
    int     i, j, k, len;
    /* check if there is an actual database to search */
    if (csnd->namedGlobals == NULL)
      return NULL;
    /* check for a valid name */
    if (name == NULL)
      return NULL;
    if (name[0] == '\0')
      return NULL;
    len = (int) strlen(name);
    /* search tree */
    i = -1;
    p = csnd->namedGlobals;
    while (++i < (len - 1)) {
      if ((unsigned char) name[i] >= (unsigned char) 0x80)
        return NULL;            /* invalid name: must be 7-bit ASCII */
      j = ((int) name[i] & 0x78) >> 3;  /* bits 3-6 */
      k = ((int) name[i] & 0x07) << 1;  /* bits 0-2 */
      if (p[j] == (void*) NULL)
        return NULL;            /* not found */
      else
        p = (void**) (p[j]);
      if (p[k] == (void*) NULL)
        return NULL;            /* not found */
      else
        p = (void**) (p[k]);
    }
    if ((unsigned char) name[i] >= (unsigned char) 0x80)
      return NULL;              /* invalid name: must be 7-bit ASCII */
    j = ((int) name[i] & 0x78) >> 3;    /* bits 3-6 */
    k = ((int) name[i] & 0x07) << 1;    /* bits 0-2 */
    if (p[j] == (void*) NULL)
      return NULL;              /* not found */
    else
      p = (void**) (p[j]);
    /* return with pointer (will be NULL for undefined name) */
    return ((void*) p[k + 1]);
}

/**
 * This function is the same as csoundQueryGlobalVariable(), except the
 * variable is assumed to exist and no error checking is done.
 * Faster, but may crash or return an invalid pointer if 'name' is
 * not defined.
 */
PUBLIC void *csoundQueryGlobalVariableNoCheck(CSOUND *csnd, const char *name)
{
    void    **p = NULL;
    int     i, j, k, len;

    len = (int) strlen(name);
    /* search tree */
    i = -1;
    p = csnd->namedGlobals;
    while (++i < (len - 1)) {
      j = ((int) name[i] & 0x78) >> 3;  /* bits 3-6 */
      k = ((int) name[i] & 0x07) << 1;  /* bits 0-2 */
      p = (void**) (p[j]);
      p = (void**) (p[k]);
    }
    j = ((int) name[i] & 0x78) >> 3;    /* bits 3-6 */
    k = ((int) name[i] & 0x07) << 1;    /* bits 0-2 */
    p = (void**) (p[j]);
    /* return with pointer */
    return ((void*) p[k + 1]);
}

/**
 * Free memory allocated for "name" and remove "name" from the database.
 * Return value is CSOUND_SUCCESS on success, or CSOUND_ERROR if the name is
 * not defined.
 */
PUBLIC int csoundDestroyGlobalVariable(CSOUND *csnd, const char *name)
{
    void    **p = NULL;
    int     i, j, k, len;
    /* check for a valid name */
    if (csoundQueryGlobalVariable(csnd, name) == (void*) NULL)
      return CSOUND_ERROR;
    len = (int) strlen(name);
    /* search tree (simple version, as the name will surely be found) */
    i = -1;
    p = csnd->namedGlobals;
    while (++i < (len - 1)) {
      j = ((int) name[i] & 0x78) >> 3;  /* bits 3-6 */
      k = ((int) name[i] & 0x07) << 1;  /* bits 0-2 */
      p = (void**) (p[j]);
      p = (void**) (p[k]);
    }
    j = ((int) name[i] & 0x78) >> 3;    /* bits 3-6 */
    k = ((int) name[i] & 0x07) << 1;    /* bits 0-2 */
    p = (void**) (p[j]);
    /* free memory and clear pointer */
    free((void*) p[k + 1]);
    p[k + 1] = (void*) NULL;
    /* done */
    return CSOUND_SUCCESS;
}

/* recursively free all allocated globals */

static void free_global_variable(void **p)
{
    void **pp;
    int  i, j;
    for (i = 0; i < 16; i++) {
      if (p[i] == (void*) NULL)
        continue;       /* nothing here */
      pp = (void**) p[i];
      for (j = 0; j < 16; j += 2) {
        if (pp[j + 1] != (void*) NULL) {
          /* found allocated memory, free it now */
          free((void*) pp[j + 1]);
          pp[j + 1] = NULL;
        }
        /* recursively search entire database */
        if (pp[j] != (void*) NULL)
          free_global_variable((void**) pp[j]);
      }
    }
}

/**
 * Free entire global variable database. This function is for internal use
 * only (e.g. by RESET routines).
 */
void csoundDeleteAllGlobalVariables(CSOUND *csound)
{
    csound->namedGlobalsCurrLimit = 0;
    csound->namedGlobalsMaxLimit = 0;
    if (csound->namedGlobals == NULL)
      return;
    free_global_variable(csound->namedGlobals);
    free(csound->namedGlobals);
    csound->namedGlobals = (void**) NULL;
}

#else   /* CSGLOBALS_USE_TREE */

typedef struct GlobalVariableEntry_s {
    struct GlobalVariableEntry_s *nxt;
    unsigned char *name;
    void *p;
    void *dummy;
} GlobalVariableEntry_t;

/**
 * Allocate nbytes bytes of memory that can be accessed later by calling
 * csoundQueryGlobalVariable() with the specified name; the space is
 * cleared to zero.
 * Returns CSOUND_SUCCESS on success, CSOUND_ERROR in case of invalid
 * parameters (zero nbytes, invalid or already used name), or
 * CSOUND_MEMORY if there is not enough memory.
 */
PUBLIC int csoundCreateGlobalVariable(CSOUND *csnd,
                                      const char *name, size_t nbytes)
{
    GlobalVariableEntry_t *p, **pp;
    int                   i, structBytes, nameBytes, allocBytes;
    unsigned char         h;
    /* create new empty database if it does not exist yet */
    if (csnd->namedGlobals == NULL) {
      csnd->namedGlobals = (void**) malloc(sizeof(void*) * 256);
      if (csnd->namedGlobals == NULL)
        return CSOUND_MEMORY;
      for (i = 0; i < 256; i++)
        csnd->namedGlobals[i] = (void*) NULL;
    }
    /* check for valid parameters */
    if (name == NULL)
      return CSOUND_ERROR;
    if (name[0] == '\0')
      return CSOUND_ERROR;
    if (nbytes < (size_t) 1 || nbytes >= (size_t) 0x7F000000L)
      return CSOUND_ERROR;
    /* calculate hash value */
    h = name_hash(name);
    /* number of bytes to allocate */
    structBytes = ((int) sizeof(GlobalVariableEntry_t) + 15) & (~15);
    nameBytes = (((int) strlen(name) + 1) + 15) & (~15);
    allocBytes = ((int) nbytes + 15) & (~15);
    /* allocate memory */
    i = structBytes + nameBytes + allocBytes;
    p = (GlobalVariableEntry_t*) malloc((size_t) i);
    if (p == NULL)
      return CSOUND_MEMORY;
    /* initialise structure */
    memset((void*) p, 0, (size_t) i);
    p->nxt = (GlobalVariableEntry_t*) NULL;
    p->name = (unsigned char*) p + (int) structBytes;
    p->p = (void*) ((unsigned char*) p + (int) (structBytes + nameBytes));
    strcpy((char*) (p->name), name);
    /* link into database */
    if (csnd->namedGlobals[(int) h] == (void*) NULL) {
      /* hash value is not used yet */
      csnd->namedGlobals[(int) h] = (void*) p;
      return CSOUND_SUCCESS;
    }
    /* need to search */
    pp = (GlobalVariableEntry_t**) &(csnd->namedGlobals[(int) h]);
    do {
      /* check for a conflicting name */
      if (sCmp(name, (char*) ((*pp)->name)) == 0) {
        free((void*) p);
        return CSOUND_ERROR;
      }
      if ((*pp)->nxt == NULL)
        break;
      pp = &((*pp)->nxt);
    } while (1);
    (*pp)->nxt = (GlobalVariableEntry_t*) p;
    /* successfully finished */
    return CSOUND_SUCCESS;
}

/**
 * Get pointer to space allocated with the name "name".
 * Returns NULL if the specified name is not defined.
 */
PUBLIC void *csoundQueryGlobalVariable(CSOUND *csnd, const char *name)
{
    GlobalVariableEntry_t *p;
    unsigned char         h;
    /* check if there is an actual database to search */
    if (csnd->namedGlobals == NULL)
      return NULL;
    /* check for a valid name */
    if (name == NULL)
      return NULL;
    if (name[0] == '\0')
      return NULL;
    /* calculate hash value */
    h = name_hash_2(name);
    /* search tree */
    p = (GlobalVariableEntry_t*) (csnd->namedGlobals[(int) h]);
    if (p == NULL)
      return NULL;
    while (sCmp(name, (char*) (p->name)) != 0) {
      p = (GlobalVariableEntry_t*) p->nxt;
      if (p == NULL)
        return NULL;
    }
    return (void*) (p->p);
}

/**
 * This function is the same as csoundQueryGlobalVariable(), except the
 * variable is assumed to exist and no error checking is done.
 * Faster, but may crash or return an invalid pointer if 'name' is
 * not defined.
 */
PUBLIC void *csoundQueryGlobalVariableNoCheck(CSOUND *csnd, const char *name)
{
    GlobalVariableEntry_t *p;
    unsigned char         h;

    /* calculate hash value */
    h = name_hash_2(name);
    /* search tree */
    p = (GlobalVariableEntry_t*) (csnd->namedGlobals[(int) h]);
    while (p->nxt != NULL && sCmp(name, (char*) (p->name)) != 0)
      p = (GlobalVariableEntry_t*) p->nxt;
    return (void*) (p->p);
}

/**
 * Free memory allocated for "name" and remove "name" from the database.
 * Return value is CSOUND_SUCCESS on success, or CSOUND_ERROR if the name is
 * not defined.
 */
PUBLIC int csoundDestroyGlobalVariable(CSOUND *csnd, const char *name)
{
    GlobalVariableEntry_t *p, *prvp;
    unsigned char         h;

    /* check for a valid name */
    if (csoundQueryGlobalVariable(csnd, name) == (void*) NULL)
      return CSOUND_ERROR;
    /* calculate hash value */
    h = name_hash(name);
    /* search database (simple version, as the name will surely be found) */
    prvp = NULL;
    p = (GlobalVariableEntry_t*) (csnd->namedGlobals[(int) h]);
    while (sCmp(name, (char*) (p->name)) != 0) {
      prvp = p;
      p = (GlobalVariableEntry_t*) p->nxt;
    }
    if (prvp != NULL)
      prvp->nxt = (struct GlobalVariableEntry_s *) (p->nxt);
    else
      csnd->namedGlobals[(int) h] = (void*) (p->nxt);
    free((void*) p);
    /* done */
    return CSOUND_SUCCESS;
}

/**
 * Free entire global variable database. This function is for internal use
 * only (e.g. by RESET routines).
 */
void csoundDeleteAllGlobalVariables(CSOUND *csound)
{
    GlobalVariableEntry_t *p, *prvp;
    int                   i;

    if (csound->namedGlobals == NULL)
      return;
    for (i = 0; i < 256; i++) {
      prvp = (GlobalVariableEntry_t*) NULL;
      p = (GlobalVariableEntry_t*) csound->namedGlobals[i];
      while (p != NULL) {
        prvp = p;
        p = (GlobalVariableEntry_t*) (p->nxt);
        if (prvp != NULL)
          free((void*) prvp);
      }
    }
    free((void*) csound->namedGlobals);
    csound->namedGlobals = (void**) NULL;
}

#endif  /* CSGLOBALS_USE_TREE */

typedef struct controlChannelInfo_s {
    int     type;
    MYFLT   dflt;
    MYFLT   min;
    MYFLT   max;
} controlChannelInfo_t;

typedef struct channelEntry_s {
    struct channelEntry_s *nxt;
    controlChannelInfo_t  *info;
    MYFLT       *data;
    int         type;
    char        name[1];
} channelEntry_t;

static int delete_channel_db(CSOUND *csound, void *p)
{
    channelEntry_t  **db, *pp;
    int             i;

    (void) p;
    db = (channelEntry_t**) csound->chn_db;
    if (db == NULL)
      return 0;
    for (i = 0; i < 256; i++) {
      while (db[i] != NULL) {
        pp = db[i];
        db[i] = pp->nxt;
        if (pp->info != NULL)
          free((void*) pp->info);
        free((void*) pp);
      }
    }
    csound->chn_db = NULL;
    free((void*) db);
    return 0;
}

static inline channelEntry_t *find_channel(CSOUND *csound, const char *name)
{
    if (csound->chn_db != NULL && name[0]) {
      channelEntry_t  *pp;
      pp = ((channelEntry_t**) csound->chn_db)[name_hash_2(name)];
      for ( ; pp != NULL; pp = pp->nxt) {
        const char  *p1 = &(name[0]);
        const char  *p2 = &(pp->name[0]);
        while (1) {
          if (*p1 != *p2)
            break;
          if (*p1 == (char) 0)
            return pp;
          p1++, p2++;
        }
      }
    }
    return NULL;
}

static CS_NOINLINE channelEntry_t *alloc_channel(CSOUND *csound, MYFLT **p,
                                                 const char *name, int type)
{
    channelEntry_t  dummy;
    void            *pp;
    int             nbytes, nameOffs, dataOffs;

    (void) dummy;
    nameOffs = (int) ((char*) &(dummy.name[0]) - (char*) &dummy);
    dataOffs = nameOffs + ((int) strlen(name) + 1);
    dataOffs += ((int) sizeof(MYFLT) - 1);
    dataOffs = (dataOffs / (int) sizeof(MYFLT)) * (int) sizeof(MYFLT);
    nbytes = dataOffs;
    if (*p == NULL) {
      switch (type & CSOUND_CHANNEL_TYPE_MASK) {
      case CSOUND_CONTROL_CHANNEL:
        nbytes += (int) sizeof(MYFLT);
        break;
      case CSOUND_AUDIO_CHANNEL:
        nbytes += ((int) sizeof(MYFLT) * csound->ksmps);
        break;
      case CSOUND_STRING_CHANNEL:
        nbytes += ((int) sizeof(MYFLT) * csound->strVarSamples);
        break;
      }
    }
    pp = (void*) malloc((size_t) nbytes);
    if (pp == NULL)
      return (channelEntry_t*) NULL;
    memset(pp, 0, (size_t) nbytes);
    if (*p == NULL)
      *p = (MYFLT*) ((char*) pp + (int) dataOffs);
    return (channelEntry_t*) pp;
}

CS_NOINLINE int create_new_channel(CSOUND *csound, MYFLT **p,
                                   const char *name, int type)
{
    channelEntry_t  *pp;
    const char      *s;
    unsigned char   h;

    /* check for valid parameters and calculate hash value */
    if ((type & (~51)) || !(type & 3) || !(type & 48))
      return CSOUND_ERROR;
    s = name;
    if (!isalpha((unsigned char) *s))
      return CSOUND_ERROR;
    h = (unsigned char) 0;
    do {
      h = strhash_tabl_8[(unsigned char) *(s++) ^ h];
    } while (isalnum((unsigned char) *s) ||
             *s == (char) '_' || *s == (char) '.');
    if (*s != (char) 0)
      return CSOUND_ERROR;
    /* create new empty database on first call */
    if (csound->chn_db == NULL) {
      if (csound->RegisterResetCallback(csound, NULL, delete_channel_db) != 0)
        return CSOUND_MEMORY;
      csound->chn_db = (void*) calloc((size_t) 256, sizeof(channelEntry_t*));
      if (csound->chn_db == NULL)
        return CSOUND_MEMORY;
    }
    /* allocate new entry */
    pp = alloc_channel(csound, p, name, type);
    if (pp == NULL)
      return CSOUND_MEMORY;
    pp->nxt = ((channelEntry_t**) csound->chn_db)[h];
    pp->info = NULL;
    pp->data = (*p);
    pp->type = type;
    strcpy(&(pp->name[0]), name);
    ((channelEntry_t**) csound->chn_db)[h] = pp;

    return CSOUND_SUCCESS;
}

/**
 * Stores a pointer to the specified channel of the bus in *p,
 * creating the channel first if it does not exist yet.
 * 'type' must be the bitwise OR of exactly one of the following values,
 *   CSOUND_CONTROL_CHANNEL
 *     control data (one MYFLT value)
 *   CSOUND_AUDIO_CHANNEL
 *     audio data (csoundGetKsmps(csound) MYFLT values)
 *   CSOUND_STRING_CHANNEL
 *     string data (MYFLT values with enough space to store
 *     csoundGetStrVarMaxLen(csound) characters, including the
 *     NULL character at the end of the string)
 * and at least one of these:
 *   CSOUND_INPUT_CHANNEL
 *   CSOUND_OUTPUT_CHANNEL
 * If the channel already exists, it must match the data type (control,
 * audio, or string), however, the input/output bits are OR'd with the
 * new value. Note that audio and string channels can only be created
 * after calling csoundCompile(), because the storage size is not known
 * until then.
 * Return value is zero on success, or a negative error code,
 *   CSOUND_MEMORY  there is not enough memory for allocating the channel
 *   CSOUND_ERROR   the specified name or type is invalid
 * or, if a channel with the same name but incompatible type already exists,
 * the type of the existing channel. In the case of any non-zero return
 * value, *p is set to NULL.
 * Note: to find out the type of a channel without actually creating or
 * changing it, set 'type' to zero, so that the return value will be either
 * the type of the channel, or CSOUND_ERROR if it does not exist.
 */

PUBLIC int csoundGetChannelPtr(CSOUND *csound,
                               MYFLT **p, const char *name, int type)
{
    channelEntry_t  *pp;

    *p = (MYFLT*) NULL;
    if (name == NULL)
      return CSOUND_ERROR;
    pp = find_channel(csound, name);
    if (pp != NULL) {
      if ((pp->type ^ type) & CSOUND_CHANNEL_TYPE_MASK)
        return pp->type;
      pp->type |= (type & (CSOUND_INPUT_CHANNEL | CSOUND_OUTPUT_CHANNEL));
      *p = pp->data;
      return CSOUND_SUCCESS;
    }
    return create_new_channel(csound, p, name, type);
}

static int cmp_func(const void *p1, const void *p2)
{
    return strcmp(((CsoundChannelListEntry*) p1)->name,
                  ((CsoundChannelListEntry*) p2)->name);
}

/**
 * Returns a list of allocated channels in *lst. A CsoundChannelListEntry
 * structure contains the name and type of a channel, with the type having
 * the same format as in the case of csoundGetChannelPtr().
 * The return value is the number of channels, which may be zero if there
 * are none, or CSOUND_MEMORY if there is not enough memory for allocating
 * the list. In the case of no channels or an error, *lst is set to NULL.
 * Notes: the caller is responsible for freeing the list returned in *lst
 * with csoundDeleteChannelList(). The name pointers may become invalid
 * after calling csoundReset().
 */

PUBLIC int csoundListChannels(CSOUND *csound, CsoundChannelListEntry **lst)
{
    channelEntry_t  *pp;
    size_t          i, n;

    *lst = (CsoundChannelListEntry*) NULL;
    if (csound->chn_db == NULL)
      return 0;
    /* count the number of channels */
    for (n = (size_t) 0, i = (size_t) 0; i < (size_t) 256; i++) {
      for (pp = ((channelEntry_t**) csound->chn_db)[i];
           pp != NULL;
           pp = pp->nxt, n++)
        ;
    }
    if (!n)
      return 0;
    /* create list, initially in unsorted order */
    *lst = (CsoundChannelListEntry*) malloc(n * sizeof(CsoundChannelListEntry));
    if (*lst == NULL)
      return CSOUND_MEMORY;
    for (n = (size_t) 0, i = (size_t) 0; i < (size_t) 256; i++) {
      for (pp = ((channelEntry_t**) csound->chn_db)[i];
           pp != NULL;
           pp = pp->nxt, n++) {
        (*lst)[n].name = pp->name;
        (*lst)[n].type = pp->type;
      }
    }
    /* sort list */
    qsort((void*) (*lst), n, sizeof(CsoundChannelListEntry), cmp_func);
    /* return the number of channels */
    return (int) n;
}

/**
 * Releases a channel list previously returned by csoundListChannels().
 */

PUBLIC void csoundDeleteChannelList(CSOUND *csound, CsoundChannelListEntry *lst)
{
    (void) csound;
    if (lst != NULL)
      free(lst);
}

/**
 * Sets special parameters for a control channel. The parameters are:
 *   type:  must be one of CSOUND_CONTROL_CHANNEL_INT,
 *          CSOUND_CONTROL_CHANNEL_LIN, or CSOUND_CONTROL_CHANNEL_EXP for
 *          integer, linear, or exponential channel data, respectively,
 *          or zero to delete any previously assigned parameter information
 *   dflt:  the control value that is assumed to be the default, should be
 *          greater than or equal to 'min', and less than or equal to 'max'
 *   min:   the minimum value expected; if the control type is exponential,
 *          it must be non-zero
 *   max:   the maximum value expected, should be greater than 'min';
 *          if the control type is exponential, it must be non-zero and
 *          match the sign of 'min'
 * Returns zero on success, or a non-zero error code on failure:
 *   CSOUND_ERROR:  the channel does not exist, is not a control channel,
 *                  or the specified parameters are invalid
 *   CSOUND_MEMORY: could not allocate memory
 */

PUBLIC int csoundSetControlChannelParams(CSOUND *csound, const char *name,
                                         int type, MYFLT dflt,
                                         MYFLT min, MYFLT max)
{
    channelEntry_t  *pp;

    if (name == NULL)
      return CSOUND_ERROR;
    pp = find_channel(csound, name);
    if (pp == NULL)
      return CSOUND_ERROR;
    if ((pp->type & CSOUND_CHANNEL_TYPE_MASK) != CSOUND_CONTROL_CHANNEL)
      return CSOUND_ERROR;
    if (!type) {
      if (pp->info != NULL) {
        free((void*) pp->info);
        pp->info = NULL;
      }
      return CSOUND_SUCCESS;
    }
    switch (type) {
    case CSOUND_CONTROL_CHANNEL_INT:
      dflt = (MYFLT) ((long) MYFLT2LRND(dflt));
      min = (MYFLT) ((long) MYFLT2LRND(min));
      max = (MYFLT) ((long) MYFLT2LRND(max));
      break;
    case CSOUND_CONTROL_CHANNEL_LIN:
    case CSOUND_CONTROL_CHANNEL_EXP:
      break;
    default:
      return CSOUND_ERROR;
    }
    if (min >= max || dflt < min || dflt > max ||
        (type == CSOUND_CONTROL_CHANNEL_EXP && ((min * max) <= FL(0.0))))
      return CSOUND_ERROR;
    if (pp->info == NULL) {
      pp->info = (controlChannelInfo_t*) malloc(sizeof(controlChannelInfo_t));
      if (pp->info == NULL)
        return CSOUND_MEMORY;
    }
    pp->info->type = type;
    pp->info->dflt = dflt;
    pp->info->min = min;
    pp->info->max = max;
    return CSOUND_SUCCESS;
}

/**
 * Returns special parameters (assuming there are any) of a control channel,
 * previously set with csoundSetControlChannelParams().
 * If the channel exists, is a control channel, and has the special parameters
 * assigned, then the default, minimum, and maximum value is stored in *dflt,
 * *min, and *max, respectively, and a positive value that is one of
 * CSOUND_CONTROL_CHANNEL_INT, CSOUND_CONTROL_CHANNEL_LIN, and
 * CSOUND_CONTROL_CHANNEL_EXP is returned.
 * In any other case, *dflt, *min, and *max are not changed, and the return
 * value is zero if the channel exists, is a control channel, but has no
 * special parameters set; otherwise, a negative error code is returned.
 */

PUBLIC int csoundGetControlChannelParams(CSOUND *csound, const char *name,
                                         MYFLT *dflt, MYFLT *min, MYFLT *max)
{
    channelEntry_t  *pp;

    if (name == NULL)
      return CSOUND_ERROR;
    pp = find_channel(csound, name);
    if (pp == NULL)
      return CSOUND_ERROR;
    if ((pp->type & CSOUND_CHANNEL_TYPE_MASK) != CSOUND_CONTROL_CHANNEL)
      return CSOUND_ERROR;
    if (pp->info == NULL)
      return 0;
    (*dflt) = pp->info->dflt;
    (*min) = pp->info->min;
    (*max) = pp->info->max;
    return pp->info->type;
}

 /* ------------------------------------------------------------------------ */

/**
 * The following functions implement deferred loading of opcode plugins.
 */

/* returns non-zero if 'fname' (not full path) */
/* is marked for deferred loading */

int csoundCheckOpcodePluginFile(CSOUND *csound, const char *fname)
{
#if !(defined(LINUX) || defined(__unix__) || defined(__MACH__))
    char                        buf[512];
    size_t                      i;
#endif
    CsoundOpcodePluginFile_t    **pp, *p;
    const char                  *s;
    unsigned char               h;

    if (fname == NULL || fname[0] == (char) 0)
      return 0;
#if !(defined(LINUX) || defined(__unix__) || defined(__MACH__))
    /* on some platforms, file names are case insensitive */
    i = (size_t) 0;
    do {
      if (isupper(fname[i]))
        buf[i] = (char) tolower(fname[i]);
      else
        buf[i] = fname[i];
      if (++i >= (size_t) 512)
        return 0;
    } while (fname[i] != (char) 0);
    buf[i] = (char) 0;
    s = &(buf[0]);
#else
    s = fname;
#endif
    pp = (CsoundOpcodePluginFile_t**) csound->pluginOpcodeFiles;
    h = name_hash_2(s);
    p = (CsoundOpcodePluginFile_t*) NULL;
    if (pp) {
      p = pp[h];
      while (p) {
        if (!sCmp(p->fname, s))
          break;
        p = p->nxt;
      }
    }
    if (!p)
      return 0;
    /* file exists, but is not loaded yet */
    p->isLoaded = 0;
    return 1;
}

static CS_NOINLINE int csoundLoadOpcodeDB_AddFile(CSOUND *csound,
                                                  CsoundOpcodePluginFile_t *fp)
{
    CsoundOpcodePluginFile_t    **pp, *p;
    unsigned char               h;

    pp = (CsoundOpcodePluginFile_t**) csound->pluginOpcodeFiles;
    h = name_hash_2(fp->fname);
    p = pp[h];
    while (p) {
      /* check for a name conflict */
      if (!sCmp(p->fname, fp->fname))
        return -1;
      p = p->nxt;
    }
    fp->nxt = pp[h];
    fp->isLoaded = -1;
    pp[h] = fp;
    return 0;
}

static CS_NOINLINE int csoundLoadOpcodeDB_AddOpcode(CSOUND *csound,
                                                    CsoundPluginOpcode_t *op)
{
    CsoundPluginOpcode_t    **pp, *p;
    unsigned char           h;

    pp = (CsoundPluginOpcode_t**) csound->pluginOpcodeDB;
    h = name_hash_2(op->opname);
    p = pp[h];
    while (p) {
      /* check for a name conflict */
      if (!sCmp(p->opname, op->opname))
        return -1;
      p = p->nxt;
    }
    op->nxt = pp[h];
    pp[h] = op;
    return 0;
}

void csoundDestroyOpcodeDB(CSOUND *csound)
{
    void    *p;

    p = csound->pluginOpcodeFiles;
    csound->pluginOpcodeFiles = NULL;
    csound->pluginOpcodeDB = NULL;
    csound->Free(csound, p);
}

/* load opcodes.dir from the specified directory, and set up database */

int csoundLoadOpcodeDB(CSOUND *csound, const char *dname)
{
    char    err_msg[256];
    char    *s, *sp, *fileData = (char*) NULL;
    void    *fd = (void*) NULL;
    FILE    *fp = (FILE*) NULL;
    size_t  i, n, fileLen, fileCnt, opcodeCnt, byteCnt;
    void    *p, *p1, *p2;
    CsoundOpcodePluginFile_t  *currentFile;

    /* check file name */
    if (dname == NULL || dname[0] == (char) 0)
      return 0;
    n = strlen(dname);
    s = (char*) csound->Malloc(csound, n + (size_t) 13);
    strcpy(s, dname);
    if (s[n - 1] != (char) DIRSEP) {
      s[n] = (char) DIRSEP;
      s[n + 1] = (char) 0;
    }
    strcat(s, "opcodes.dir");
    /* open and load file */
    fd = csound->FileOpen(csound, &fp, CSFILE_STD, s, "rb", NULL);
    csound->Free(csound, s);
    if (fd == NULL)
      return 0;
    if (fseek(fp, 0L, SEEK_END) != 0) {
      sprintf(&(err_msg[0]), "seek error");
      goto err_return;
    }
    fileLen = (size_t) ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    if (fileLen == (size_t) 0) {
      csound->FileClose(csound, fd);
      return 0;
    }
    fileData = (char*) csound->Malloc(csound, fileLen + (size_t) 1);
    n = fread(fileData, (size_t) 1, fileLen, fp);
    csound->FileClose(csound, fd);
    fd = NULL;
    if (n != fileLen) {
      sprintf(&(err_msg[0]), "read error");
      goto err_return;
    }
    fileData[fileLen] = (char) '\n';
    /* check syntax, and count the number of files and opcodes */
    fileCnt = (size_t) 0;
    opcodeCnt = (size_t) 0;
    byteCnt = (size_t) 0;
    n = fileLen;
    for (i = (size_t) 0; i <= fileLen; i++) {
      if (fileData[i] == (char) ' ' || fileData[i] == (char) '\t' ||
          fileData[i] == (char) '\r' || fileData[i] == (char) '\n') {
        if (n >= fileLen)
          continue;
        fileData[i] = (char) 0;
        if (fileData[i - 1] != ':') {
          if (!fileCnt) {
            sprintf(&(err_msg[0]), "syntax error");
            goto err_return;
          }
          opcodeCnt++;
        }
#if !(defined(LINUX) || defined(__unix__) || defined(__MACH__))
        else {
          size_t  j;
          /* on some platforms, file names are case insensitive */
          for (j = n; j < (i - 1); j++) {
            if (isupper(fileData[j]))
              fileData[j] = (char) tolower(fileData[j]);
          }
        }
#endif
        n = fileLen;
        continue;
      }
      if (n >= fileLen)
        n = i;
      if (!(isalnum(fileData[i]) || fileData[i] == (char) '.' ||
            fileData[i] == (char) '-' || fileData[i] == (char) '_')) {
        if (fileData[i] == (char) ':' && i != n && i < fileLen &&
            (fileData[i + 1] == (char) ' ' || fileData[i + 1] == (char) '\t' ||
             fileData[i + 1] == (char) '\r' || fileData[i + 1] == (char) '\n'))
          fileCnt++;
        else {
          sprintf(&(err_msg[0]), "syntax error");
          goto err_return;
        }
      }
      else
        byteCnt++;
    }
    /* calculate the number of bytes to allocate */
    byteCnt += ((size_t) 256 * sizeof(CsoundOpcodePluginFile_t*));
    byteCnt += ((size_t) 256 * sizeof(CsoundPluginOpcode_t*));
    byteCnt += (fileCnt * sizeof(CsoundOpcodePluginFile_t));
    byteCnt = (byteCnt + (size_t) 15) & (~((size_t) 15));
    byteCnt += (opcodeCnt * sizeof(CsoundPluginOpcode_t));
    byteCnt += (fileCnt * (strlen(dname)
#if defined(WIN32)
                + (size_t) 6        /* "\\NAME.dll\0" */
#elif defined(__MACH__)
                + (size_t) 11       /* "/libNAME.dylib\0" */
#else
                + (size_t) 8        /* "/libNAME.so\0" */
#endif
                ));
    byteCnt += opcodeCnt;
    /* allocate and set up database */
    p = csound->Calloc(csound, byteCnt);
    csound->pluginOpcodeFiles = p;
    n = (size_t) 256 * sizeof(CsoundOpcodePluginFile_t*);
    csound->pluginOpcodeDB = (void*) &(((char*) p)[n]);
    n += ((size_t) 256 * sizeof(CsoundPluginOpcode_t*));
    p1 = (void*) &(((char*) p)[n]);
    n += (fileCnt * sizeof(CsoundOpcodePluginFile_t));
    n = (n + (size_t) 15) & (~((size_t) 15));
    p2 = (void*) &(((char*) p)[n]);
    n += (opcodeCnt * sizeof(CsoundPluginOpcode_t));
    sp = &(((char*) p)[n]);
    currentFile = (CsoundOpcodePluginFile_t*) NULL;
    i = (size_t) 0;
    while (i < fileLen) {
      if (fileData[i] == (char) ' ' || fileData[i] == (char) '\t' ||
          fileData[i] == (char) '\r' || fileData[i] == (char) '\n') {
        i++;
        continue;
      }
      s = &(fileData[i]);
      n = strlen(s);
      i += (n + (size_t) 1);
      if (s[n - 1] != (char) ':') {
        /* add opcode entry */
        CsoundPluginOpcode_t  *op_;
        op_ = (CsoundPluginOpcode_t*) p2;
        p2 = (void*) ((char*) p2 + (long) sizeof(CsoundPluginOpcode_t));
        strcpy(sp, s);
        op_->opname = sp;
        sp += ((long) strlen(s) + 1L);
        op_->fp = currentFile;
        if (csoundLoadOpcodeDB_AddOpcode(csound, op_) != 0) {
          sprintf(&(err_msg[0]), "duplicate opcode name");
          goto err_return;
        }
      }
      else {
        /* add file entry */
        CsoundOpcodePluginFile_t  *fp_;
        fp_ = (CsoundOpcodePluginFile_t*) p1;
        p1 = (void*) ((char*) p1 + (long) sizeof(CsoundOpcodePluginFile_t));
        s[n - 1] = (char) 0;
        strcpy(sp, dname);
        n = strlen(dname);
        if (sp[n - 1] == (char) DIRSEP)
          n--;
        fp_->fullName = sp;
        sp = (char*) fp_->fullName + (long) n;
#if defined(WIN32)
        sprintf(sp, "\\%s.dll", s);
#elif defined(__MACH__)
        sprintf(sp, "/lib%s.dylib", s);
#else
        sprintf(sp, "%clib%s.so", DIRSEP, s);
#endif
        fp_->fname = &(sp[1]);
        sp = (char*) strchr(fp_->fname, '\0') + 1L;
        if (csoundLoadOpcodeDB_AddFile(csound, fp_) != 0) {
          sprintf(&(err_msg[0]), "duplicate file name");
          goto err_return;
        }
        currentFile = fp_;
      }
      if ((size_t) ((char*) sp - (char*) p) > byteCnt)
        csound->Die(csound, Str(" *** internal error while "
                                "loading opcode database file"));
    }
    /* clean up */
    csound->Free(csound, fileData);
    /* plugin opcode database has been successfully loaded */
    return 0;

 err_return:
    if (fileData)
      csound->Free(csound, fileData);
    if (fd)
      csound->FileClose(csound, fd);
    csoundDestroyOpcodeDB(csound);
    csound->ErrorMsg(csound, Str(" *** error loading opcode database file: "),
                             Str(&(err_msg[0])));
    return -1;
}

/* load all pending opcode plugin libraries */
/* called when listing opcodes (-z) */

int csoundLoadAllPluginOpcodes(CSOUND *csound)
{
    CsoundOpcodePluginFile_t    *p;
    int                         i, err;

    if (csound->pluginOpcodeFiles == NULL)
      return CSOUND_SUCCESS;

    err = CSOUND_SUCCESS;
    for (i = 0; i < 256; i++) {
      p = ((CsoundOpcodePluginFile_t**) csound->pluginOpcodeFiles)[i];
      while (p) {
        if (!p->isLoaded) {
          int   retval;
          retval = csoundLoadAndInitModule(csound, p->fullName);
          p->isLoaded = (retval == 0 ? 1 : -1);
          if (retval != 0 && retval != CSOUND_ERROR) {
            /* record serious errors */
            if (retval < err)
              err = retval;
          }
        }
        p = p->nxt;
      }
    }
    csoundDestroyOpcodeDB(csound);
    /* report any errors */
    return (err == 0 || err == CSOUND_MEMORY ? err : CSOUND_ERROR);
}

#ifdef __cplusplus
}
#endif

