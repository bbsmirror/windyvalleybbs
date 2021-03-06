/*-------------------------------------------------------*/
/* cache.c	( NTHU CS MapleBBS Ver 2.36 )		 */
/*-------------------------------------------------------*/
/* target : cache up data by shared memory		 */
/* create : 95/03/29				 	 */
/* update : 95/12/15				 	 */
/*-------------------------------------------------------*/


#include "bbs.h"
#include <sys/ipc.h>
#include <sys/shm.h>


#ifdef	HAVE_SEM
#include <sys/sem.h>
#endif


#ifdef MODE_STAT 
UMODELOG modelog; 
time_t mode_lastchange; 
#endif


#ifdef	HAVE_SEM

/* ----------------------------------------------------- */
/* semaphore : for critical section			 */
/* ----------------------------------------------------- */


static int ap_semid;


static void
attach_err(shmkey)
  int shmkey;
{
  char buf[20];

  sprintf(buf, "key = %x", shmkey);
  blog("shmget", buf);
  exit(1);
}


void
sem_init()
{
  int semid;

  union semun
  {
    int val;
    struct semid_ds *buf;
    ushort *array;
  }     arg =
  {
    1
  };

  semid = semget(BSEM_KEY, 1, 0);
  if (semid == -1)
  {
    semid = semget(BSEM_KEY, 1, IPC_CREAT | BSEM_FLG);
    if (semid == -1)
      attach_err(BSEM_KEY);
    semctl(semid, 0, SETVAL, arg);
  }
  ap_semid = semid;
}


void
sem_lock(op)
  int op;			/* op is BSEM_ENTER or BSEM_LEAVE */
{
  struct sembuf sops;

  sops.sem_num = 0;
  sops.sem_flg = SEM_UNDO;
  sops.sem_op = op;
  semop(ap_semid, &sops, 1);
}


#endif	/* HAVE_SEM */


/*-------------------------------------------------------*/
/* .UTMP cache						 */
/*-------------------------------------------------------*/


UCACHE *ushm;


void
ushm_init()
{
  ushm = shm_new(UTMPSHM_KEY, sizeof(UCACHE));
  if (!(ushm->bbs_start))
    time(&(ushm->bbs_start));
}


void
utmp_mode(mode)
  int mode;
{
  if (bbsmode != mode)
  {
#ifdef MODE_STAT
    time_t now;

    time(&now);
    modelog.used_time[bbsmode] += (now - mode_lastchange);
    mode_lastchange = now;
#endif

    cutmp->mode = bbsmode = mode;
  }
}


int
utmp_new(up)
  UTMP *up;
{
  UCACHE *xshm;
  UTMP *uentp, *utail;

  /* --------------------------------------------------- */
  /* semaphore : critical section			 */
  /* --------------------------------------------------- */

#ifdef	HAVE_SEM
  sem_lock(BSEM_ENTER);
#endif

  xshm = ushm;
  uentp = xshm->uslot;
  utail = uentp + MAXACTIVE;

  /* uentp += (up->pid % xshm->count); */	/* hashing */

  do
  {
    if (!uentp->pid)
    {
      usint offset;

      offset = (void *) uentp - (void *) xshm->uslot;
      memcpy(uentp, up, sizeof(UTMP));
      xshm->count++;
      if (xshm->offset < offset)
	xshm->offset = offset;
      cutmp = uentp;

#ifdef	HAVE_SEM
      sem_lock(BSEM_LEAVE);
#endif

      return 1;
    }
  } while (++uentp < utail);

  /* Thor:?i?Duser???H?n???@?B?F */

#ifdef	HAVE_SEM
  sem_lock(BSEM_LEAVE);
#endif

  return 0;
}


void
utmp_free(up)
  UTMP *up;
{
  if (!up || !up->pid)
    return;

#ifdef	HAVE_SEM
  sem_lock(BSEM_ENTER);
#endif

  up->pid = up->userno = 0;
  ushm->count--;

#ifdef	HAVE_SEM
  sem_lock(BSEM_LEAVE);
#endif
}


UTMP *
utmp_find(userno)
  int userno;
{
  UTMP *uentp, *uceil;

  uentp = ushm->uslot;
  uceil = (void *) uentp + ushm->offset;
  do
  {
    if (uentp->userno == userno)
      return uentp;
  } while (++uentp <= uceil);

  return NULL;
}


