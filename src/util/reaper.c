/*-------------------------------------------------------*/
/* util/reaper.c	( NTHU CS MapleBBS Ver 3.00 )	 */
/*-------------------------------------------------------*/
/* target : �ϥΪ̱b���w���M�z				 */
/* create : 95/03/29				 	 */
/* update : 97/03/29				 	 */
/*-------------------------------------------------------*/
/* syntax : reaper					 */
/*-------------------------------------------------------*/
/* notice : ~bbs/usr/@/     - expired users's data	 */
/*          run/reaper      - list of expired users	 */
/*          run/manager     - list of managers		 */
/*          run/lazybm      - list of lazy bm		 */
/*          run/emailaddr   - list of same email addr    */
/*-------------------------------------------------------*/


#include "bbs.h"


/* Thor.980930: �Y�n�����H�U�\��ɥ� undef �Y�i */

#define CHECK_LAZYBM		/* �ˬd���i�O�D */
#define EADDR_GROUPING		/* �ˬd�@�� email */


#ifdef CHECK_LAZYBM
#define DAY_LAZYBM	7	/* 7 �ѥH�W���W�����O�D�Y�O�� */
#endif

#ifdef EADDR_GROUPING
#define EMAIL_REG_LIMIT 3	/* 3 �ӥH�W�ϥΪ̥ΦP�@ email �Y�O�� */
#endif

/* cache.080715: �ثe�]�w�u�� �n�J���W�L�T�����ϥΪ� �� �Q�]�w�M���b�� */ 

/* �O�d�b������ -- �Ǵ��� */
#define DAY_NEWUSR	365	    /* �n�J���W�L10�����ϥΪ̫O�d 365 �� */
#define DAY_FORFUN	1825	/* �����������{�Ҫ��ϥΪ̫O�d 365*5 �� */
#define DAY_OCCUPY	9999	/* �w���������{�Ҫ��ϥΪ̫O�d 9999 �� */

/* �O�d�b������ -- ���� */
#define VAC_NEWUSR	365	    /* �n�J���W�L10�����ϥΪ̫O�d 365 �� */
#define VAC_FORFUN	1825    /* �����������{�Ҫ��ϥΪ̫O�d 365*5 �� */
                            /* �`�N�|���ϥΪ̦b�����]�{�ҹL���Ӧ]���Q��b�� */
#define VAC_OCCUPY	9999	/* �w���������{�Ҫ��ϥΪ̫O�d 9999 �� */


static time_t due_newusr;
static time_t due_forfun;
static time_t due_occupy;


static int visit;
static int prune;
static int manager;
static int invalid;


static FILE *flog;
static FILE *flst;

#ifdef CHECK_LAZYBM
static time_t due_lazybm;
static int lazybm;
static FILE *fbm;
#endif

#ifdef EADDR_GROUPING
static FILE *faddr;
#endif

/* ----------------------------------------------------- */
/* ----------------------------------------------------- */


static int funo;
static int max_uno;
static SCHEMA schema;


static void
userno_free(uno)
  int uno;
{
  off_t off;
  int fd;

  fd = funo;

  /* flock(fd, LOCK_EX); */
  /* Thor.981205: �� fcntl ���Nflock, POSIX�зǥΪk */
  f_exlock(fd);

  time(&schema.uptime);
  off = (uno - 1) * sizeof(schema);
  if (lseek(fd, off, SEEK_SET) < 0)
    exit(2);
  if (write(fd, &schema, sizeof(schema)) != sizeof(schema))
    exit(2);

  /* flock(fd, LOCK_UN); */
  /* Thor.981205: �� fcntl ���Nflock, POSIX�зǥΪk */
  f_unlock(fd);
}


/* ----------------------------------------------------- */
/* ----------------------------------------------------- */


static void
levelmsg(str, level)
  char *str;
  int level;
{
  static char perm[] = "bctpjm#x--------PTC---L*B#-RACBS";
  int len = 32;
  char *p = perm;

  do
  {
    *str = (level & 1) ? *p : '-';
    p++;
    str++;
    level >>= 1;
  } while (--len);
  *str = '\0';
}