UTMP *
utmp_get(userno, userid)	/* itoc.010306: ???d???????O?_?b???W */
  int userno;
  char *userid;
{
  UTMP *uentp, *uceil;
  int seecloak;
#ifdef HAVE_SUPERCLOAK
  int seesupercloak;
#endif

  /* itoc.020718.????: ?????P?@?????P???@???????v???b?????A???{?O?_?O?U?d?????G?A
     ?M?????d???e???O???A?Y???????A?h???????W???? */

  seecloak = HAS_PERM(PERM_SEECLOAK);
#ifdef HAVE_SUPERCLOAK
  seesupercloak = cuser.ufo & UFO_SUPERCLOAK;
#endif
  uentp = ushm->uslot;
  uceil = (void *) uentp + ushm->offset;
  do
  {
    if (uentp->pid && 		/* ?w?g???????????d */
      ((userno && uentp->userno == userno) || (userid && !strcmp(userid, uentp->userid))))
    {
      if (!seecloak && (uentp->ufo & UFO_CLOAK))	/* ?????????? */
	continue;

#ifdef HAVE_SUPERCLOAK
      if (!seesupercloak && (uentp->ufo & UFO_SUPERCLOAK))	/* ?????????? */
	continue;
#endif

#ifdef HAVE_BADPAL
      if (!seecloak && is_obad(uentp))		/* ?Q?]?a?H?A?s?O?? multi-login ?]?????? */
	break;
#endif

      return uentp;
    }
  } while (++uentp <= uceil);

  return NULL;
}


UTMP *
utmp_seek(hdr)		/* itoc.010306: ???d???????O?_?b???W */
  HDR *hdr;
{
  if (hdr->xmode & POST_INCOME)	/* POST_INCOME ?M MAIL_INCOME ?O???P?? */
    return NULL;
  return utmp_get(0, hdr->owner);
}


void  
utmp_admset(userno, status)	/* itoc.010811: ???A?]?w?u?W?????? */
  int userno;
  usint status;
{
  UTMP *uentp, *uceil;
  extern int ulist_userno[];

  uentp = ushm->uslot;
  uceil = (void *) uentp + ushm->offset;
  do
  {
    if (uentp->userno == userno)
      uentp->status |= status;	/* ?[?W?????Q?????L???X?? */

    /* itoc.041211: ???????????s?W/????/?????B?????A
       ???F?n???L?[?W STATUS_PALDIRTY (???Y?s?L???s?P?_?L???v?? ulist_ftype[????])?A
       ???n???? ulist_userno[????] ???? 0 (???Y?s?????v???s ulist_ftype[????]) */
    if (status == STATUS_PALDIRTY)
      ulist_userno[uentp - ushm->uslot] = 0;
  } while (++uentp <= uceil);
}


int
utmp_count(userno, show)
  int userno;
  int show;
{
  UTMP *uentp, *uceil;
  int count;

  count = 0;
  uentp = ushm->uslot;
  uceil = (void *) uentp + ushm->offset;
  do
  {
    if (uentp->userno == userno)
    {
      count++;
      if (show)
      {
	prints("(%d) ???e???A??: %-17.16s(???? %s)\n",
	  count, bmode(uentp, 0), uentp->from);
      }
    }
  } while (++uentp <= uceil);
  return count;
}


UTMP *
utmp_search(userno, order)
  int userno;
  int order;			/* ???X?? */
{
  UTMP *uentp, *uceil;

  uentp = ushm->uslot;
  uceil = (void *) uentp + ushm->offset;
  do
  {
    if (uentp->userno == userno)
    {
      if (--order <= 0)
	return uentp;
    }
  } while (++uentp <= uceil);
  return NULL;
}


#if 0
int
apply_ulist(fptr)
  int (*fptr) ();
{
  UTMP *uentp;
  int i, state;

  uentp = ushm->uslot;
  for (i = 0; i < USHM_SIZE; i++, uentp++)
  {
    if (uentp->pid)
      if (state = (*fptr) (uentp))
	return state;
  }
  return 0;
}
#endif


/*-------------------------------------------------------*/
/* .BRD cache						 */
/*-------------------------------------------------------*/


BCACHE *bshm;


void
bshm_init()
{
  int i;

  /* itoc.030727: ?b?}?? bbsd ???e?A?????N?n?????L account?A
     ???H bshm ?????w?]?w?n */

  bshm = shm_new(BRDSHM_KEY, sizeof(BCACHE));

  i = 0;
  while (bshm->uptime <= 0)	/* bshm ???]?w?????A?]?\?O???] account?A?]?\?O???????n?b?}?O */
  {
    sleep(5);
    if (++i >= 6)		/* ?Y 30 ???H?????S?n?A?_?u???} */
      abort_bbs();
  }
}


void
bshm_reload()		/* ?}?O?H???A???s???J bshm */
{
  time_t *uptime;
  int fd;
  BRD *head, *tail;

  uptime = &(bshm->uptime);

  while (*uptime <= 0)
  {
    /* ???L?????]???n?b?}?O?A???? 30 ?? */
    sleep(30);
  }

  *uptime = -1;		/* ?}?l?]?w */

  if ((fd = open(FN_BRD, O_RDONLY)) >= 0)
  {
    bshm->number = read(fd, bshm->bcache, MAXBOARD * sizeof(BRD)) / sizeof(BRD);
    close(fd);
  }

  /* ?????? boards ???????s???A?]?w uptime */
  time(uptime);

  /* itoc.040314: ?O?D???????O???z???O???????????O???~?|?? bpost/blast ?g?i .BRD ??
     ???H .BRD ???? bpost/blast ?????O?????A?n???s initial?C
     initial ?????k?O?N btime ?]?? -1?A?? class_item() ?h???s */
  head = bshm->bcache;
  tail = head + bshm->number;
  do
  {
    head->btime = -1;
  } while (++head < tail);

  blog("CACHE", "reload bcache");
}


#if 0
int
apply_boards(func)
  int (*func) ();
{
  extern char brd_bits[];
  BRD *bhdr;
  int i;

  for (i = 0, bhdr = bshm->bcache; i < bshm->number; i++, bhdr++)
  {
    if (brd_bits[i])
    {
      if ((*func) (bhdr) == -1)
	return -1;
    }
  }
  return 0;
}
#endif


static int
brdname_cmp(a, b)
  BRD *a, *b;
{
  return str_cmp(a->brdname, b->brdname);
}


int
brd_bno(bname)
  char *bname;
{
  BRD xbrd, *bcache, *brdp, *bend;

  bcache = bshm->bcache;

  /* ???b?????O binary serach */

  /* str_ncpy(xbrd.brdname, bname, sizeof(xbrd.brdname)); */
  str_lower(xbrd.brdname, bname);	/* ???????p?g?A?o???b brdname_cmp() ???|???@?? */
  if (bend = bsearch(&xbrd, bcache, bshm->numberOld, sizeof(BRD), brdname_cmp))
    return bend - bcache;

  /* ?Y???????A?A?h?s???O sequential search */

  brdp = bcache + bshm->numberOld;
  bend = bcache + bshm->number;

  while (brdp < bend)
  {
    if (!str_cmp(bname, brdp->brdname))
      return brdp - bcache;

    brdp++;
  }

  return -1;
}


/*-------------------------------------------------------*/
/* movie cache						 */
/*-------------------------------------------------------*/


FCACHE *fshm;


void
fshm_init()
{
  fshm = shm_new(FILMSHM_KEY, sizeof(FCACHE));
}


/* ----------------------------------------------------- */
/* itoc.020822.????:					 */
/* ----------------------------------------------------- */
/* ?? 0 ?? FILM_MOVIE-1 ?i?O?t???e?????????e??		 */
/* ?? FILM_MOVIE ?? fmax-1 ?i?O???A???O			 */
/* ----------------------------------------------------- */
/* tag:							 */
/* < FILM_MOVIE  ?? ???????i?e??			 */
/* >= FILM_MOVIE ?? ???????? FILM_MOVIE~fmax-1 ?????@?i	 */
/* ----------------------------------------------------- */
/* row:							 */
/* >=0 ?? ?t???e???A?q (row, 0) ?}?l?L			 */
/* <0  ?? ?????e???A?q (0, 0) ?}?l?L?A?????| vmsg(NULL)	 */
/* ----------------------------------------------------- */


void
film_out(tag, row)
  int tag;
  int row;			/* -1 : help */
{
  int fmax, len, *shot;
  char *film, buf[FILM_SIZ];

  len = 0;
  shot = fshm->shot;

  while (!(fmax = *shot))	/* util/camera.c ???b???? */
  {
    sleep(5);
    if (++len >= 6)		/* ?Y 30 ???H?????S???n???A?i???O?S?] camera?A???????} */
      return;
  }

  if (row <= 0)
    clear();
  else
    move(row, 0);

  if (tag >= FILM_MOVIE)	/* ???A???O */
    tag += time(0) % (fmax - FILM_MOVIE);

  film = fshm->film;

  if (tag)
  {
    len = shot[tag];
    film += len;
    len = shot[tag + 1] - len;
  }
  else
  {
    len = shot[1];
  }

  if (len >= FILM_SIZ - MOVIE_LINES)
    return;

  memcpy(buf, film, len);
  buf[len] = '\0';

  if (d_cols)	/* waynesan.040831: ???e?????m?? */
  {
    char *ptr;
    for (film = buf; *film;)
    {
      if (ptr = strchr(film, '\n'))
	*ptr = '\0';
      move(row++, (d_cols >> 1));
      outx(film);
      if (ptr)
	film = ptr + 1;
    }
  }
  else
    outx(buf);

  if (row < 0)			/* help screen */
    vmsg(NULL);

  return;
}