static void
datemsg(str, chrono)
  char *str;
  time_t *chrono;
{
  struct tm *t;

  t = localtime(chrono);
  /* Thor.990329: y2k */
  sprintf(str, "%2d/%02d/%02d%3d:%02d:%02d ",
    t->tm_year % 100, t->tm_mon + 1, t->tm_mday,
    t->tm_hour, t->tm_min, t->tm_sec);
}


/* ----------------------------------------------------- */
/* ----------------------------------------------------- */

#ifdef EADDR_GROUPING
/*
 * Thor.980930: �N�P�@email addr��account, �����_�ӨæC�� �u�@��z: 1.
 * _hash() �N email addr �ƭȤ� 2.��binary search, ���happend userno,
 * �䤣��h insert �sentry 3.�N userno list ��z�C�X
 * 
 * ��Ƶ��c: chain: int hash, int link_userno plist: int next_userno
 * 
 * �Ȯɹw��email addr�`�Ƥ��W�L100000, �Ȯɹw��user�`�Ƥ��W�L 100000
 * 
 */

typedef struct
{
  int hash;
  int link;
}      Chain;

static Chain *chain;
static int *plist;
static int numC;

static void
eaddr_group(userno, eaddr)
  int userno;
  char *eaddr;
{
  int left, right, mid, i;
  int hash = str_hash(eaddr, 0);

  left = 0;
  right = numC - 1;
  for (;;)
  {
    int cmp;
    Chain *cptr;

    if (left > right)		/* Thor.980930: ��S */
    {
      for (i = numC; i > left; i--)
	chain[i] = chain[i - 1];

      cptr = &chain[left];
      cptr->hash = hash;
      cptr->link = userno;
      plist[userno] = 0;	/* Thor: tail */
      numC++;
      break;
    }

    mid = (left + right) >> 1;
    cptr = &chain[mid];
    cmp = hash - cptr->hash;

    if (!cmp)
    {
      plist[userno] = cptr->link;
      cptr->link = userno;
      break;
    }

    if (cmp < 0)
      right = mid - 1;
    else
      left = mid + 1;
  }
}

static void
report_eaddr_group()
{
  int i, j, cnt;
  off_t off;
  int fd;

  SCHEMA s;
  /* funo */

  fprintf(faddr, "Email registration over %d times list\n\n", EMAIL_REG_LIMIT);

  for (i = 0; i < numC; i++)
  {
    for (cnt = 0, j = chain[i].link; j; cnt++, j = plist[j]);
    if (cnt > EMAIL_REG_LIMIT)
    {
      fprintf(faddr, "\n> %d\n", chain[i].hash);
      for (j = chain[i].link; j; j = plist[j])
      {
	off = (j - 1) * sizeof(schema);
	if (lseek(funo, off, SEEK_SET) < 0)
	{
	  fprintf(faddr, "==> %d) can't lseek\n", j);
	  continue;
	}
	if (read(funo, &s, sizeof(schema)) != sizeof(schema))
	{
	  fprintf(faddr, "==> %d) can't read\n", j);
	  continue;
	}
	else
	{
	  ACCT acct;
	  char buf[256];
	  if (s.userid[0] <= ' ')
	  {
	    fprintf(faddr, "==> %d) has been reapered\n", j);
	    continue;
	  }
	  usr_fpath(buf, s.userid, FN_ACCT);
	  fd = open(buf, O_RDONLY, 0);
	  if (fd < 0)
	  {
	    fprintf(faddr, "==> %d)%-13s can't open\n", j, s.userid);
	    continue;
	  }
	  if (read(fd, &acct, sizeof(acct)) != sizeof(acct))
	  {
	    fprintf(faddr, "==> %d)%-13s can't read\n", j, s.userid);
	    continue;
	  }
	  close(fd);

	  datemsg(buf, &acct.lastlogin);
	  fprintf(faddr, "%5d) %-13s%s[%d]\t%s\n", acct.userno, acct.userid, buf, acct.numlogins, acct.email);
	}
      }
    }
  }
}
#endif

static void
reaper(fpath, lowid)
  char *fpath;
  char *lowid;
{
  int fd, login;
  usint ulevel;
  time_t life;
  char buf[256], data[40];
  ACCT acct;

  sprintf(buf, "%s/" FN_ACCT, fpath);
  fd = open(buf, O_RDONLY, 0);
  if (fd < 0)
  {				/* Thor.981001: �[�� log */
    fprintf(flog, "acct can't open %-13s ==> %s\n", lowid, buf);
    return;
  }

  if (read(fd, &acct, sizeof(acct)) != sizeof(acct))
  {
    fprintf(flog, "acct can't read %-13s ==> %s\n", lowid, buf);
    close(fd);
    return;
  }
  close(fd);

  fd = acct.userno;

  if ((fd <= 0) || (fd > max_uno))
  {
    fprintf(flog, "%5d) %-13s ==> %s\n", fd, acct.userid, buf);
    return;
  }

  ulevel = acct.userlevel;
  life = acct.lastlogin;
  login = acct.numlogins;

#ifdef EADDR_GROUPING
  if (ulevel & PERM_VALID)	/* Thor.980930: �u�ݳq�L�{�Ҫ� email, ���� */
    eaddr_group(fd, acct.email);
#endif

  if (ulevel & (PERM_XEMPT | PERM_BM | PERM_ALLADMIN))
  {
    datemsg(buf, &acct.lastlogin);
    levelmsg(data, ulevel);
    fprintf(flst, "%5d) %-13s%s[%s] %d\n", fd, acct.userid, buf, data, login);
    manager++;

#ifdef CHECK_LAZYBM
    if ((ulevel & PERM_BM) && life < due_lazybm)
    {
      fprintf(fbm, "%5d) %-13s%s %d\n", fd, acct.userid, buf, login);
      lazybm++;
    }
#endif

  }
  else if (ulevel)		/* guest.ulevel == 0, �û��O�d */
  {
    if (login <= 10 && life < due_newusr) //�n�J10���H���S�Onewuser�|�� 
      life = 0;

    if (ulevel & PERM_PURGE)	/* lkchu.990221: �u�M���b���v */
    {
      life = 0;
    }
    else if (ulevel & PERM_DENYLOGIN)
    /* itoc.010927: ���u�T��W���v������A���Y���u�M���b���v�v���Ӭ� */
    {
      /* life = 1; */	/* ���ݭn�A�e�����F */
    }
//    else if (ulevel & PERM_VALID)
//    {
//      if (life < due_occupy)
//	life = 0;
//    }
    else
    {
      if (login <= 50 && life < due_forfun) //�n�J50���H���S�Oforfunuser�|��
	life = 0;
      else
	invalid++;
    }

    if (!life)
    {
      sprintf(buf, "usr/@/%s", lowid);

      while (rename(fpath, buf))
      {
	extern int errno;

	fprintf(flog, "rename %s ==> %s : %d\n", fpath, buf, errno);
	sprintf(buf, "usr/@/%s.%d", lowid, ++life);
      }

      userno_free(fd);
      datemsg(buf, &acct.lastlogin);
      fprintf(flog, "%5d) %-13s%s%d\n", fd, acct.userid, buf, login);
      prune++;
    }

#ifdef HAVE_MBOX_GEM
    else if (!(ulevel & PERM_MBOX))
    {				/* �M���ϥΪ̥ؿ��U�� gem �ؿ� */
      DIR *dirp;
      char fph[128], command[128];

      usr_fpath(fpath, acct.userid, "gem");
      if ((dirp = opendir(fph)))
      {
	closedir(dirp);
	sprintf(command, "rm -rf %s", fph);
	system(command);
      }
    }
#endif
  }

  visit++;
}


static void
traverse(fpath)
  char *fpath;
{
  DIR *dirp;
  struct dirent *de;
  char *fname, *str;

  /* visit the second hierarchy */

  if (!(dirp = opendir(fpath)))
  {
    fprintf(flog, "## unable to enter hierarchy [%s]\n", fpath);
    return;
  }

  for (str = fpath; *str; str++);
  *str++ = '/';

  while (de = readdir(dirp))
  {
    fname = de->d_name;
    if (fname[0] > ' ' && fname[0] != '.')
    {
      strcpy(str, fname);
      reaper(fpath, fname);
    }
  }
  closedir(dirp);
}


int
main()
{
  int ch;
  time_t start, end;
  struct tm *ptime;
  struct stat st;
  char *fname, fpath[256];

  setuid(BBSUID);
  setgid(BBSGID);
  chdir(BBSHOME);

  flog = fopen(FN_RUN_REAPER, "w");
  if (flog == NULL)
    exit(1);

  flst = fopen(FN_RUN_MANAGER, "w");
  if (flst == NULL)
    exit(1);

#ifdef CHECK_LAZYBM
  fbm = fopen(FN_RUN_LAZYBM, "w");
  if (fbm == NULL)
    exit(1);
#endif

#ifdef EADDR_GROUPING
  faddr = fopen(FN_RUN_EMAILADDR, "w");
  if (faddr == NULL)
    exit(1);
#endif

#ifdef EADDR_GROUPING
  funo = open(".USR", O_RDWR | O_CREAT, 0600);	/* Thor.980930: for read name */
#else
  funo = open(".USR", O_WRONLY | O_CREAT, 0600);
#endif

  if (funo < 0)
    exit(1);

  /* ���]�M���b�������A�s���U�H�Ƥ��|�W�L 300 �H */

  fstat(funo, &st);
  max_uno = st.st_size / sizeof(SCHEMA) + 300;

  time(&start);
  ptime = localtime(&start);

  /* itoc.011002.����: ����b�@�}�ǴN���W apply �Y�檺�ɶ�����A
     �_�h�ܦh user �|�]����Ӵ����S���W���A�b�@�}�ǴN�Q reaper �� */   

  if ((ptime->tm_mon >= 6 && ptime->tm_mon <= 8) ||	/* 7 ��� 9 ��O���� */
    (ptime->tm_mon >= 1 && ptime->tm_mon <= 2))		/* 2 ��� 3 ��O�H�� */
  {
    due_newusr = start - VAC_NEWUSR * 86400;
    due_forfun = start - VAC_FORFUN * 86400;
    due_occupy = start - VAC_OCCUPY * 86400;
  }
  else
  {
    due_newusr = start - DAY_NEWUSR * 86400;
    due_forfun = start - DAY_FORFUN * 86400;
    due_occupy = start - DAY_OCCUPY * 86400;
  }

/* itoc.011124: ����b��: �w�{�ҤH�� acct.lastlogin �� >= 0 */
  due_occupy = 0;

#ifdef CHECK_LAZYBM
  due_lazybm = start - DAY_LAZYBM * 86400;
#endif

#ifdef EADDR_GROUPING
  chain = (Chain *) malloc(max_uno * sizeof(Chain));
  plist = (int *)malloc((max_uno + 1) * sizeof(int));
  if (!chain || !plist)
  {
    fprintf(faddr, "out of memory....\n");
    exit(1);
  }
#endif

  strcpy(fname = fpath, "usr/@");
  mkdir(fname, 0700);
  fname = (char *)strchr(fname, '@');

  /* visit the first hierarchy */

  for (ch = 'a'; ch <= 'z'; ch++)
  {
    fname[0] = ch;
    fname[1] = '\0';
    traverse(fpath);
  }

#ifdef EADDR_GROUPING
  report_eaddr_group();		/* Thor.980930: before close funo */
#endif

  close(funo);

  fprintf(flst, "\nManager: %d\n", manager);
  fclose(flst);

  time(&end);
  fprintf(flog, "\n\n# start: %s", ctime(&start));
  fprintf(flog, "\n# end  : %s", ctime(&end));
  end -= start;
  start = end % 60;
  end /= 60;
  fprintf(flog, "# time : %d:%d:%d\n", end / 60, end % 60, start);
  fprintf(flog, "# Visit: %d\n", visit);
  fprintf(flog, "# Prune: %d\n", prune);
  fprintf(flog, "# Valid: %d\n", invalid);
  fclose(flog);

#ifdef CHECK_LAZYBM
  fprintf(fbm, "\nLazy BM for %d days: %d\n", DAY_LAZYBM, lazybm);
  fclose(fbm);
#endif

#ifdef EADDR_GROUPING
  free(chain);
  free(plist);
  fclose(faddr);
#endif

  exit(0);
}