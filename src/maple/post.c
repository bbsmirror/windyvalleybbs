/*-------------------------------------------------------*/
/* post.c       ( NTHU CS MapleBBS Ver 2.39 )		 */
/*-------------------------------------------------------*/
/* target : bulletin boards' routines		 	 */
/* create : 95/03/29				 	 */
/* update : 96/04/05				 	 */
/*-------------------------------------------------------*/


#include "bbs.h"


extern BCACHE *bshm;
extern XZ xz[];


extern int wordsnum;		/* itoc.010408: �p��峹�r�� */
extern int TagNum;
extern char xo_pool[];
extern char brd_bits[];


#ifdef HAVE_ANONYMOUS
extern char anonymousid[];	/* itoc.010717: �۩w�ΦW ID */
#endif


int
cmpchrono(hdr)
  HDR *hdr;
{
  return hdr->chrono == currchrono;
}


/* 090127.cache: �[�K�峹�i���W�� */

static void
RefusePal_fpath(fpath, board, mode, hdr)
  char *fpath;
  char *board;
  char mode;       /* 'C':Cansee  'R':Refimage */
  HDR *hdr;
{
  sprintf(fpath, "brd/%s/RefusePal_DIR/%s_%s",
    board, mode == 'C' ? "Cansee" : "Refimage", hdr->xname);
}

void
RefusePal_kill(board, hdr)   /* amaki.030322: �ΨӬ�W��p�� */
  char *board;
  HDR *hdr;
{
  char fpath[64];

  RefusePal_fpath(fpath, board, 'C', hdr);
  unlink(fpath);
  RefusePal_fpath(fpath, board, 'R', hdr);
  unlink(fpath);
}

int          /* 1:�b�i���W��W 0:���b�i���W��W */
RefusePal_belong(board, hdr)
  char *board;
  HDR *hdr;
{
  int fsize;
  char fpath[64];
  int *fimage;

  RefusePal_fpath(fpath, board, 'R', hdr);
  if (fimage =(int *)f_img(fpath, &fsize))  /*�ץ��ǻ���ƫ��A���Ū�warning*/
  {
    fsize = belong_pal(fimage, fsize / sizeof(int), cuser.userno);
    free(fimage);
    return fsize;
  }
  return 0;
}

static void
refusepal_cache(hdr, board)
  HDR *hdr;
  char *board;
{
  int fd, max;
  char fpath[64];
  int pool[PAL_MAX];

  RefusePal_fpath(fpath, board, 'C', hdr);

  if (max = image_pal(fpath, pool))
  {
    RefusePal_fpath(fpath, board, 'R', hdr);
    if ((fd = open(fpath, O_WRONLY | O_CREAT | O_TRUNC, 0600)) >= 0)
    {
      write(fd, pool, max * sizeof(int));
      close(fd);
    }
  }
  else{
    RefusePal_kill(board, hdr);
  }
}
/* FinFunnel.20110219: �쥻�����qxo_pool��hdr���覡�A�C�����Ĥ@�g�峹hdr���e
   �g�Lxover����G�|�Q�ʨ�ɭP�䤣��峹�s���A�Ȯɱĥνƻs�@��hdr���覡 */
static int
XoBM_Refuse_pal(xo)
  XO *xo;
{
  char fpath[64];
  HDR *hdr, hdr_tmp;
  XO *xt;

  hdr = (HDR *) xo_pool + (xo->pos - xo->top);
  memcpy(&hdr_tmp, hdr, sizeof(hdr_tmp));
  hdr = &hdr_tmp;

  if (!(hdr->xmode & POST_RESTRICT))
    return XO_NONE;

  if (strcmp(hdr->owner, cuser.userid) && !(bbstate & STAT_BM))
    return XO_NONE;

  sprintf(fpath, "brd/%s/RefusePal_DIR", currboard);
  if (!dashd(fpath))
    mkdir(fpath, 0700);

  RefusePal_fpath(fpath, currboard, 'C', hdr);
  xz[XZ_PAL - XO_ZONE].xo = xt = xo_new(fpath);
  xt->key=PALTYPE_PAL;
  xover(XZ_PAL);
  refusepal_cache(hdr, currboard);
  free(xt);
  return XO_INIT;
}

//�[�K�峹�i���W��

static void
change_stamp(folder, hdr)
  char *folder;
  HDR *hdr;
{
  HDR buf;

  /* ���F�T�w�s�y�X�Ӫ� stamp �]�O unique (���M�J���� chrono ����)�A
     �N���ͤ@�ӷs���ɮסA���ɮ��H�K link �Y�i�C
     �o�Ӧh���ͥX�Ӫ��U���|�b expire �Q sync �� (�]�����b .DIR ��) */
  hdr_stamp(folder, HDR_LINK | 'A', &buf, "etc/stamp");
  hdr->stamp = buf.chrono;
}

/* ----------------------------------------------------- */
/* ��} innbbsd ��X�H��B�s�u��H���B�z�{��		 */
/* ----------------------------------------------------- */


void
btime_update(bno)
  int bno;
{
  if (bno >= 0)
    (bshm->bcache + bno)->btime = -1;	/* �� class_item() ��s�� */
}


#ifndef HAVE_NETTOOL
static 			/* �� enews.c �� */
#endif
void
outgo_post(hdr, board)
  HDR *hdr;
  char *board;
{
  bntp_t bntp;

  memset(&bntp, 0, sizeof(bntp_t));

  if (board)		/* �s�H */
  {
    bntp.chrono = hdr->chrono;
  }
  else			/* cancel */
  {
    bntp.chrono = -1;
    board = currboard;
  }
  strcpy(bntp.board, board);
  strcpy(bntp.xname, hdr->xname);
  strcpy(bntp.owner, hdr->owner);
  strcpy(bntp.nick, hdr->nick);
  strcpy(bntp.title, hdr->title);
  rec_add("innd/out.bntp", &bntp, sizeof(bntp_t));
}


void
cancel_post(hdr)
  HDR *hdr;
{
  if ((hdr->xmode & POST_OUTGO) &&		/* �~��H�� */
    (hdr->chrono > ap_start - 7 * 86400))	/* 7 �Ѥ������� */
  {
    outgo_post(hdr, NULL);
  }
}


static inline int		/* �^�Ǥ峹 size �h���� */
move_post(hdr, folder, by_bm, rangedel)	/* �N hdr �q folder �h��O���O */
  HDR *hdr;
  char *folder;
  int by_bm;
  int rangedel;
{
  HDR post;
  int xmode;
  char fpath[64], fnew[64], *board;
  struct stat st;

  xmode = hdr->xmode;
  hdr_fpath(fpath, folder, hdr);

  if (!(xmode & POST_BOTTOM) || !strcmp(currboard, BN_RANGEDELETED))
  /* �m�����BN_RANGEDELETED�����峹�Q�夣�� move_post */
  {
   if (by_bm)
   {
#ifdef HAVE_REFUSEMARK
     if (xmode & POST_RESTRICT)
       board = BN_RANGEDELETED;
     else
#endif
     if (rangedel)
       board = BN_RANGEDELETED;
     else
       board = BN_DELETED;
   }
   else
   {
     board = BN_RANGEDELETED;
   }

    brd_fpath(fnew, board, fn_dir);
    hdr_stamp(fnew, HDR_LINK | 'A', &post, fpath);

    /* �����ƻs trailing data�Gowner(�t)�H�U�Ҧ���� */

    memcpy(post.owner, hdr->owner, sizeof(HDR) -
      (sizeof(post.chrono) + sizeof(post.xmode) + sizeof(post.xid) + sizeof(post.xname)));

    if (by_bm)
      sprintf(post.title, "%-13s%.59s", cuser.userid, hdr->title);

    rec_bot(fnew, &post, sizeof(HDR));
    btime_update(brd_bno(board));
  }

  by_bm = stat(fpath, &st) ? 0 : st.st_size;

  unlink(fpath);
  btime_update(currbno);
  cancel_post(hdr);

  return by_bm;
}


#ifdef HAVE_DETECT_CROSSPOST
/* ----------------------------------------------------- */
/* ��} cross post ���v					 */
/* ----------------------------------------------------- */


#define MAX_CHECKSUM_POST	20	/* �O���̪� 20 �g�峹�� checksum */
#define MAX_CHECKSUM_LINE	6	/* �u���峹�e 6 ��Ӻ� checksum */


typedef struct
{
  int sum;			/* �峹�� checksum */
  int total;			/* ���峹�w�o��X�g */
}      CHECKSUM;


static CHECKSUM checksum[MAX_CHECKSUM_POST];
static int checknum = 0;


static inline int
checksum_add(str)		/* �^�ǥ��C��r�� checksum */
  char *str;
{
  int i, len, sum;

  len = strlen(str);

  sum = len;	/* ��r�ƤӤ֮ɡA�e�|�����@�ܥi�৹���ۦP�A�ҥH�N�r�Ƥ]�[�J sum �� */
  for (i = len >> 2; i > 0; i--)	/* �u��e�|�����@�r���� sum �� */
    sum += *str++;

  return sum;
}


static inline int		/* 1:�Ocross-post 0:���Ocross-post */
checksum_put(sum)
  int sum;
{
  int i;

  if (sum)
  {
    for (i = 0; i < MAX_CHECKSUM_POST; i++)
    {
      if (checksum[i].sum == sum)
      {
	checksum[i].total++;

	if (checksum[i].total > MAX_CROSS_POST)
	  return 1;
	return 0;	/* total <= MAX_CROSS_POST */
      }
    }

    if (++checknum >= MAX_CHECKSUM_POST)
      checknum = 0;
    checksum[checknum].sum = sum;
    checksum[checknum].total = 1;
  }
  return 0;
}


static int			/* 1:�Ocross-post 0:���Ocross-post */
checksum_find(fpath)
  char *fpath;
{
  int i, sum;
  char buf[ANSILINELEN];
  FILE *fp;

  sum = 0;
  if (fp = fopen(fpath, "r"))
  {
    for (i = -(LINE_HEADER + 1);;)	/* �e�X�C�O���Y */
    {
      if (!fgets(buf, ANSILINELEN, fp))
	break;

      if (i < 0)	/* ���L���Y */
      {
	i++;
	continue;
      }

      if (*buf == QUOTE_CHAR1 || *buf == '\n' || !strncmp(buf, "��", 2))	 /* ���L�ި� */
	continue;

      sum += checksum_add(buf);
      if (++i >= MAX_CHECKSUM_LINE)
	break;
    }

    fclose(fp);
  }

  return checksum_put(sum);
}


static int
check_crosspost(fpath, bno)
  char *fpath;
  int bno;			/* �n��h���ݪO */
{
  char *blist, folder[64];
  ACCT acct;
  HDR hdr;

  if (HAS_PERM(PERM_ALLADMIN))
    return 0;

  /* �O�D�b�ۤv�޲z���ݪO���C�J��K�ˬd */
  blist = (bshm->bcache + bno)->BM;
  if (HAS_PERM(PERM_BM) && blist[0] > ' ' && is_bm(blist, cuser.userid))
    return 0;

  if (checksum_find(fpath))
  {
    /* �p�G�O cross-post�A������h BN_SECURITY �ê������v */
    brd_fpath(folder, BN_SECURITY, fn_dir);
    hdr_stamp(folder, HDR_COPY | 'A', &hdr, fpath);
    strcpy(hdr.owner, cuser.userid);
    strcpy(hdr.nick, cuser.username);
    sprintf(hdr.title, "%s %s Cross-Post", cuser.userid, Now());
    rec_bot(folder, &hdr, sizeof(HDR));
    btime_update(brd_bno(BN_SECURITY));

    bbstate &= ~STAT_POST;
    cuser.userlevel &= ~PERM_POST;
    cuser.userlevel |= PERM_DENYPOST;
    if (acct_load(&acct, cuser.userid) >= 0)
    {
      acct.tvalid = time(NULL) + CROSSPOST_DENY_DAY * 86400;
      acct_setperm(&acct, PERM_DENYPOST, PERM_POST);
    }
    board_main();
    mail_self(FN_ETC_CROSSPOST, str_sysop, "Cross-Post ���v", 0);
    vmsg("�z�]���L�� Cross-Post �w�Q���v");
    return 1;
  }
  return 0;
}
#endif	/* HAVE_DETECT_CROSSPOST */


/* ----------------------------------------------------- */
/* �o��B�^���B�s��B����峹				 */
/* ----------------------------------------------------- */

int
is_author(hdr)
  HDR *hdr;
{
  /* �o�̨S���ˬd�O���O guest�A�`�N�ϥΦ��禡�ɭn�S�O�Ҽ{ guest ���p */

  /* itoc.070426: ��b���Q�M����A�s���U�ۦP ID ���b���ä��֦��L�h�� ID �o���峹���Ҧ��v */
  return !strcmp(hdr->owner, cuser.userid) && (hdr->chrono > cuser.firstlogin);
}


#ifdef HAVE_REFUSEMARK
int
chkrestrict(hdr)
  HDR *hdr;
{
//  return !(hdr->xmode & POST_RESTRICT) || is_author(hdr) || (bbstate & STAT_BM);
  return !(hdr->xmode & POST_RESTRICT) || is_author(hdr) || (bbstate & STAT_BM) || RefusePal_belong(currboard, hdr);

}
#endif


#ifdef HAVE_ANONYMOUS
static void
log_anonymous(fname)
  char *fname;
{
  char buf[512];

  sprintf(buf, "%s %-13s(%s)\n%-13s %s %s\n",
    Now(), cuser.userid, fromhost, currboard, fname, ve_title);
  f_cat(FN_RUN_ANONYMOUS, buf);
}
#endif


#ifdef HAVE_UNANONYMOUS_BOARD
static void
do_unanonymous(fpath)
  char *fpath;
{
  HDR hdr;
  char folder[64];

  brd_fpath(folder, BN_UNANONYMOUS, fn_dir);
  hdr_stamp(folder, HDR_LINK | 'A', &hdr, fpath);

  strcpy(hdr.owner, cuser.userid);
  strcpy(hdr.title, ve_title);

  rec_bot(folder, &hdr, sizeof(HDR));
  btime_update(brd_bno(BN_UNANONYMOUS));
}
#endif

/* �A���m�[�J�@ function post_prefix() */

static char *
post_prefix()
{
  char *menu[13], fpath[64];
  PREFIX prefix;
  static char buf[10][14];
  char buf_title[50];
  int ch, i, j, num, page, page_max, page_size;

  if (currbattr & BRD_PREFIX)
  {
    brd_fpath(fpath, currboard, FN_PREFIX);
    if ((num = rec_num(fpath, sizeof(PREFIX))) > 0)
    {
      page_size = 9;
      page_max = ((num - 1) / page_size) + 1;
      page = 0;
      menu[0] = "QP";

      for (;;)
      {
        i = page * page_size;
        for (j = 0; j <= page_size; i++, j++)
        {
          if (!(rec_get(fpath, &prefix, sizeof(PREFIX), i)))
          {
            if (prefix.xmode && (!(bbstate & STAT_BOARD)))
              strcpy(buf[j], "#  �O�D�s��");
            else
              sprintf(buf[j], "%d  %s", (j + 1), prefix.title);

            menu[j + 1] = buf[j];
          }
          else
          {
            j++;
            break;
          }
        }

        menu[j] = "Q  ����";
        menu[j + 1] = NULL;

        sprintf(buf_title, "�п�ܤ峹���O (��)������%c��", page + '1');
        ch = pans(3, 0, buf_title, menu);
        if (ch == 'q')
          break;
        else if (ch == 'p')
        {
          page++;
          if (page == page_max)
            page = 0;
        }
        else if (ch == '#')
          vmsg("�L�k�ϥΦ��峹���O...�Э��s��ܡI");
        else
          return buf[ch - '1'] + 3 ;
      }
    }
  }
  return NULL;
}

/*080805.cache: ALL POST �ƥ� �O�W:AllPost*/
static void
do_allpost(fpath, owner, nick, mode)
  char *fpath;
  char *owner;
  char *nick;
  int mode;
{
  HDR hdr;
  char folder[64];
  char *brdname = "AllPost";       // �O�W�۩w

  brd_fpath(folder, brdname, fn_dir);
  hdr_stamp(folder, HDR_LINK | 'A', &hdr, fpath);

  hdr.xmode = mode & ~POST_OUTGO;  /* ���� POST_OUTGO */
  strcpy(hdr.owner, owner);
  strcpy(hdr.nick, nick);
  strcpy(hdr.title, ve_title);

  rec_bot(folder, &hdr, sizeof(HDR));

  btime_update(brd_bno(brdname));
}

/*080812.cache: ����峹��Y�S�w���O*/

static int
depart_post(xo)
  XO *xo;
{
  char folder[64], fpath[64];
  HDR post, *hdr;
  BRD *brdp, *bend;
  int ans, pos, cur;

  if (!HAS_PERM(PERM_ALLBOARD)/*|| strcmp(currboard, "DepartAll")*/)/* �ȬݪO���� �����w�ݪO */
    {
      vmsg("�z���v�������I");
      return XO_NONE;
    }

  if (vans("�нT�{�O�_�i������ݪO����H[y/N] ") != 'y')
    {
      vmsg("��������I");
      return XO_FOOT;
    }

  pos = xo->pos;
  cur = pos - xo->top;
  hdr = (HDR *) xo_pool + cur;

  hdr_fpath(fpath, xo->dir, hdr);

  hdr->xmode ^= POST_CROSSED; /* �����ۭq�аO ��post_attr()��� */
  currchrono = hdr->chrono;
  rec_put(xo->dir, hdr, sizeof(HDR), pos, cmpchrono);

  brdp = bshm->bcache;
  bend = brdp + bshm->number;

/*cache.080812: ���ɶ������νƻs��,�g������*/
  switch (ans = vans("�� ����� 1)���t 2)�Z�� 3)�ӤH �H[Q] "))
  {
  case '1':
    while (brdp < bend)
    {
      if (!strcmp(brdp->class, "���t") /*&& strcmp(brdp->brdname, "DepartAll")*/)
      /* ����ܫ��w���� �����]�t���� */
      {
        brd_fpath(folder, brdp->brdname, fn_dir);
        hdr_stamp(folder, HDR_COPY | 'A', &post, fpath);
        strcpy(post.owner, hdr->owner);
        sprintf(post.title, "[���] %s", hdr->title); /* �[������r�� */
        rec_bot(folder, &post, sizeof(HDR));
        btime_update(brd_bno(brdp->brdname)); /* �ݪO�\Ū������s */
      }
      brdp++;
    }
    break;

  case '2':
    while (brdp < bend)
    {
      if (!strcmp(brdp->class, "�Z��") /*&& strcmp(brdp->brdname, "DepartAll")*/)
      /* ����ܫ��w���� �����]�t���� */
      {
        brd_fpath(folder, brdp->brdname, fn_dir);
        hdr_stamp(folder, HDR_COPY | 'A', &post, fpath);
        strcpy(post.owner, hdr->owner);
        sprintf(post.title, "[���] %s", hdr->title); /* �[������r�� */
        rec_bot(folder, &post, sizeof(HDR));
        btime_update(brd_bno(brdp->brdname)); /* �ݪO�\Ū������s */
      }
      brdp++;
    }
    break;

  case '3':
    while (brdp < bend)
    {
      if (!strcmp(brdp->class, "�ӤH") /*&& strcmp(brdp->brdname, "DepartAll")*/)
      /* ����ܫ��w���� �����]�t���� */
      {
        brd_fpath(folder, brdp->brdname, fn_dir);
        hdr_stamp(folder, HDR_COPY | 'A', &post, fpath);
        strcpy(post.owner, hdr->owner);
        sprintf(post.title, "[���] %s", hdr->title); /* �[������r�� */
        rec_bot(folder, &post, sizeof(HDR));
        btime_update(brd_bno(brdp->brdname)); /* �ݪO�\Ū������s */
      }
      brdp++;
    }
    break;

  default:
    vmsg("��������I");
    return XO_INIT;
    break;
  }

//���T�w�O�_�n�[�W...�A�ݬ�
/*
    if (xmode & POST_CROSSED)
    {
      hdr->xmode &= ~POST_CROSSED;
    }
*/

  vmsg("��������I");
  return XO_INIT; /* ���wPOST_XXX ��s���� */
}

void
post_history(xo, hdr)          /* �N hdr �o�g�[�J brh */
  XO *xo;
  HDR *hdr;
{
  int fd;
  time_t prev, chrono, next, this;
  HDR buf;

  chrono = BMAX(hdr->chrono, hdr->stamp);
  if (!brh_unread(chrono))      /* �p�G�w�b brh ���A�N�L�ݰʧ@ */
    return;

  if ((fd = open(xo->dir, O_RDONLY)) >= 0)
  {
    prev = chrono + 1;
    next = chrono - 1;

    while (read(fd, &buf, sizeof(HDR)) == sizeof(HDR))
    {
      this = BMAX(buf.chrono, buf.stamp);

      if (chrono - this < chrono - prev)
       prev = this;
      else if (this - chrono < next - chrono)
        next = this;
    }
    close(fd);

    if (prev > chrono)      /* �S���U�@�g */
      prev = chrono;
    if (next < chrono)      /* �S���W�@�g */
      next = chrono;

    brh_add(prev, chrono, next);
  }
}

static int
do_post(xo, title)
  XO *xo;
  char *title;
{
  /* Thor.981105: �i�J�e�ݳ]�n curredit �� quote_file */
  HDR hdr;
  char fpath[64], *folder, *nick, *rcpt;
  int mode;
  time_t spendtime;
  BRD *brd;

  if (!(bbstate & STAT_POST))
  {
#ifdef NEWUSER_LIMIT
    if (cuser.lastlogin - cuser.firstlogin < 3 * 86400)
      vmsg("�s��W���A�T���l�i�i�K�峹");
    else
#endif
      vmsg("�藍�_�A�z�S���b���o��峹���v��");
    return XO_FOOT;
  }

  film_out(FILM_POST, 0);

  prints("�o��峹��i %s �j�ݪO", currboard);

#ifdef POST_PREFIX
  /* �ɥ� mode�Brcpt�Bfpath */

  if (title)
  {
    rcpt = NULL;
  }
  else		/* itoc.020113: �s�峹��ܼ��D���� */
  {

//��F����n���N�Τ���F,�H��A�ױ�
//  #define NUM_PREFIX 6
//    char *prefix[NUM_PREFIX] = {"[���i] ", "[�s�D] ", "[����] ", "[���] ", "[���D] ", "[����] "};

    move(20, 0);
//    outs("���O�G");
//    for (mode = 0; mode < NUM_PREFIX; mode++)
//      prints("%d.%s", mode + 1, prefix[mode]);

//    outs("�YBM���]�w�峹�����N�|�۰ʸ��X���");/*080419.cache: ���Ȯɳ]�w���o��*/
//    mode = vget(20, 0, "�YBM���]�w�峹�����N�|�۰ʸ��X���]�� Enter �~��^�G", fpath, 3, DOECHO) - '1';
//   if (mode >= 0 && mode < NUM_PREFIX)		// ��J�Ʀr�ﶵ
//     rcpt = prefix[mode];
//   else					// �ťո��L
//     rcpt = NULL;
  }

//  if (!ve_subject(21, title, rcpt)) //080419.cache: �o�˭쥻���Τ@�����N�����F
    if (!ve_subject(22, title, ((title) ? NULL : post_prefix())))
#else

#endif
      return XO_HEAD;

  /* ����� Internet �v���̡A�u��b�����o��峹 */
  /* Thor.990111: �S��H�X�h���ݪO, �]�u��b�����o��峹 */

  if (!HAS_PERM(PERM_INTERNET) || (currbattr & BRD_NOTRAN))
    curredit &= ~EDIT_OUTGO;

  utmp_mode(M_POST);
  fpath[0] = '\0';
  time(&spendtime);
  if (vedit(fpath, 1) < 0)
  {
    unlink(fpath);
    vmsg(msg_cancel);
    return XO_HEAD;
  }
  spendtime = time(0) - spendtime;	/* itoc.010712: �`�@�᪺�ɶ�(���) */

  /* build filename */

  folder = xo->dir;
  hdr_stamp(folder, HDR_LINK | 'A', &hdr, fpath);

  /* set owner to anonymous for anonymous board */

#ifdef HAVE_ANONYMOUS
  /* Thor.980727: lkchu�s�W��[²�檺��ܩʰΦW�\��] */
  if (curredit & EDIT_ANONYMOUS)
  {
    rcpt = anonymousid;	/* itoc.010717: �۩w�ΦW ID */
    nick = STR_ANONYMOUS;

    /* Thor.980727: lkchu patch: log anonymous post */
    /* Thor.980909: gc patch: log anonymous post filename */
    log_anonymous(hdr.xname);

#ifdef HAVE_UNANONYMOUS_BOARD
    do_unanonymous(fpath);
#endif
  }
  else
#endif
  {
    rcpt = cuser.userid;
    nick = cuser.username;
  }
  title = ve_title;
  mode = (curredit & EDIT_OUTGO) ? POST_OUTGO : 0;
#ifdef HAVE_REFUSEMARK
  if (curredit & EDIT_RESTRICT)
    mode |= POST_RESTRICT;
#endif

/* qazq.031025: ���}�O�~�� all_post */
  brd = bshm->bcache + currbno;
//  if ((brd->readlevel | brd->postlevel) < (PERM_VALID << 1))
    do_allpost(fpath, rcpt, nick, mode);

  hdr.xmode = mode;
  strcpy(hdr.owner, rcpt);
  strcpy(hdr.nick, nick);
  strcpy(hdr.title, title);

  rec_bot(folder, &hdr, sizeof(HDR));
  btime_update(currbno);

  if (mode & POST_OUTGO)
    outgo_post(&hdr, currboard);

//#if 1	/* itoc.010205: post ���峹�N�O���A�Ϥ��X�{���\Ū���ϸ� */
//  chrono = hdr.chrono;
//  prev = ((mode = rec_num(folder, sizeof(HDR)) - 2) >= 0 && !rec_get(folder, &buf, sizeof(HDR), mode)) ? buf.chrono : chrono;
//  brh_add(prev, chrono, chrono);
//#endif

  post_history(xo, &hdr);

  clear();
  outs("���Q�K�X�峹�A");

  if (currbattr & BRD_NOCOUNT || wordsnum < 30)
  {				/* itoc.010408: �H���������{�H */
    outs("�峹���C�J�����A�q�Х]�[�C");
  }
  else
  {
    /* itoc.010408: �̤峹����/�ҶO�ɶ��ӨM�w�n���h�ֿ��F����~�|���N�q */
    mode = BMIN(wordsnum, spendtime) / 10;	/* �C�Q�r/�� �@�� */
    prints("�o�O�z���� %d �g�峹�A�o %d �ȡC", ++cuser.numposts, mode);
    addmoney(mode);
  }

  /* �^�����@�̫H�c */

  if (curredit & EDIT_BOTH)
  {
    rcpt = quote_user;

    if (strchr(rcpt, '@'))	/* ���~ */
      mode = bsmtp(fpath, title, rcpt, 0);
    else			/* �����ϥΪ� */
      mode = mail_him(fpath, rcpt, title, 0);

    outs(mode >= 0 ? "\n\n���\\�^���ܧ@�̫H�c" : "\n\n�@�̵L�k���H");
  }

  unlink(fpath);

  vmsg(NULL);

  return XO_INIT;
}


int
do_reply(xo, hdr)
  XO *xo;
  HDR *hdr;
{
  curredit = 0;

  switch (vans("�� �^���� (F)�ݪO (M)�@�̫H�c (B)�G�̬ҬO (Q)�����H[F] "))
  {
  case 'm':
    hdr_fpath(quote_file, xo->dir, hdr);
    return do_mreply(hdr, 0);

  case 'q':
    return XO_FOOT;

  case 'b':
    /* �Y�L�H�H���v���A�h�u�^�ݪO */
    if (HAS_PERM(strchr(hdr->owner, '@') ? PERM_INTERNET : PERM_LOCAL))
      curredit = EDIT_BOTH;
    break;
  }

  /* Thor.981105: ���׬O��i��, �άO�n��X��, ���O�O���i�ݨ쪺, �ҥH�^�H�]��������X */
  if (hdr->xmode & (POST_INCOME | POST_OUTGO))
    curredit |= EDIT_OUTGO;

  hdr_fpath(quote_file, xo->dir, hdr);
  strcpy(quote_user, hdr->owner);
  strcpy(quote_nick, hdr->nick);
  return do_post(xo, hdr->title);
}


static int
post_reply(xo)
  XO *xo;
{
  if (bbstate & STAT_POST)
  {
    HDR *hdr;

    hdr = (HDR *) xo_pool + (xo->pos - xo->top);

#ifdef HAVE_LOCKEDMARK
    if (hdr->xmode & POST_LOCKED)    /* ��w�峹����^�� */
      {
        vmsg("�Q��w�峹����^��");
        return XO_NONE;
      }           /* �Y�n�w��w��i�^��A�o�q�N���n�[ */
#endif

#ifdef HAVE_REFUSEMARK
    if (!chkrestrict(hdr))
      return XO_NONE;
#endif

    return do_reply(xo, hdr);
  }
  return XO_NONE;
}


static int
post_add(xo)
  XO *xo;
{
  curredit = EDIT_OUTGO;
  *quote_file = '\0';
  return do_post(xo, NULL);
}


/* ----------------------------------------------------- */
/* �L�X hdr ���D					 */
/* ----------------------------------------------------- */


int
tag_char(chrono)
  int chrono;
{
  return TagNum && !Tagger(chrono, 0, TAG_NIN) ? '*' : ' ';
}


#ifdef HAVE_DECLARE
static inline int
cal_day(date)		/* itoc.010217: �p��P���X */
  char *date;
{
#if 0
   ���Ǥ����O�@�ӱ�����@�ѬO�P���X������.
   �o�����O:
         c                y       26(m+1)
    W= [---] - 2c + y + [---] + [---------] + d - 1
         4                4         10
    W �� ���ҨD������P����. (�P����: 0  �P���@: 1  ...  �P����: 6)
    c �� ���w�������~�����e���Ʀr.
    y �� ���w�������~��������Ʀr.
    m �� �����
    d �� �����
   [] �� ��ܥu���Ӽƪ���Ƴ��� (�a�O���)
    ps.�ҨD������p�G�O1���2��,�h�������W�@�~��13���14��.
       �ҥH������m�����Ƚd�򤣬O1��12,�ӬO3��14
#endif

  /* �A�� 2000/03/01 �� 2099/12/31 */

  int y, m, d;

  y = 10 * ((int) (date[0] - '0')) + ((int) (date[1] - '0'));
  d = 10 * ((int) (date[6] - '0')) + ((int) (date[7] - '0'));
  if (date[3] == '0' && (date[4] == '1' || date[4] == '2'))
  {
    y -= 1;
    m = 12 + (int) (date[4] - '0');
  }
  else
  {
    m = 10 * ((int) (date[3] - '0')) + ((int) (date[4] - '0'));
  }
  return (-1 + y + y / 4 + 26 * (m + 1) / 10 + d) % 7;
}
#endif


void
hdr_outs(hdr, cc)		/* print HDR's subject */
  HDR *hdr;
  int cc;			/* �L�X�̦h cc - 1 �r�����D */
{
  /* �^��/���/���/�\Ū�����P�D�D�^��/�\Ū�����P�D�D���/�\Ū�����P�D�D��� */
  static char *type[6] = {"Re", "Fw", "��", "\033[1;33m=>", "\033[1;33m=>", "\033[1;32m��"};
  uschar *title, *mark;
  int ch, len;
  int in_chi;		/* 1: �b����r�� */
#ifdef HAVE_DECLARE
  int square;		/* 1: �n�B�z��A */
#endif
#ifdef CHECK_ONLINE
  UTMP *online;
#endif

  /* --------------------------------------------------- */
  /* �L�X���						 */
  /* --------------------------------------------------- */

#ifdef HAVE_DECLARE
  /* itoc.010217: ��άP���X�ӤW�� */
  prints("\033[1;3%dm%s\033[m ", cal_day(hdr->date) + 1, hdr->date + 3);
#else
  outs(hdr->date + 3);
  outc(' ');
#endif

  /* --------------------------------------------------- */
  /* �L�X�@��						 */
  /* --------------------------------------------------- */

#ifdef CHECK_ONLINE
  if (online = utmp_seek(hdr))
    outs(COLOR7);
#endif

  mark = hdr->owner;
  len = IDLEN + 1;
  in_chi = 0;

  while (ch = *mark)
  {
    if (--len <= 0)
    {
      /* ��W�L len ���ת������������� */
      /* itoc.060604.����: �p�G��n���b����r���@�b�N�|�X�{�ýX�A���L�o���p�ܤֵo�͡A�ҥH�N���ޤF */
      ch = '.';
    }
    else
    {
      /* ���~���@�̧� '@' ���� '.' */
      if (in_chi || IS_ZHC_HI(ch))	/* ����r���X�O '@' ������ */
	in_chi ^= 1;
      else if (ch == '@')
	ch = '.';
    }

    outc(ch);

    if (ch == '.')
      break;

    mark++;
  }

  while (len--)
    outc(' ');

#ifdef CHECK_ONLINE
  if (online)
    outs(str_ransi);
#endif

  /* --------------------------------------------------- */
  /* �L�X���D������					 */
  /* --------------------------------------------------- */

  /* len: ���D�O type[] �̭������@�� */
  title = str_ttl(mark = hdr->title);
  len = (title == mark) ? 2 : (*mark == 'R') ? 0 : 1;
  if (!strcmp(currtitle, title))
    len += 3;
  outs(type[len]);
  outc(' ');

  /* --------------------------------------------------- */
  /* �L�X���D						 */
  /* --------------------------------------------------- */

  mark = title + cc;

#ifdef HAVE_DECLARE	/* Thor.980508: Declaration, ���ըϬY��title����� */
  square = in_chi = 0;
  if (len < 3)
  {
    if (*title == '[')
    {
      outs("\033[1m");
      square = 1;
    }
  }
#endif

  /* ��W�L cc ���ת������������� */
  /* itoc.060604.����: �p�G��n���b����r���@�b�N�|�X�{�ýX�A���L�o���p�ܤֵo�͡A�ҥH�N���ޤF */
  while ((ch = *title++) && (title < mark))
  {
#ifdef HAVE_DECLARE
    if (square)
    {
      if (in_chi || IS_ZHC_HI(ch))	/* ����r���ĤG�X�Y�O ']' ����O��A */
      {
	in_chi ^= 1;
      }
      else if (ch == ']')
      {
	outs("]\033[m");
	square = 0;			/* �u�B�z�@�դ�A�A��A�w�g�B�z���F */
	continue;
      }
    }
#endif

    outc(ch);
  }

#ifdef HAVE_DECLARE
  if (square || len >= 3)	/* Thor.980508: �ܦ��٭�� */
#else
  if (len >= 3)
#endif
    outs("\033[m");

  outc('\n');
}


/* ----------------------------------------------------- */
/* �ݪO�\���						 */
/* ----------------------------------------------------- */


static int post_body();
static int post_head();


static int
post_init(xo)
  XO *xo;
{
  xo_load(xo, sizeof(HDR));
  return post_head(xo);
}


static int
post_load(xo)
  XO *xo;
{
  xo_load(xo, sizeof(HDR));
  return post_body(xo);
}


static int
post_attr(hdr)
  HDR *hdr;
{
  int mode, attr;

  mode = hdr->xmode;

  /* �w�\Ū���p�g�A���\Ū���j�g */
  /* �m���夣�|���\Ū�O�� */
  /* �[�K�峹�����wŪ */
#ifdef HAVE_REFUSEMARK
  attr = ((mode & POST_BOTTOM) || !brh_unread(BMAX(hdr->chrono, hdr->stamp)) || !chkrestrict(hdr)) ? 0x20 : 0;
#else
  attr = ((mode & POST_BOTTOM) || !brh_unread(BMAX(hdr->chrono, hdr->stamp))) ? 0x20 : 0;
#endif

#ifdef HAVE_REFUSEMARK
  if (mode & POST_RESTRICT)
    attr |= 'X';
  else
#endif

#ifdef HAVE_LOCKEDMARK
  if (mode & POST_LOCKED)
    attr |= '!';
  else
#endif

 if (mode & POST_DONE)
   attr |= 'S';        /* qazq.030815: ����B�z���аO�� */
 else

#ifdef HAVE_LABELMARK
  if (mode & POST_DELETE)
    attr |= 'T';
  else
#endif
  if (mode & POST_MARKED)
    attr |= 'M';
  else if (mode & POST_WIKI)
    attr |= 'W';
  else if (!attr)
    attr = '+';

  return attr;
}


static void
post_item(num, hdr)
  int num;
  HDR *hdr;
{
#ifdef HAVE_SCORE /* cache.080314: ����t�� */
  char buf[10];
  sprintf(buf,"%d",num);
//  prints("%6s%c%c", (hdr->xmode & POST_BOTTOM) ? "Imp" : buf, tag_char(hdr->chrono), post_attr(hdr));

/* cache.081204: �m����h�ˤƿ�� */

//�ĥ����� token �P�_
  if (hdr->xmode & POST_BOTTOM)
  {
    if (hdr->xmode & POST_BOTTOM1)
      prints("   Imp%c%c", tag_char(hdr->chrono), post_attr(hdr));
    else if (hdr->xmode & POST_BOTTOM2)
      prints("  \033[1;33m���n\033[m%c%c", tag_char(hdr->chrono), post_attr(hdr));
    else if (hdr->xmode & POST_BOTTOM3)
      prints("    \033[1;31m��\033[m%c%c", tag_char(hdr->chrono), post_attr(hdr));
    else if (hdr->xmode & POST_BOTTOM4)
      prints("    \033[1;32m��\033[m%c%c", tag_char(hdr->chrono), post_attr(hdr));
    else if (hdr->xmode & POST_BOTTOM5)
      prints("    \033[1;33m��\033[m%c%c", tag_char(hdr->chrono), post_attr(hdr));
    else if (hdr->xmode & POST_BOTTOM6)
      prints("    \033[1;36m��\033[m%c%c", tag_char(hdr->chrono), post_attr(hdr));
//�ª��m����u��POST_BOTTOM�X�ЧP�_,�n�O�d�H���¤峹�S���m��
    else
      prints("   Imp%c%c", tag_char(hdr->chrono), post_attr(hdr));
  }
  else
    prints("%6d%c%c", num, tag_char(hdr->chrono), post_attr(hdr));

  if (hdr->xmode & POST_SCORE)
  {
    num = hdr->score;
/*080419.cache: ��²��@�I���P�_�N�n*/
/*080730.cache: ���ŭ��g�o�q���ꪺ�{���X = =*/
/*080813.cache: �Ȯɥ��o��--����h�F�@�靈���S��XDD*/
/*100107.FinFunnel: ���ק令�o��*/
	if (num == 0)
		outs(" -- "); //�аO���g������L�����Ƭ��s
	else if (abs(num) > 120)
		prints("\033[1;3%s \033[m", (num >= 0) ? "3m �z" : "0m ��");
	else if (abs(num) > 99)
		prints("\033[1;3%s \033[m", (num >= 0) ? "1m �z" : "2m ��");
	else if (abs(num) > 19)
		prints("\033[1;3%cm X%d \033[m", (num >= 0) ? '1':'2', abs(num) / 10);
    else if (abs(num) > 9)
      prints("\033[1;3%cm %d \033[m", num >= 0 ? '1' : '2', abs(num));
    else
      prints("\033[1;3%cm  %d \033[m", num >= 0 ? '1' : '2', abs(num));
/*(num <= 99 && num >= -99)�ﱼ,�ηs���覡�P�_*/
  }
  else
  {
    outs("    ");
  }
  hdr_outs(hdr, d_cols + 46);	/* �֤@��ө���� */
#else
  prints("%6s%c%c ", (hdr->xmode & POST_BOTTOM) ? "Imp" : buf, tag_char(hdr->chrono), post_attr(hdr));
  hdr_outs(hdr, d_cols + 47);
#endif
}


static int
post_body(xo)
  XO *xo;
{
  HDR *hdr;
  int num, max, tail;

  max = xo->max;
  if (max <= 0)
  {
    if (bbstate & STAT_POST)
    {
      if (vans("�n�s�W��ƶ�(Y/N)�H[N] ") == 'y')
	return post_add(xo);
    }
    else
    {
      vmsg("���ݪO�|�L�峹");
    }
    return XO_QUIT;
  }

  hdr = (HDR *) xo_pool;
  num = xo->top;
  tail = num + XO_TALL;
  if (max > tail)
    max = tail;

  move(3, 0);
  do
  {
    post_item(++num, hdr++);
  } while (num < max);
  clrtobot();

  /* return XO_NONE; */
  return XO_FOOT;	/* itoc.010403: �� b_lines ��W feeter */
}


static int
post_head(xo)
  XO *xo;
{
  vs_head(currBM, xo->xyz);
  prints(NECKER_POST, d_cols, "", currbattr & BRD_SCORE ? "��" : "��", bshm->mantime[currbno]);
  return post_body(xo);
}


/* ----------------------------------------------------- */
/* ��Ƥ��s���Gbrowse / history				 */
/* ----------------------------------------------------- */


static int
post_visit(xo)
  XO *xo;
{
  int ans, row, max;
  HDR *hdr;

  ans = vans("�]�w�Ҧ��峹 (U)��Ū (V)�wŪ (W)�e�wŪ�᥼Ū (Q)�����H[Q] ");
  if (ans == 'v' || ans == 'u' || ans == 'w')
  {
    row = xo->top;
    max = xo->max - row + 3;
    if (max > b_lines)
      max = b_lines;

    hdr = (HDR *) xo_pool + (xo->pos - row);
    /* brh_visit(ans == 'w' ? hdr->chrono : ans == 'u'); */
    /* weiyu.041010: �b�m����W�� w ���������wŪ */
    brh_visit((ans == 'u') ? 1 : (ans == 'w' && !(hdr->xmode & POST_BOTTOM)) ? hdr->chrono : 0);

    hdr = (HDR *) xo_pool;
    row = 3;
    do
    {
      move(row, 7);
      outc(post_attr(hdr++));
    } while (++row < max);
  }
  return XO_FOOT;
}

/*�ϥηs����
static void
post_history(xo, hdr)
  XO *xo;
  HDR *hdr;
{
  time_t prev, chrono, next;
  int pos, top;
  char *dir;
  HDR buf;

  if (hdr->xmode & POST_BOTTOM)
    return;

  chrono = hdr->chrono;
  if (!brh_unread(chrono))
    return;

  dir = xo->dir;
  pos = xo->pos;
  top = xo->top;

  pos--;
  if (pos >= top)
  {
    prev = hdr[-1].chrono;
  }
  else
  {
    if (!rec_get(dir, &buf, sizeof(HDR), pos))
      prev = buf.chrono;
    else
      prev = chrono;
  }

  pos += 2;
  if (pos < top + XO_TALL && pos < xo->max)
  {
    next = hdr[1].chrono;
  }
  else
  {
    if (!rec_get(dir, &buf, sizeof(HDR), pos))
      next = buf.chrono;
    else
      next = chrono;
  }

  brh_add(prev, chrono, next);
}
*/

static int
post_browse(xo)
  XO *xo;
{
  HDR *hdr;
  int xmode, pos, key;
  char *dir, fpath[64];

  dir = xo->dir;

  for (;;)
  {
    pos = xo->pos;
    hdr = (HDR *) xo_pool + (pos - xo->top);
    xmode = hdr->xmode;

#ifdef HAVE_REFUSEMARK
    if (!chkrestrict(hdr))
      break;
#endif

    hdr_fpath(fpath, dir, hdr);

    post_history(xo, hdr);
    strcpy(currtitle, str_ttl(hdr->title));

    /* Thor.990204: ���Ҽ{more �Ǧ^�� */
    if ((key = more(fpath, FOOTER_POST)) < 0)
      break;

re_key:
    switch (xo_getch(xo, key))
    {
    case XO_BODY:
      continue;

    case 'y':
    case 'r':
      if (bbstate & STAT_POST)
      {
	if (do_reply(xo, hdr) == XO_INIT)	/* �����\�a post �X�h�F */
	  return post_init(xo);
      }
      break;

    case 'm':
      if ((bbstate & STAT_BOARD) && !(xmode & POST_MARKED))
      {
	/* hdr->xmode = xmode ^ POST_MARKED; */
	/* �b post_browse �ɬݤ��� m �O���A�ҥH����u�� mark */
	hdr->xmode = xmode | POST_MARKED;
	currchrono = hdr->chrono;
	rec_put(dir, hdr, sizeof(HDR), pos, cmpchrono);
      }
      break;

#ifdef HAVE_SCORE
    case 'X':
      post_score(xo);
      return post_init(xo);

    case '%':
      post_editscore(xo);
      return post_init(xo);
#endif

    case '/':
      if (vget(b_lines, 0, "�j�M�G", hunt, sizeof(hunt), DOECHO))
      {
	more(fpath, FOOTER_POST);
	goto re_key;
      }
      continue;

    case 'E':
      return post_edit(xo);

    case 'C':	/* itoc.000515: post_browse �ɥi�s�J�Ȧs�� */
      {
	FILE *fp;
	if (fp = tbf_open())
	{
	  f_suck(fp, fpath);
	  fclose(fp);
	}
      }
      break;

    case 'h':
      xo_help("post");
      break;

/* 081229.cache: BBSRuby */
//    case '!':
//      run_ruby(fpath);
//      break;
    }
    break;
  }

  return post_head(xo);
}


/* ----------------------------------------------------- */
/* ��ذ�						 */
/* ----------------------------------------------------- */


static int
post_gem(xo)
  XO *xo;
{
  int level;
  char fpath[64];

  strcpy(fpath, "gem/");
  strcpy(fpath + 4, xo->dir);

  level = 0;
  if (bbstate & STAT_BOARD)
    level ^= GEM_W_BIT;
  if (HAS_PERM(PERM_SYSOP))
    level ^= GEM_X_BIT;
  if (bbstate & STAT_BM)
    level ^= GEM_M_BIT;

  XoGem(fpath, "��ذ�", level);
  return post_init(xo);
}


/* ----------------------------------------------------- */
/* �i�O�e��						 */
/* ----------------------------------------------------- */


static int
post_memo(xo)
  XO *xo;
{
  char fpath[64];

  brd_fpath(fpath, currboard, fn_note);
  /* Thor.990204: ���Ҽ{more �Ǧ^�� */
  if (more(fpath, NULL) < 0)
  {
    vmsg("���ݪO�|�L�u�i�O�e���v");
    return XO_HEAD;
  }

  return post_head(xo);
}


/* ----------------------------------------------------- */
/* �\��Gtag / switch / cross / forward			 */
/* ----------------------------------------------------- */


static int
post_tag(xo)
  XO *xo;
{
  HDR *hdr;
  int tag, pos, cur;

  pos = xo->pos;
  cur = pos - xo->top;
  hdr = (HDR *) xo_pool + cur;

  if (xo->key == XZ_XPOST)
    pos = hdr->xid;

  if (tag = Tagger(hdr->chrono, pos, TAG_TOGGLE))
  {
    move(3 + cur, 6);
    outc(tag > 0 ? '*' : ' ');
  }

  /* return XO_NONE; */
  return xo->pos + 1 + XO_MOVE; /* lkchu.981201: ���ܤU�@�� */
}


static int
post_switch(xo)
  XO *xo;
{
  int bno;
  BRD *brd;
  char bname[BNLEN + 1];

  if (brd = ask_board(bname, BRD_R_BIT, NULL))
  {
    if ((bno = brd - bshm->bcache) >= 0 && currbno != bno)
    {
      XoPost(bno);
      return XZ_POST;
    }
  }
  else
  {
    vmsg(err_bid);
  }
  return post_head(xo);
}

/*080812.cache: patch hialan's cross-post code*/

/*-------------------------------------------------------*/
/* target : �N�峹�����ۭq���ݪO�C��                 */
/* create : 2005/09/02                                   */
/* author : hialan.bbs@bbs.math.cycu.edu.tw              */
/*-------------------------------------------------------*/

#define MAX_CROSSBOARD 100      /* �̦h�������ݪO�� */

#define STR_CROSSLISTSTART      "<�ݪO�W��}�l>"
#define STR_CROSSLISTEND        "<�ݪO�W�浲��>"

static int
cross_loadlist(char *fpath, char boardlist[MAX_CROSSBOARD][BNLEN + 1])
{
  FILE *fp;
  int num, in_list, bno;
  char buf[ANSILINELEN], *brdname;
  BRD *brd;

  if (!(fp = fopen(fpath, "r")))
  {
    vmsg("�L�k�}���ɮסI");
    return 0;
  }

  num = in_list = 0;

  while (fgets(buf, sizeof(buf), fp) && num < MAX_CROSSBOARD)
  {
    if (!in_list)
    {
      if (!strncmp(STR_CROSSLISTSTART, buf, strlen(STR_CROSSLISTSTART)))
        in_list = 1;
      continue;
    }

    if (!strncmp(STR_CROSSLISTEND, buf, strlen(STR_CROSSLISTEND)))
      break;

    brdname = boardlist[num];

    if (sscanf(buf, " %12s", brdname) <= 0)
      continue;

    if ((bno = brd_bno(brdname)) < 0)
    {
      sprintf(buf, "%s �ݪO���s�b�I", brdname);
      vmsg(buf);
    }
    else
    {
      brd = bshm->bcache + bno;
      strcpy(brdname, brd->brdname);       /* ���ܤj�p�g */
      num++;
    }
  }

  fclose(fp);
  return num;
}


static int
cross_announce(xo)
  XO *xo;
{
  char boardlist[MAX_CROSSBOARD][BNLEN + 1];
  char *dir;
  HDR *hdr, xpost;
  int num, i;
  char xfolder[64], fpath[64];

  dir = xo->dir;
  hdr = (HDR *) xo_pool + (xo->pos - xo->top);
  hdr_fpath(fpath, dir, hdr);

  if (!(num = cross_loadlist(fpath, boardlist)))
  {
    vmsg("�W��榡����άO�S���W��I");
    return XO_FOOT;
  }

  clear();
  prints("���D�G%s\n", hdr->title);
  outs("�N��ܤU�C�ݪO\n");

  for (i = 0; i < num && i < (b_lines - 4) * b_cols / (BNLEN + 1); i++)
  {
    prints("%-13.13s", boardlist[i]);
    if (i % (b_cols / (BNLEN + 1)) == b_cols / (BNLEN + 1) - 1)
      outs("\n");
  }

  if (vans(msg_sure_ny) == 'y')
  {
    for (i = 0; i < num; i++)
    {
      brd_fpath(xfolder, boardlist[i], fn_dir);

      /* itoc.030325: ���������� copy �Y�i */
      hdr_stamp(xfolder, HDR_COPY | 'A', &xpost, fpath);

      strcpy(xpost.owner, hdr->owner);
      strcpy(xpost.nick, hdr->nick);
      strcpy(xpost.date, hdr->date);    /* �������O�d���� */
      strcpy(xpost.title, hdr->title);

      rec_bot(xfolder, &xpost, sizeof(HDR));
    }
  }
  else
    vmsg(msg_cancel);

  return XO_HEAD;
}

int
post_cross(xo)
  XO *xo;
{
  /* �ӷ��ݪO */
  char *dir, *ptr;
  HDR *hdr, xhdr;

  /* ����h���ݪO */
  int xbno;
  usint xbattr;
  char xboard[BNLEN + 1], xfolder[64];
  HDR xpost;

  int tag, rc, locus, finish;
  int method;		/* 0:������ 1:�q���}�ݪO/��ذ�/�H�c����峹 2:�q���K�ݪO����峹 */
  usint tmpbattr;
  char tmpboard[BNLEN + 1];
  char fpath[64], buf[ANSILINELEN];
  FILE *fpr, *fpw, *fpo;

  if (!cuser.userlevel)	/* itoc.000213: �קK guest ����h sysop �O */
    return XO_NONE;

  tag = AskTag("���");
  if (tag < 0)
    return XO_FOOT;

  dir = xo->dir;

//  if (!ask_board(xboard, BRD_W_BIT, "\n\n\033[1;33m�ЬD��A���ݪO�A��������W�L�W�w���ơC\033[m\n\n") ||
//    (*dir == 'b' && !strcmp(xboard, currboard)))	/* �H�c�B��ذϤ��i�H�����currboard */
//    return XO_HEAD;

  hdr = tag ? &xhdr : (HDR *) xo_pool + (xo->pos - xo->top);	/* lkchu.981201: ������ */

  /* ��@������ۤv�峹�ɡA�i�H��ܡu�������v */
//  method = (HAS_PERM(PERM_ALLBOARD) || (!tag && is_author(hdr))) &&
//    (vget(2, 0, "(1)������ (2)����峹�H[1] ", buf, 3, DOECHO) != '2') ? 0 : 1;

  if (!HAS_PERM(PERM_ALLBOARD) && (tag || strcmp(hdr->owner, cuser.userid)))
    method = 1;
  else
  {
    vget(1, 0, HAS_PERM(PERM_ALLBOARD) ?
      "(1)������ (2)����峹 (3)�ݪO�`��CrossPost�H[Q] " :
      "(1)������ (2)����峹�H[Q] ", buf, 3, DOECHO);

    method = (buf[0] == '1') ? 0 :
             (buf[0] == '2') ? 1 :
             (buf[0] == '3') ? 3 : -1;

    if (method == 3)
      return cross_announce(xo);

    if (method < 0)
      return XO_HEAD;
  }

  if (!ask_board(xboard, BRD_W_BIT, "\n\n"
    "\033[1;33m�ЬD��A���ݪO�A��������W�L�W�w���ơC\033[m\n\n") ||
    (*dir == 'b' && !strcmp(xboard, currboard)))
    /* �H�c�B��ذϤ��i�H�����currboard */
    return XO_HEAD;

  if (!tag)	/* lkchu.981201: �������N���n�@�@�߰� */
  {
    if (method)
      sprintf(ve_title, "Fw: %.68s", str_ttl(hdr->title));	/* �w�� Re:/Fw: �r�˴N�u�n�@�� Fw: */
    else
      strcpy(ve_title, hdr->title);

    if (!vget(2, 0, "���D�G", ve_title, TTLEN + 1, GCARRY))
      return XO_HEAD;
  }

#ifdef HAVE_REFUSEMARK
  rc = vget(2, 0, "(S)�s�� (L)���� (X)�K�� (Q)�����H[Q] ", buf, 3, LCECHO);
  if (rc != 'l' && rc != 's' && rc != 'x')
#else
  rc = vget(2, 0, "(S)�s�� (L)���� (Q)�����H[Q] ", buf, 3, LCECHO);
  if (rc != 'l' && rc != 's')
#endif
    return XO_HEAD;

  if (method && *dir == 'b')	/* �q�ݪO��X�A���ˬd���ݪO�O�_�����K�O */
  {
    /* �ɥ� tmpbattr */
    tmpbattr = (bshm->bcache + currbno)->readlevel;
    if (tmpbattr == PERM_SYSOP || tmpbattr == PERM_BOARD)
      method = 2;
  }

  xbno = brd_bno(xboard);
  xbattr = (bshm->bcache + xbno)->battr;

  /* Thor.990111: �b�i�H��X�e�A�n�ˬd���S����X���v�O? */
  if ((rc == 's') && (!HAS_PERM(PERM_INTERNET) || (xbattr & BRD_NOTRAN)))
    rc = 'l';

  /* �ƥ� currboard */
  if (method)
  {
    /* itoc.030325: �@������I�s ve_header�A�|�ϥΨ� currboard�Bcurrbattr�A���ƥ��_�� */
    strcpy(tmpboard, currboard);
    strcpy(currboard, xboard);
    tmpbattr = currbattr;
    currbattr = xbattr;
  }

  locus = 0;
  do	/* lkchu.981201: ������ */
  {
    if (tag)
    {
      EnumTag(hdr, dir, locus, sizeof(HDR));

      if (method)
	sprintf(ve_title, "Fw: %.68s", str_ttl(hdr->title));	/* �w�� Re:/Fw: �r�˴N�u�n�@�� Fw: */
      else
	strcpy(ve_title, hdr->title);
    }

    if (hdr->xmode & GEM_FOLDER)	/* �D plain text ������ */
      continue;

#ifdef HAVE_REFUSEMARK
    if (hdr->xmode & POST_RESTRICT)
      continue;
#endif

    hdr_fpath(fpath, dir, hdr);

#ifdef HAVE_DETECT_CROSSPOST
    if (check_crosspost(fpath, xbno))
      break;
#endif

    brd_fpath(xfolder, xboard, fn_dir);

    if (method)		/* �@����� */
    {
      /* itoc.030325: �@������n���s�[�W header */
      fpw = fdopen(hdr_stamp(xfolder, 'A', &xpost, buf), "w");
      ve_header(fpw);

      /* itoc.040228: �p�G�O�q��ذ�����X�Ӫ��ܡA�|�������� [currboard] �ݪO�A
	 �M�� currboard �����O�Ӻ�ذϪ��ݪO�C���L���O�ܭ��n�����D�A�ҥH�N���ޤF :p */
      fprintf(fpw, "�� ��������� [%s] %s\n\n",
	*dir == 'u' ? cuser.userid : method == 2 ? "���K" : tmpboard,
	*dir == 'u' ? "�H�c" : "�ݪO");

      /* Kyo.051117: �Y�O�q���K�ݪO��X���峹�A�R���峹�Ĥ@��ҰO�����ݪO�W�� */
      finish = 0;
      if ((method == 2) && (fpr = fopen(fpath, "r")))
      {
	if (fgets(buf, sizeof(buf), fpr) &&
	  ((ptr = strstr(buf, str_post1)) || (ptr = strstr(buf, str_post2))) && (ptr > buf))
	{
	  ptr[-1] = '\n';
	  *ptr = '\0';

	  do
	  {
	    fputs(buf, fpw);
	  } while (fgets(buf, sizeof(buf), fpr));
	  finish = 1;
	}
	fclose(fpr);
      }
      if (!finish)
	f_suck(fpw, fpath);

      fclose(fpw);

      strcpy(xpost.owner, cuser.userid);
      strcpy(xpost.nick, cuser.username);
    }
    else		/* ������ */
    {
      /* itoc.030325: ���������� copy �Y�i */
      hdr_stamp(xfolder, HDR_COPY | 'A', &xpost, fpath);

      strcpy(xpost.owner, hdr->owner);
      strcpy(xpost.nick, hdr->nick);
      strcpy(xpost.date, hdr->date);	/* �������O�d���� */
    }

/* 20081130.cache: ����峹�|�b��峹���� */
  if (!HAS_PERM(PERM_ALLBOARD))
  {
    if (fpo = fopen(fpath, "a"))
    {
      char msg[64];
      usint checklevel;

      time_t now;
      struct tm *ptime;

      time(&now);
      ptime = localtime(&now);

      checklevel = (bshm->bcache + xbno)->readlevel;

      sprintf(msg, "�����%s�ݪO",
             (checklevel == PERM_SYSOP || checklevel == PERM_BOARD) ?
             "�Y���K" : xboard); /* �ˬd�ؼЬݪO�\Ū�v�� */

      fprintf(fpo, "\033[1;36m%13s\033[m \033[1;33m��\033[m�G%-51s %02d/%02d/%02d\n",
      cuser.userid, msg, ptime->tm_year % 100, ptime->tm_mon + 1, ptime->tm_mday);
      /* �榡�ۭq �ɶq�P�������� */

      fclose(fpo);

    } /* ��������O�� 070317 guessi */
  }

    strcpy(xpost.title, ve_title);

    if (rc == 's')
      xpost.xmode = POST_OUTGO;
#ifdef HAVE_REFUSEMARK
    else if (rc == 'x')
      xpost.xmode = POST_RESTRICT;
#endif

    rec_bot(xfolder, &xpost, sizeof(HDR));

    if (rc == 's')
      outgo_post(&xpost, xboard);
  } while (++locus < tag);

  btime_update(xbno);

  /* Thor.981205: check �Q�઺�O���S���C�J����? */
  if (!(xbattr & BRD_NOCOUNT))
    cuser.numposts += tag ? tag : 1;	/* lkchu.981201: �n�� tag */

  /* �_�� currboard�Bcurrbattr */
  if (method)
  {
    strcpy(currboard, tmpboard);
    currbattr = tmpbattr;
  }

  vmsg("�������");
  return XO_HEAD;
}


int
post_forward(xo)
  XO *xo;
{
  ACCT muser;
  int result;
  HDR *hdr;

  if (!HAS_PERM(PERM_LOCAL))
    return XO_NONE;

  hdr = (HDR *) xo_pool + (xo->pos - xo->top);

  if (hdr->xmode & GEM_FOLDER)	/* �D plain text ������ */
    return XO_NONE;

#ifdef HAVE_REFUSEMARK
  if (!chkrestrict(hdr))
    return XO_NONE;
#endif

  if (acct_get("��F�H�󵹡G", &muser) > 0)
  {
    strcpy(quote_user, hdr->owner);
    strcpy(quote_nick, hdr->nick);
    hdr_fpath(quote_file, xo->dir, hdr);
    sprintf(ve_title, "%.64s (fwd)", hdr->title);
    move(1, 0);
    clrtobot();
    prints("��F��: %s (%s)\n��  �D: %s\n", muser.userid, muser.username, ve_title);


  if (!HAS_PERM(PERM_ALLBOARD))
  {
    result = mail_send(muser.userid);

    if (result >= 0)
    {
      char fpath[64];
      FILE *fp;

      hdr_fpath(fpath, xo->dir, hdr);

#if 0
�o�䤣�T�w��_�����ϥΤW���ж��⪺�ܼƷ�@ FILE���ɮ׸��|
�]�� hdr_path �쪺���@��
�p�G�i�H�A�h�U���� fpath �Ч令 quote_file
#endif

      if (fp = fopen(fpath, "a"))
      {
        char msg[32];

        time_t now;
        struct tm *ptime;

        time(&now);
        ptime = localtime(&now);

        sprintf(msg, "��H��%s�H�c", (muser.userid == cuser.userid) ?
          muser.userid : "�Y�p�H");

      fprintf(fp, "\033[1;36m%13s\033[m \033[1;33m��\033[m�G%-51s %02d/%02d/%02d\n",
      cuser.userid, msg, ptime->tm_year % 100, ptime->tm_mon + 1, ptime->tm_mday);
          /* �榡�ۭq �ɶq�P�������� */

        fclose(fp);

      } /* ��������O�� 070510 hpo14 */
    }
  }
  else
    mail_send(muser.userid);

    *quote_file = '\0';
  }
  return XO_HEAD;
}


/* ----------------------------------------------------- */
/* �O�D�\��Gmark / delete / label			 */
/* ----------------------------------------------------- */


static int
post_mark(xo)
  XO *xo;
{
  if (bbstate & STAT_BOARD)
  {
    HDR *hdr;
    int pos, cur, xmode, userno;;
    char ans,buf[40];

    pos = xo->pos;
    cur = pos - xo->top;
    hdr = (HDR *) xo_pool + cur;
    xmode = hdr->xmode;

#ifdef HAVE_LABELMARK
    if (xmode & POST_DELETE)	/* �ݬ媺�峹���� mark */
      return XO_NONE;
#endif

    BRD *oldbrd, newbrd;
    oldbrd = bshm->bcache + currbno;
    memcpy(&newbrd, oldbrd, sizeof(BRD));

    if(newbrd.battr & BRD_EVALUATE)
      {
         if(hdr->xmode & POST_MARKED)
           {
             hdr->xmode = xmode ^ POST_MARKED;
             currchrono = hdr->chrono;
             rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono);
             move(3 + cur, 7);
             outc(post_attr(hdr));
             ans = vans("�� �O�_�N���g�����u�� Y)�O N)�_ �H(�w�]=�_) ");

             if(ans == 'y' || ans == 'Y')
               {
                 if((userno = acct_userno(hdr->owner)) > 0)
                   {
                      extern UCACHE *ushm;
                      int online = 0;
                      UTMP *uentp, *uceil;
                      uentp = ushm->uslot;
                      uceil = (void *) uentp + ushm->offset;
                      do
                        {
                           if(uentp->userno == userno)
                             {
                             /* �b�u�W�A�g�J utmp-> */
                               if(uentp->goodart > 0)
                                 uentp->goodart--;
                               online = 1;
                               sprintf(buf, "�]�w�����I %s �u��g�ƴ %d �g�C", uentp->userid, uentp->goodart);
                             }
                        } while (++uentp <= uceil);

                      if(online)
                         vmsg(buf);
                      else        /* ���b�u�W�A�g�J .ACCT */
                        {
                           ACCT acct;
                           if(acct_load(&acct, hdr->owner) >= 0)
                             {
                                if(acct.goodart > 0)
                                  acct.goodart--;
                                acct_save(&acct);
                                sprintf(buf, "�]�w�����I %s �u��g�ƴ %d �g�C", acct.userid, acct.goodart);
                                vmsg(buf);
                             }
                        }//if online
                   }//if userno
               }//if ans
           }//if MARKED
         else
           {
             hdr->xmode = xmode ^ POST_MARKED;
             currchrono = hdr->chrono;
             rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono);
             move(3 + cur, 7);
             outc(post_attr(hdr));

             ans = vans("�� �O�_�N���g�]���u�� Y)�O N)�_ �H(�w�]=�_) ");
             if(ans == 'y' || ans == 'Y')
               {
                 if((userno = acct_userno(hdr->owner)) > 0)
                   {
                      extern UCACHE *ushm;
                      int online = 0;
                      UTMP *uentp, *uceil;
                      uentp = ushm->uslot;
                      uceil = (void *) uentp + ushm->offset;
                      do
                        {
                           if(uentp->userno == userno)
                             {
                             /* �b�u�W�A�g�J utmp-> */
                               uentp->goodart++;
                               online = 1;
                               sprintf(buf, "�]�w�����I %s �u��g�ƼW�� %d �g�C", uentp->userid, uentp->goodart);
                             }
                        } while (++uentp <= uceil);

                      if(online)
                        vmsg(buf);
                      else        /* ���b�u�W�A�g�J .ACCT */
                        {
                           ACCT acct;
                           if(acct_load(&acct, hdr->owner) >= 0)
                              {
                                 acct.goodart++;
                                 acct_save(&acct);
                                 sprintf(buf, "�]�w�����I %s �u��g�ƼW�� %d �g�C", acct.userid, acct.goodart);
                                 vmsg(buf);
                              }
                        }//online
                   }//if userno
                }//if ans
            }//else
      }//if EVALUATE

    hdr->xmode = xmode ^ POST_MARKED;
    currchrono = hdr->chrono;
    rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono);

    move(3 + cur, 7);
    outc(post_attr(hdr));
  }
  return XO_FOOT;
}

static int
post_wiki(xo)
  XO *xo;
{
  HDR *hdr;
  int pos, cur, xmode;

  if (!cuser.userlevel) /* itoc.020114: guest ������L guest ���峹���� */
    return XO_NONE;

  pos = xo->pos;
  cur = pos - xo->top;
  hdr = (HDR *) xo_pool + cur;

  if (HAS_PERM(PERM_ALLBOARD) || !strcmp(hdr->owner, cuser.userid) || (bbstate & STAT_BOARD))
  {
    xmode = hdr->xmode;

#ifdef HAVE_LABELMARK
    if (xmode & POST_DELETE)    /* �ݬ媺�峹���� wiki */
      return XO_NONE;
#endif

    hdr->xmode = xmode ^ POST_WIKI;
    currchrono = hdr->chrono;
    rec_put(xo->dir, hdr, sizeof(HDR), pos, cmpchrono);

    move(3 + cur, 7);
    outc(post_attr(hdr));
  }
  return XO_NONE;
}

static int          /* qazq.030815: ����B�z�����аO��*/
post_done_mark(xo)
  XO *xo;
{
  if (HAS_PERM(PERM_ALLADMIN)) /* qazq.030815: �u�������i�H�аO */
  {
    HDR *hdr;
    int pos, cur, xmode;

    pos = xo->pos;
    cur = pos - xo->top;
    hdr = (HDR *) xo_pool + cur;
    xmode = hdr->xmode;

#ifdef HAVE_LABELMARK
    if (xmode & POST_DELETE)    /* �ݬ媺�峹���� mark */
      return XO_NONE;
#endif

    hdr->xmode = xmode ^ POST_DONE;
    currchrono = hdr->chrono;

    rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ?
      hdr->xid : pos, cmpchrono);
    move(3 + cur, 7);
    outc(post_attr(hdr));
  }
  return XO_NONE;
}

static int
post_bottom(xo)
  XO *xo;
{
  if (bbstate & STAT_BOARD)
  {
    HDR *hdr, post;
    char fpath[64], ans, choice;

    hdr = (HDR *) xo_pool + (xo->pos - xo->top);

    hdr_fpath(fpath, xo->dir, hdr);
    hdr_stamp(xo->dir, HDR_LINK | 'A', &post, fpath);

  switch (choice = vans("�T�w�n��o�g�峹�m���� �H[Y/N/C]? [N] "))
  {
  case 'y':
  case 'Y':
#ifdef HAVE_REFUSEMARK
    post.xmode = POST_BOTTOM | POST_BOTTOM1 | POST_MARKED | (hdr->xmode & POST_RESTRICT);
#else
    post.xmode = POST_BOTTOM | POST_BOTTOM1 | POST_MARKED;
#endif
    break;

  case 'c':
  case 'C':
/* cache.081205: �m���Ÿ���� */
      switch (ans = vans("�� �m����� 1)Imp 2)���n 3)���� 4)�� 5)���� 6)�š� �H(�w�]=Imp) "))
      {
      case '1':
#ifdef HAVE_REFUSEMARK
    post.xmode = POST_BOTTOM | POST_BOTTOM1 | POST_MARKED | (hdr->xmode & POST_RESTRICT);
#else
    post.xmode = POST_BOTTOM | POST_BOTTOM1 | POST_MARKED;
#endif
        break;

      case '2':
#ifdef HAVE_REFUSEMARK
    post.xmode = POST_BOTTOM | POST_BOTTOM2 | POST_MARKED | (hdr->xmode & POST_RESTRICT);
#else
    post.xmode = POST_BOTTOM | POST_BOTTOM2 | POST_MARKED;
#endif
        break;

      case '3':
#ifdef HAVE_REFUSEMARK
    post.xmode = POST_BOTTOM | POST_BOTTOM3 | POST_MARKED | (hdr->xmode & POST_RESTRICT);
#else
    post.xmode = POST_BOTTOM | POST_BOTTOM3 | POST_MARKED;
#endif
        break;

      case '4':
#ifdef HAVE_REFUSEMARK
    post.xmode = POST_BOTTOM | POST_BOTTOM4 | POST_MARKED | (hdr->xmode & POST_RESTRICT);
#else
    post.xmode = POST_BOTTOM | POST_BOTTOM4 | POST_MARKED;
#endif
        break;

      case '5':
#ifdef HAVE_REFUSEMARK
    post.xmode = POST_BOTTOM | POST_BOTTOM5 | POST_MARKED | (hdr->xmode & POST_RESTRICT);
#else
    post.xmode = POST_BOTTOM | POST_BOTTOM5 | POST_MARKED;
#endif
        break;

      case '6':
#ifdef HAVE_REFUSEMARK
    post.xmode = POST_BOTTOM | POST_BOTTOM6 | POST_MARKED | (hdr->xmode & POST_RESTRICT);
#else
    post.xmode = POST_BOTTOM | POST_BOTTOM6 | POST_MARKED;
#endif
        break;

      default:
#ifdef HAVE_REFUSEMARK
    post.xmode = POST_BOTTOM | POST_BOTTOM1 | POST_MARKED | (hdr->xmode & POST_RESTRICT);
#else
    post.xmode = POST_BOTTOM | POST_BOTTOM1 | POST_MARKED;
#endif
      }
     break;
/* cache.081205: �m���Ÿ���� */

   default:
     return XO_FOOT;
  }

    strcpy(post.owner, hdr->owner);
    strcpy(post.nick, hdr->nick);
    strcpy(post.title, hdr->title);

    rec_add(xo->dir, &post, sizeof(HDR));
    //btime_update(currbno);  /* �m���峹���|�C�J��Ū */


    return post_load(xo);	/* �ߨ���ܸm���峹 */
  }
  return XO_NONE;
}


#ifdef HAVE_REFUSEMARK
static int
post_refuse(xo)		/* itoc.010602: �峹�[�K */
  XO *xo;
{
  HDR *hdr;
  int pos, cur;

  if (!cuser.userlevel)	/* itoc.020114: guest ������L guest ���峹�[�K */
    return XO_NONE;

  pos = xo->pos;
  cur = pos - xo->top;
  hdr = (HDR *) xo_pool + cur;

//#ifdef HAVE_LOCKEDMARK
//    if (xmode & POST_LOCKED)    /* ��w�峹����[�K */
//      return XO_NONE;           /* �Y�n�w��w��i�[�K�A�o�q�N���n�[ */
//#endif

  if (is_author(hdr) || (bbstate & STAT_BM))
  {

/* 090127.cache: �峹�[�K�W�� */
    if (hdr->xmode & POST_RESTRICT)
    {
#if 0 //���߰ݪ����屼�i���W��
      if (vans("�Ѱ��峹�O�K�|�R�������i���W��A�z�T�w��(Y/N)�H[N] ") != 'y')
      {
        move(3 + cur, 7);
        outc(post_attr(hdr));
        return XO_FOOT;
      }
#endif
      RefusePal_kill(currboard, hdr);
    }
//�峹�[�K�W��

    hdr->xmode ^= POST_RESTRICT;
    currchrono = hdr->chrono;
    rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono);

    move(3 + cur, 7);
    outc(post_attr(hdr));

//�K���n�ۤv�� Shift+O �h�s�W�[�K�峹�i���W��
//    if (hdr->xmode & POST_RESTRICT)
//      return XoBM_Refuse_pal(xo);
  }

  return XO_NONE;
}
#endif


#ifdef HAVE_LOCKEDMARK
static int
post_locked(xo)     /* hpo14.20070513: �峹��w */
  XO *xo;           /* �� STAT_BOARD �v���~�L�k�R��/�s�� */
{
  if (bbstate & STAT_BOARD)
  {
    HDR *hdr;
    int pos, cur, xmode;

    pos = xo->pos;
    cur = pos - xo->top;
    hdr = (HDR *) xo_pool + cur;
    xmode = hdr->xmode;

//#ifdef HAVE_LABELMARK
//    if (xmode & POST_DELETE)    /* �ݬ�峹������w */
//      return XO_NONE;
//#endif

#ifdef HAVE_REFUSEMARK
    if (xmode & POST_RESTRICT)  /* �[�K�峹������w */
      return XO_NONE;           /* �Y�n�[�K��i��w�A�o�q�N���n�[ */
#endif

    if (vans(xmode & POST_LOCKED ? "�Ѱ��峹��w [Y/N]? [N] " : "��w���g�峹 [Y/N]? [N] ") != 'y')
      return XO_FOOT;

    if (xmode & POST_LOCKED)
    {
      hdr->xmode &= ~POST_LOCKED;
      hdr->xmode &= ~POST_MARKED;
    }
    else
    {
      /* ��w���峹�A�q�`�ܭ��n�A�ҥH�[ POST_MARKED */
      if (xmode & POST_MARKED)
        hdr->xmode ^= ( POST_LOCKED );
      else
        hdr->xmode ^= ( POST_LOCKED | POST_MARKED );
    }

//#ifdef HAVE_POST_BOTTOM         /* hpo14.070523:�Ĥ@�ظm������w�P�B */
//      if (hdr->xmode & POST_BOTTOM2)
//        hdr->xmode ^= POST_BOTTOM;  /* �קK�����ܦ��å� */
//#endif

    currchrono = hdr->chrono;
    rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos,cmpchrono);

//#ifdef HAVE_POST_BOTTOM         /* hpo14.070523:�Ĥ@�ظm������w�P�B */
//    if (hdr->xmode & POST_BOTTOM)   /* �P�B�m�����å� */
//    {
//      char fpath[64];
//      brd_fpath(fpath, currboard, FN_BOTTOM);
//      hdr->xmode ^= POST_BOTTOM;
//      rec_put(fpath, hdr, sizeof(HDR), 0, cmpchrono);
//      return XO_LOAD;
//    }
//#endif

    move(3 + cur, 7);
    outc(post_attr(hdr));
  }
  return XO_LOAD;
}
#endif

#ifdef HAVE_LABELMARK
static int
post_label(xo)
  XO *xo;
{
  if (bbstate & STAT_BOARD)
  {
    HDR *hdr;
    int pos, cur, xmode;

    pos = xo->pos;
    cur = pos - xo->top;
    hdr = (HDR *) xo_pool + cur;
    xmode = hdr->xmode;

    if (xmode & (POST_MARKED | POST_RESTRICT | POST_LOCKED))	/* mark �� �[�K���峹����ݬ� */
      return XO_NONE;

    hdr->xmode = xmode ^ POST_DELETE;
    currchrono = hdr->chrono;
    rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono);

    move(3 + cur, 7);
    outc(post_attr(hdr));

    return pos + 1 + XO_MOVE;	/* ���ܤU�@�� */
  }

  return XO_NONE;
}


static int
post_delabel(xo)
  XO *xo;
{
  int fdr, fsize, xmode;
  char fnew[64], fold[64], *folder;
  HDR *hdr;
  FILE *fpw;

  if (!(bbstate & STAT_BOARD))
    return XO_NONE;

  if (vans("�T�w�n�R���ݬ�峹��(Y/N)�H[N] ") != 'y')
    return XO_FOOT;

  folder = xo->dir;
  if ((fdr = open(folder, O_RDONLY)) < 0)
    return XO_FOOT;

  if (!(fpw = f_new(folder, fnew)))
  {
    close(fdr);
    return XO_FOOT;
  }

  fsize = 0;
  mgets(-1);
  while (hdr = mread(fdr, sizeof(HDR)))
  {
    xmode = hdr->xmode;

    if (!(xmode & POST_DELETE))
    {
      if ((fwrite(hdr, sizeof(HDR), 1, fpw) != 1))
      {
	close(fdr);
	fclose(fpw);
	unlink(fnew);
	return XO_FOOT;
      }
      fsize++;
    }
    else
    {
      /* �s�u��H */
      cancel_post(hdr);

      hdr_fpath(fold, folder, hdr);
      unlink(fold);
      if (xmode & POST_RESTRICT)
        RefusePal_kill(currboard, hdr);
    }
  }
  close(fdr);
  fclose(fpw);

  sprintf(fold, "%s.o", folder);
  rename(folder, fold);
  if (fsize)
    rename(fnew, folder);
  else
    unlink(fnew);

  btime_update(currbno);

  return post_load(xo);
}
#endif


static int
post_delete(xo)
  XO *xo;
{
  int pos, cur, by_BM;
  HDR *hdr;
  char buf[80];

  if (!cuser.userlevel ||
    !strcmp(currboard, BN_DELETED) ||
    !strcmp(currboard, BN_JUNK))
    return XO_NONE;

  pos = xo->pos;
  cur = pos - xo->top;
  hdr = (HDR *) xo_pool + cur;

#ifdef HAVE_LOCKEDMARK
    if (hdr->xmode & POST_LOCKED)    /* ��w�峹������ */
      {
        vmsg("�Q��w�峹������");
        return XO_NONE;
      }           /* �Y�n�w��w��i���A�o�q�N���n�[ */
#endif

  if ((hdr->xmode & POST_MARKED) || (!(bbstate & STAT_BOARD) && !is_author(hdr)))
    return XO_NONE;

  by_BM = bbstate & STAT_BOARD;

  if (vans(msg_del_ny) == 'y')
  {
    currchrono = hdr->chrono;

    BRD *oldbrd, newbrd;
    oldbrd = bshm->bcache + currbno;
    memcpy(&newbrd, oldbrd, sizeof(BRD));
//    char ans;
#ifdef BAD_POST
    if(bbstate & STAT_BOARD && (newbrd.battr & BRD_EVALUATE))
    {
       ans = vans("�� �O�_�N���g�]���H�� Y)�O N)�_ �H(�w�]=�_) ");
       if(ans == 'y' || ans == 'Y')
          {
             if((userno = acct_userno(hdr->owner)) > 0)
                {
                   extern UCACHE *ushm;
                   int online = 0;
                   UTMP *uentp, *uceil;
                   uentp = ushm->uslot;
                   uceil = (void *) uentp + ushm->offset;
                   do
                     {
                       if(uentp->userno == userno)
                          {
                          /* �b�u�W�A�g�J utmp-> */
                             uentp->badart++;
                             online = 1;
                             sprintf(buf, "�]�w�����I %s �H��g�ƼW�� %d �g�C", uentp->userid, uentp->badart);
                           }
                     } while (++uentp <= uceil);

                   if(online)
                      vmsg(buf);
                   else        /* ���b�u�W�A�g�J .ACCT */
                     {
                        ACCT acct;
                        if(acct_load(&acct, hdr->owner) >= 0)
                          {
                             acct.badart++;
                             acct_save(&acct);
                             sprintf(buf, "�]�w�����I %s �H��g�ƼW�� %d �g�C", acct.userid, acct.badart);
                             vmsg(buf);
                          }
                     }//if online
                 }//if userno
           }//if ans
    }//if battr
#endif
    if (!rec_del(xo->dir, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono))
    {
      pos = move_post(hdr, xo->dir, by_BM, 0);

      if (!by_BM && !(currbattr & BRD_NOCOUNT) && !(hdr->xmode & POST_BOTTOM))
      {
	/* itoc.010711: ��峹�n�����A���ɮפj�p */
	pos = pos >> 3;	/* �۹�� post �� wordsnum / 10 */

	/* itoc.010830.����: �|�}: �Y multi-login �夣��t�@������ */
	if (cuser.money > pos)
	  cuser.money -= pos;
	else
	  cuser.money = 0;

	if (cuser.numposts > 0)
	  cuser.numposts--;
	sprintf(buf, "%s�A�z���峹� %d �g", MSG_DEL_OK, cuser.numposts);
	vmsg(buf);
      }

      if (xo->key == XZ_XPOST)
      {
	vmsg("��C��g�R����V�áA�Э��i�걵�Ҧ��I");
	return XO_QUIT;
      }
      return XO_LOAD;
    }
  }
  return XO_FOOT;
}


static int
chkpost(hdr)
  HDR *hdr;
{
  return (hdr->xmode & POST_MARKED);
}


static int
vfypost(hdr, pos)
  HDR *hdr;
  int pos;
{
  return (Tagger(hdr->chrono, pos, TAG_NIN) || chkpost(hdr));
}


static void
delpost(xo, hdr)
  XO *xo;
  HDR *hdr;
{
  char fpath[64];

  cancel_post(hdr);
  hdr_fpath(fpath, xo->dir, hdr);
  unlink(fpath);
  if (hdr->xmode & POST_RESTRICT)
    RefusePal_kill(currboard, hdr);
}

static void
delpost_movepost(xo, hdr)
  XO *xo;
  HDR *hdr;
{
  cancel_post(hdr);
  move_post(hdr, xo->dir, 1, 1);
}

static int
post_rangedel(xo)
  XO *xo;
{
  if (!(bbstate & STAT_BOARD))
    return XO_NONE;

  btime_update(currbno);

  return xo_rangedel(xo, sizeof(HDR), chkpost, delpost_movepost);
}


static int
post_prune(xo)
  XO *xo;
{
  int ret;

  if (!(bbstate & STAT_BOARD))
    return XO_NONE;

  ret = xo_prune(xo, sizeof(HDR), vfypost, delpost);

  btime_update(currbno);

  if (xo->key == XZ_XPOST && ret == XO_LOAD)
  {
    vmsg("��C��g�妸�R����V�áA�Э��i�걵�Ҧ��I");
    return XO_QUIT;
  }

  return ret;
}


static int
post_copy(xo)	   /* itoc.010924: ���N gem_gather */
  XO *xo;
{
  int tag;

  tag = AskTag("�ݪO�峹����");

  if (tag < 0)
    return XO_FOOT;

#ifdef HAVE_REFUSEMARK
  gem_buffer(xo->dir, tag ? NULL : (HDR *) xo_pool + (xo->pos - xo->top), chkrestrict);
#else
  gem_buffer(xo->dir, tag ? NULL : (HDR *) xo_pool + (xo->pos - xo->top), NULL);
#endif

  if (bbstate & STAT_BOARD)
  {
#ifdef XZ_XPOST
    if (xo->key == XZ_XPOST)
    {
      zmsg("�ɮ׼аO�����C[�`�N] �z���������}�걵�Ҧ��~��i�J��ذϡC");
      return XO_FOOT;
    }
    else
#endif
    {
      zmsg("���������C[�`�N] �K�W��~��R�����I");
      return post_gem(xo);	/* �����������i��ذ� */
    }
  }

  zmsg("�ɮ׼аO�����C[�`�N] �z�u��b���(�p)�O�D�Ҧb�έӤH��ذ϶K�W�C");
  return XO_FOOT;
}


/* ----------------------------------------------------- */
/* �����\��Gedit / title				 */
/* ----------------------------------------------------- */

/* append data from tail of src (starting point=off) to dst */
int
AppendTail(const char *src, const char *dst, int off)
{
    int fi, fo, bytes;
    char buf[8192];

    fi=open(src, O_RDONLY);
    if(fi<0) return -1;

    fo=open(dst, O_WRONLY | O_APPEND | O_CREAT, 0600);
    if(fo<0) {close(fi); return -1;}
    // flock(dst, LOCK_SH);

    if(off > 0)
        lseek(fi, (off_t)off, SEEK_SET);

    while((bytes=read(fi, buf, sizeof(buf)))>0)
    {
         write(fo, buf, bytes);
    }
    // flock(dst, LOCK_UN);
    close(fo);
    close(fi);
    return 0;
}


// return: 0 - ok; otherwise - fail.
static int
hash_partial_file( char *path, size_t sz, unsigned char output[SMHASHLEN] )
{
    int fd;
    size_t n;
    unsigned char buf[1024];

    Fnv64_t fnvseed = FNV1_64_INIT;
    assert(SMHASHLEN == sizeof(fnvseed));

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return 1;

    while(  sz > 0 &&
            (n = read(fd, buf, sizeof(buf))) > 0 )
    {
        if (n > sz) n = sz;
        fnvseed = fnv_64_buf(buf, (int) n, fnvseed);
        sz -= n;
    }
    close(fd);

    if (sz > 0) // file is different
        return 2;

    memcpy(output, (void*) &fnvseed, sizeof(fnvseed));
    return HASHPF_RET_OK;
}

int
post_editscore(xo)
  XO *xo;
{
char fpath[64], genbuf[64];

 // char folder[64];
 // HDR xpost;

  HDR *hdr;

  struct stat st;
  time_t oldmt, newmt;
  off_t oldsz, newsz;
  char SmartMerge = 1,  reset = 0;

  hdr = (HDR *) xo_pool + (xo->pos - xo->top);

#ifdef HAVE_REFUSEMARK
   if (!chkrestrict(hdr))
     return XO_NONE;
#endif

  hdr_fpath(genbuf, xo->dir, hdr);
  sprintf(fpath, "tmp/edit.%s.%ld", cuser.userid, time(NULL));

  //brd_fpath(folder, BN_MODIFY, fn_dir); //���ΦAModify�O�t�~�o�g�s��

  f_cp(genbuf, fpath, O_APPEND);                /* do Copy */
  stat(fpath, &st);        oldsz = st.st_size;
  stat(genbuf, &st);        oldmt = st.st_mtime;


       unsigned char oldsum[SMHASHLEN] = {0}, newsum[SMHASHLEN] = {0};

        if (hash_partial_file(fpath, oldsz, oldsum) != HASHPF_RET_OK)
            SmartMerge = 0;


if(!HAS_PERM(PERM_SYSOP))
{
#ifdef HAVE_LOCKEDMARK
            if(hdr->xmode & POST_LOCKED)
              {
                vmsg("�峹�w�Q��w�A�Ȩ��s��");
                  return XO_NONE;
              }
#endif

}

         //   hdr_stamp(folder, HDR_COPY | 'A', &xpost, fpath);
         //   strcpy(xpost.owner, hdr->owner);
         //   strcpy(xpost.nick, hdr->nick);
         //   strcpy(xpost.title, hdr->title);


      if(sedit(fpath,hdr))/*�ݭn�Ψ�edit.c�̪����c�M�禡*/
      {
              return XO_HEAD;
      }

      //�ɥΤ峹�X�֥\��
      stat(genbuf, &st); newmt = st.st_mtime; newsz = st.st_size;
      if (SmartMerge && newmt == oldmt || newsz < oldsz)
            SmartMerge = 0;
        if (SmartMerge && hash_partial_file(genbuf, oldsz, newsum) != HASHPF_RET_OK)
            SmartMerge = 0;
        if (SmartMerge && memcmp(oldsum, newsum, sizeof(newsum)) != 0)
            SmartMerge = 0;

        if (SmartMerge)
        {
            reset = 1;
            SmartMerge = 0;


            move(b_lines, 0);
            vmsg("\033[1;33m �� �ɮפw�Q�ק�L! ��\033[m");
            outs("�i��۰ʦX�� [Smart Merge]...");

            if (AppendTail(genbuf, fpath, oldsz) == 0)
            {

                oldmt = newmt;
                outs("\033[1;32m�X�֦��\\033[m");

            }
            else
            {

                vmsg("�X�֥���");

            }
        }
        if (oldmt == newmt)
        {
           f_mv(fpath, genbuf);



        }else
        {   return XO_HEAD;//�X�֥��ѴN�٭�峹�����s�F�C
        }

/*
      if (fp = fopen(genbuf, "a"))
        {
           ve_banner(fp, 1);
           fclose(fp);
        }
*/

           change_stamp(xo->dir, hdr);
           currchrono = hdr->chrono;
           rec_put(xo->dir, hdr, sizeof(HDR), xo->pos, cmpchrono);
           post_history(xo, hdr);
           btime_update(currbno);


          // rec_bot(folder, &xpost, sizeof(HDR));
          // btime_update(brd_bno(BN_MODIFY));

   if (reset)
    {
       reset = 0;
       return post_rescore(xo);
    }
  else
    return XO_HEAD;

}

int
post_edit(xo)
  XO *xo;
{
  char fpath[64], genbuf[64];

//081226.cache: �ק�峹�ƥ�
  char folder[64];
  HDR xpost;

  HDR *hdr;
  FILE *fp;

  struct stat st;
  time_t oldmt, newmt;
  off_t oldsz, newsz;
  char SmartMerge = 1, DoRecord = 0, MergeDone = 0, reset = 0;

  hdr = (HDR *) xo_pool + (xo->pos - xo->top);

#ifdef HAVE_REFUSEMARK
   if (!chkrestrict(hdr))
     return XO_NONE;
#endif

  hdr_fpath(genbuf, xo->dir, hdr);
  sprintf(fpath, "tmp/edit.%s.%ld", cuser.userid, time(NULL));

//081226.cache: �ק�峹�ƥ�
  brd_fpath(folder, BN_MODIFY, fn_dir);

  f_cp(genbuf, fpath, O_APPEND);                /* do Copy */
  stat(fpath, &st);        oldsz = st.st_size;
  stat(genbuf, &st);        oldmt = st.st_mtime;

  while (1)
  {
        unsigned char oldsum[SMHASHLEN] = {0}, newsum[SMHASHLEN] = {0};

        if (hash_partial_file(fpath, oldsz, oldsum) != HASHPF_RET_OK)
            SmartMerge = 0;

        if (HAS_PERM(PERM_SYSOP))                        /* �����ק� */
        {

           hdr_stamp(folder, HDR_COPY | 'A', &xpost, fpath);
           strcpy(xpost.owner, hdr->owner);
           strcpy(xpost.nick, hdr->nick);
           strcpy(xpost.title, hdr->title);

           if(!vedit(fpath, 0))
             {
                if(vans("�O�_�d�U�ק�峹�O���H[Y/n] ") != 'n')
                  DoRecord = 1;
             }
           else
             {
               hdr_fpath(fpath, folder, &xpost);
               unlink(fpath);
               return XO_HEAD;
             }
        }//�����ק�
    else if (cuser.userlevel && (is_author(hdr) || (hdr->xmode & POST_WIKI)) )	/* ��@�̭ק� */
        {
          #ifdef HAVE_LOCKEDMARK
            if(hdr->xmode & POST_LOCKED)
              {
                vmsg("�峹�w�Q��w�A�Ȩ��s��");    /* �ԭz�ۭq */
                  return XO_NONE;
              }
          #endif

            hdr_stamp(folder, HDR_COPY | 'A', &xpost, fpath);
            strcpy(xpost.owner, hdr->owner);
            strcpy(xpost.nick, hdr->nick);
            strcpy(xpost.title, hdr->title);

            if (!vedit(fpath, 0))        /* �Y�D�����h�[�W�ק��T */
            {
                DoRecord = 1;
            }
            else
            {
                return XO_HEAD;
            }
        }
        else                /* itoc.010301: ���ѨϥΪ̭ק�(�������x�s)��L�H�o���峹 */
        {
          #ifdef HAVE_REFUSEMARK
            if (!chkrestrict(hdr))
              return XO_NONE;
          #endif
          vedit(fpath, -1);
          return XO_HEAD;
        }

        stat(genbuf, &st); newmt = st.st_mtime; newsz = st.st_size;
        if (SmartMerge && newmt == oldmt || newsz < oldsz)
            SmartMerge = 0;
        if (SmartMerge && hash_partial_file(genbuf, oldsz, newsum) != HASHPF_RET_OK)
            SmartMerge = 0;
        if (SmartMerge && memcmp(oldsum, newsum, sizeof(newsum)) != 0)
            SmartMerge = 0;

        if (SmartMerge)
        {
            reset = 1;
            SmartMerge = 0;

            clear();
            move(b_lines-8, 0);
            outs("\033[1;33m �� �ɮפw�Q�ק�L! ��\033[m\n\n");
            outs("�i��۰ʦX�� [Smart Merge]...\n\n");

            if (AppendTail(genbuf, fpath, oldsz) == 0)
            {

                oldmt = newmt;
                outs("\033[1;32m�X�֦��\\�A�s�ק�(�α���)�w�[�J�z���峹���C\033[m\n");
                MergeDone = 1;
                vmsg("�X�ֹL�{�����ʪ�����ƱN�۰ʭ��s�p��");
            }
            else
            {
                outs("\033[1;31m�۰ʦX�֥��ѡC �Ч�ΤH�u��ʽs��X�֡C\033[m");
                vmsg("�X�֥���");
            }
        }
        if (oldmt != newmt)
        {
            int c = 0;

            clear();
            move(b_lines-8, 0);
            outs("\033[1;33m �� �ɮפw�Q�ק�L! ��\033[m\n\n");

            outs("�i��O�z�b�s�誺�L�{�����H�i�����έפ�C\n\n"
                 "�z�i�H��ܪ����л\\�ɮ�(y)�B���(n)�A\n"
                 " �άO\033[1m���s�s��\033[m(�s��|�Q�K���s���ɮ׫᭱)(e)�C\n");
            c = vans("�n�����л\\�ɮ�/����/���s��? [Y/n/e]");

            if (c == 'n')
                return XO_HEAD;

            if (c == 'e')
            {
                FILE *fp, *src;

                /* merge new and old stuff */
                fp = fopen(fpath, "at");
                src = fopen(genbuf, "rt");

                if(!fp)
                {
                    vmsg("��p�A�ɮפw�l���C");
                    if(src) fclose(src);
                    unlink(fpath); // fpath is a temp file
                    return XO_HEAD;
                }

                if(src)
                {
                    int c = 0;

                    fprintf(fp, MSG_SEPERATOR "\n");
                    fprintf(fp, "�H�U���Q�ק�L���̷s���e: ");
                    fprintf(fp,
                            " (%s)\n",
                            Btime(&newmt));
                    fprintf(fp, MSG_SEPERATOR "\n");
                    while ((c = fgetc(src)) >= 0)
                        fputc(c, fp);
                    fclose(src);

                    // update oldsz, old mt records
                    stat(genbuf, &st); oldmt = st.st_mtime; oldsz = st.st_size;
                }
                fclose(fp);
                continue;
            }
        }

        // let's do something instead to keep the link with hard-linking posts
        {
            FILE *fp = fopen(genbuf, "w");
            f_suck(fp, fpath);
            fclose(fp);
            break;

            //f_mv(fpath, genbuf);
            //break;
        }

    }

    if (DoRecord)
    {
        if (fp = fopen(genbuf, "a"))
        {
            if (hdr->xmode & POST_WIKI)
              {
              char msg[32];
              time_t now;
              struct tm *ptime;
              time(&now);
              ptime = localtime(&now);
              sprintf(msg, "�ѻP %s �s��", "wiki");
              fprintf(fp, "\033[1;36m%13s\033[m \033[1;33m��\033[m�G%-51s %02d/%02d/%02d\n",
                cuser.userid, msg, ptime->tm_year % 100, ptime->tm_mon + 1, ptime->tm_mday);
                fclose(fp);
              }
            else
              {
                ve_banner(fp, 1);
                fclose(fp);
              }
        }

    change_stamp(xo->dir, hdr);
    currchrono = hdr->chrono;
    rec_put(xo->dir, hdr, sizeof(HDR), xo->pos, cmpchrono);
    post_history(xo, hdr);
    btime_update(currbno);

    //�ƻs�� modify �ݪO
    rec_bot(folder, &xpost, sizeof(HDR));
    btime_update(brd_bno(BN_MODIFY));

      /* 090127.cache: wiki���� */
      if (hdr->xmode & POST_WIKI)
        {
          /* bluesway.070323: ��fpath �Ӧs��ذϸ��| */
          gem_fpath(fpath, currboard, fn_dir);
          gem_log(fpath, "Wiki", hdr);
        }

      if (MergeDone)
         vmsg("�X�֧���");

    }
    else
    {
      hdr_fpath(fpath, folder, &xpost);
      unlink(fpath);
    }

  if (reset)
    {
       reset = 0;
       return post_rescore(xo);
    }
  else
    return XO_HEAD;        /* itoc.021226: XZ_POST �M XZ_XPOST �@�� post_edit() */

}


void
header_replace(xo, hdr)		/* itoc.010709: �ק�峹���D���K�ק鷺�媺���D */
  XO *xo;
  HDR *hdr;
{
  FILE *fpr, *fpw;
  char srcfile[64], tmpfile[64], buf[ANSILINELEN];

  hdr_fpath(srcfile, xo->dir, hdr);
  strcpy(tmpfile, "tmp/");
  strcat(tmpfile, hdr->xname);
  f_cp(srcfile, tmpfile, O_TRUNC);

  if (!(fpr = fopen(tmpfile, "r")))
    return;

  if (!(fpw = fopen(srcfile, "w")))
  {
    fclose(fpr);
    return;
  }

  fgets(buf, sizeof(buf), fpr);		/* �[�J�@�� */
  fputs(buf, fpw);

  fgets(buf, sizeof(buf), fpr);		/* �[�J���D */
  if (!str_ncmp(buf, "��", 2))		/* �p�G�� header �~�� */
  {
    strcpy(buf, buf[2] == ' ' ? "��  �D: " : "���D: ");
    strcat(buf, hdr->title);
    strcat(buf, "\n");
  }
  fputs(buf, fpw);

  while(fgets(buf, sizeof(buf), fpr))	/* �[�J��L */
    fputs(buf, fpw);

  fclose(fpr);
  fclose(fpw);
  f_rm(tmpfile);
}


static int
post_title(xo)
  XO *xo;
{
  HDR *fhdr, mhdr;
  int pos, cur;

  if (!cuser.userlevel)	/* itoc.000213: �קK guest �b sysop �O����D */
    return XO_NONE;

  pos = xo->pos;
  cur = pos - xo->top;
  fhdr = (HDR *) xo_pool + cur;
  memcpy(&mhdr, fhdr, sizeof(HDR));

  if (!is_author(&mhdr) && !HAS_PERM(PERM_ALLBOARD))
    return XO_NONE;

  vget(b_lines, 0, "���D�G", mhdr.title, TTLEN + 1, GCARRY);

  if (HAS_PERM(PERM_ALLBOARD))  /* itoc.000213: ��@�̥u�����D */
  {
    vget(b_lines, 0, "�@�̡G", mhdr.owner, 73 /* sizeof(mhdr.owner) */, GCARRY);
		/* Thor.980727: sizeof(mhdr.owner) = 80 �|�W�L�@�� */
    vget(b_lines, 0, "�ʺ١G", mhdr.nick, sizeof(mhdr.nick), GCARRY);
    vget(b_lines, 0, "����G", mhdr.date, sizeof(mhdr.date), GCARRY);
  }

  if (memcmp(fhdr, &mhdr, sizeof(HDR)) && vans(msg_sure_ny) == 'y')
  {
    memcpy(fhdr, &mhdr, sizeof(HDR));
    currchrono = fhdr->chrono;
    rec_put(xo->dir, fhdr, sizeof(HDR), xo->key == XZ_XPOST ? fhdr->xid : pos, cmpchrono);

    move(3 + cur, 0);
    post_item(++pos, fhdr);

    /* itoc.010709: �ק�峹���D���K�ק鷺�媺���D */
    header_replace(xo, fhdr);
  }
  return XO_FOOT;
}


/* ----------------------------------------------------- */
/* �B�~�\��Gwrite / score				 */
/* ----------------------------------------------------- */


int
post_write(xo)			/* itoc.010328: ��u�W�@�̤��y */
  XO *xo;
{
  if (HAS_PERM(PERM_PAGE))
  {
    HDR *hdr;
    UTMP *up;

    hdr = (HDR *) xo_pool + (xo->pos - xo->top);

    if (!(hdr->xmode & POST_INCOME) && (up = utmp_seek(hdr)))
      do_write(up);
  }
  return XO_NONE;
}


#ifdef HAVE_SCORE

static int curraddscore;

static void
addscore(hdd, ram)
  HDR *hdd, *ram;
{

if (curraddscore)
{
  hdd->xmode |= POST_SCORE;
  hdd->stamp = ram->stamp;
  if (curraddscore > 0)
  {
    if (hdd->score < 127)
     /* ������ .DIR ���� score ��s�A���� XO �̭��� score �O�O���h�� */
      hdd->score += curraddscore;
  }
  else
  {
    if (hdd->score > -127)
     /* ������ .DIR ���� score ��s�A���� XO �̭��� score �O�O���h�� */
      hdd->score += curraddscore;
  }
}
}

/* 080813.cache: �O�D�i�H�M�����媺������ */

static int
post_resetscore(xo)
  XO *xo;
{
  if (bbstate & STAT_BOARD) /* bm only */
  {
    HDR *hdr;
    int pos, cur, xmode, score, pm;
    char ans[3];

    pos = xo->pos;
    cur = pos - xo->top;
    hdr = (HDR *) xo_pool + cur;

    xmode = hdr->xmode;

if(hdr->xmode & POST_SCORE)
  {
    switch (vans("�������]�w 1)�ۭq 2)�M�� 3)���s�p�� [Q] "))
    {
    case '1':
      if (!vget(b_lines, 0, "�п�J�Ʀr�G", ans, 3, DOECHO))
        return XO_FOOT;
      pm = vans("�п�ܥ��t 1)�� 2)�t [Q] ");
      if (pm =='1')
        {
          score = atoi(ans);
        }
      else if (pm == '2')
        {
          score = atoi(ans);
          score = -score;
        }
      else
        return XO_FOOT;
      if (score > 99 || score < -99)
        return XO_FOOT;
      else if (score < 41 && score > 1) {
        vmsg("�վ㥢��");
        return XO_FOOT;
      }
      hdr->xmode = xmode |= POST_SCORE; /* ���i��L���� */
      hdr->score = score;
      break;

    case '2':
      hdr->xmode = xmode & ~POST_SCORE; /* �M���N���ݭnPOST_SCORE�F */
      hdr->score = 0;
      break;

    case '3':
      return post_rescore(xo);
      break;

    default:
      return XO_FOOT;
    }

    currchrono = hdr->chrono;
    rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ?
      hdr->xid : pos, cmpchrono);
    move(3 + cur, 7);
    outc(post_attr(hdr));

  return XO_LOAD;
  }

else
  {
    vmsg("���g�峹�S������");
    return XO_FOOT;
  }

  }
  else
    {
      vmsg("�z���v�������I");
      return XO_FOOT;
    }
}

int
post_score(xo)
  XO *xo;
{
  static time_t next2 = 0;   /* 080417.cache: �U���i����ɶ� */
  static time_t next = 0;    /* 080417.cache: �����ܵ��Ѯɶ� */
  HDR *hdr;
  int pos, cur, ans, vtlen, maxlen , cando, eof, plus, cmt, userno;
  char *dir, *userid, *verb, fpath[64], reason[80], defverb[2], pushverb, vtbuf[18];
  FILE *fp;
#ifdef HAVE_ANONYMOUS
  char uid[IDLEN + 1];
#endif

  if (!(currbattr & BRD_SCORE) || !cuser.userlevel || !(bbstate & STAT_POST))/* ������P�o��峹 */
    return XO_NONE;

 /*�q���٦��h�֬�~�i�H����*/
 if ((ans = next - time(NULL)) > 0)
 {
   sprintf(fpath, "�٦� %d ��~�i�H���פ峹��", ans);
   vmsg(fpath);
   return XO_FOOT;
 }

  pos = xo->pos;
  cur = pos - xo->top;
  hdr = (HDR *) xo_pool + cur;

#ifdef HAVE_LOCKEDMARK
  if((hdr->xmode & POST_LOCKED) && !(bbstate & STAT_BM))  /* ��w�峹������� */
    {
      vmsg("�Q��w�峹�������");
      return XO_NONE;
    }
#endif

#ifdef HAVE_REFUSEMARK
  if (!chkrestrict(hdr))
    return XO_NONE;
#endif

 if (next2 < time(NULL))
 {

//��l�ƥH���U�@
   cando = 0;
   plus = 0;
   cmt = 0; /*090710.syntaxerror: �W�[���Ѫ��X��*/

/*080419.cache: �Ȯɭק令�o��*/
//switch (ans = vans("�� ���� 1)���� 2)�N�� 3)�۩w�� 4)�۩w�N 5)����(�|���}��)�H[Q] "))
  switch (ans = vans("�� ���� 1)���� 2)�N�� 3)���� �H[Q] "))
  {
  case '1':
    verb = "\033[1;31m��\033[m";
    vtlen = 2;
    cando = 1;
    plus = 1;
    break;

  case '2':
    verb = "\033[1;32m�N\033[m";
    vtlen = 2;
    cando = 1;
    break;

  case '3':
    verb = "\033[1;33m��\033[m";
    cmt = 1;
    cando = 0;
    break;

  case '4':
    pushverb = vget(b_lines, 0, "�п�J�ۭq�������ʵ��G", defverb, 3, DOECHO);
    eof = strlen(defverb);
    if(eof<2)
      {
        zmsg("�ʵ������@�Ӥ���r���Ϊ̨�ӭ^��r��");
        return XO_FOOT;
      }
    sprintf(verb = vtbuf, "\033[1;31m%s\033[m", defverb);
    vtlen = 2;
    cando = 1;
    plus = 1;
    break;

  case '5':
     pushverb = vget(b_lines, 0, "�п�J�ۭq���t���ʵ��G", defverb, 3, DOECHO);
     eof = strlen(defverb);
     if(eof<2)
       {
         zmsg("�ʵ������@�Ӥ���r���Ϊ̨�ӭ^��r��");
         return XO_FOOT;
       }
     sprintf(verb = vtbuf, "\033[1;32m%s\033[m", defverb);
     vtlen = 2;
     cando = 1;
     break;
//�ª� itoc �ۭq�ʵ�
//  case '3':
//  case '4':
//    if (!vget(b_lines, 0, "�п�J�ۭq���e�G", fpath, 3, DOECHO))
//      return XO_FOOT;
//    vtlen = strlen(fpath) + 2;
//    sprintf(verb = vtbuf, "%cm  %s", ans - 2, fpath);
//    break;
  default:
    return XO_FOOT;
  }

 }
 else
 {
   cando = 0;
   cmt = 1;
   verb = "\033[1;33m��\033[m";
 }

#ifdef HAVE_ANONYMOUS
  if (currbattr & BRD_ANONYMOUS)
    maxlen = 64 - IDLEN - vtlen + 2;
  else
#endif
    //maxlen = 64 - strlen(cuser.userid) - vtlen;
    maxlen = 64 - 10 - vtlen;
//�ﱼstrlen(cuser.userid)���������
  if (!vget(b_lines, 0, "�� �� �J �z �� �G ", reason, maxlen, DOECHO))
    return XO_FOOT;

#ifdef HAVE_ANONYMOUS
  if (currbattr & BRD_ANONYMOUS)
  {
    userid = uid;
    if (!vget(b_lines, 0, "�п�J�z�Q�Ϊ�ID�A�]�i������[Enter]�A�άO��[r]�ίu�W�G", userid, IDLEN, DOECHO))
      userid = STR_ANONYMOUS;
    else if (userid[0] == 'r' && userid[1] == '\0')
      userid = cuser.userid;
    else
      strcat(userid, ".");		/* �۩w���ܡA�̫�[ '.' */
    //maxlen = 64 - strlen(userid) - vtlen;
    maxlen = 64 - 10 - vtlen;
//�ﱼstrlen(userid)���������
  }
  else
#endif
    userid = cuser.userid;

  dir = xo->dir;
  hdr_fpath(fpath, dir, hdr);

  if (fp = fopen(fpath, "a"))
  {
    time_t now;
    struct tm *ptime;

    time(&now);
    ptime = localtime(&now);

    fprintf(fp, "\033[1;36m%13s %s�G%-*s%02d/%02d/%02d\n",
      userid, verb, maxlen, reason,
      ptime->tm_year % 100, ptime->tm_mon + 1, ptime->tm_mday);
    fclose(fp);
  }

  curraddscore = 0;

 if (cando > 0)
   {
   if (plus == 1/*(ans - '0') & 0x01*/)	/* �[�� */
     {
       if (hdr->score < 127) { /* cache.080730: �z���峹�n���N20���H�W�~�|���� */
         curraddscore = 1;

         if ((hdr->score == 19) && !(currbattr & BRD_PUSH)) { /* cache.090817: ����D�s���ݪO */
           if((userno = acct_userno(hdr->owner)) > 0)
             {
                extern UCACHE *ushm;
                int online = 0;
                char buf[20];
                UTMP *uentp, *uceil;
                uentp = ushm->uslot;
                uceil = (void *) uentp + ushm->offset;

                do {
                     if(uentp->userno == userno)/* �b�u�W�A�g�J utmp-> */
                       {
                       uentp->goodart++;
                       online = 1;
                       sprintf(buf, "�@�� %s �u��g�� +1", uentp->userid );
                       }
                } while (++uentp <= uceil);

                if(online)
                   vmsg(buf);
                else /* ���b�u�W�A�g�J .ACCT */
                  {
                     ACCT acct;
                     if(acct_load(&acct, hdr->owner) >= 0)
                       {
                          acct.goodart++;
                          acct_save(&acct);
                       sprintf(buf, "�@�� %s �u��g�� +1", acct.userid );
                          vmsg(buf);
                       }
                   }//online
             }//if userno
          }//hdr->score == 19
        }//hdr->score < 127
     }//if
   else				/* ���� */
     {
       if (hdr->score > -127) { /* cache.080730: �z���峹�n���N20���H�W�~�|���� */
         curraddscore = -1;

         if ((hdr->score == 20) && !(currbattr & BRD_PUSH)) { /* cache.090817: ����D�s���ݪO */
           if((userno = acct_userno(hdr->owner)) > 0)
             {
                extern UCACHE *ushm;
                int online = 0;
                char buf[20];
                UTMP *uentp, *uceil;
                uentp = ushm->uslot;
                uceil = (void *) uentp + ushm->offset;

                do {
                     if(uentp->userno == userno)/* �b�u�W�A�g�J utmp-> */
                       {
                       uentp->goodart--;
                       online = 1;
                       sprintf(buf, "�@�� %s �u��g�� -1", uentp->userid );
                       }
                } while (++uentp <= uceil);

                if(online)
                   vmsg(buf);
                else /* ���b�u�W�A�g�J .ACCT */
                  {
                     ACCT acct;
                     if(acct_load(&acct, hdr->owner) >= 0)
                       {
                          acct.goodart--;
                          acct_save(&acct);
                       sprintf(buf, "�@�� %s �u��g�� -1", acct.userid );
                          vmsg(buf);
                       }
                   }//online
             }//if userno
          }//hdr->score == 20
       }//hdr->score > -127
     }//else
   }//cando

  if (currbattr & BRD_PUSH)
    {
      next2 = time(NULL) + 0;       /* 080731.cache: �i�H�s�� */
      next = time(NULL)  + 0;
    }

  else
    {
      next2 = time(NULL) + 60;      /* �C 60 ���i���N��@�� */
      next = time(NULL)  + 10;      /* �C 10 ���i���פ@�� */
    }

  if (curraddscore)
  {
    change_stamp(xo->dir, hdr);
    currchrono = hdr->chrono;
    rec_ref(dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ?
    hdr->xid : pos, cmpchrono, addscore);
    post_history(xo, hdr);
    btime_update(currbno);
    return XO_LOAD;

  }else if(cmt)  //�O�����Ѫ��ɶ�
  {
    change_stamp(xo->dir, hdr);
    currchrono = hdr->chrono;
    rec_put( dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ?
    hdr->xid : pos, cmpchrono);
    post_history(xo, hdr);
    btime_update(currbno);
    return XO_LOAD;
  }
  return XO_FOOT;

}
#endif	/* HAVE_SCORE */

int
post_rescore(xo)
  XO *xo;
{
  int start_quote, counta, countb, pos, cur, userno, ori=0;
  HDR *hdr;
  char fpath[64], buf[ANSILINELEN], *reason1, *reason2, *reason3;
  FILE *fpr;

  reason1 = "[1;31m";
  reason2 = "[1;32m";
  reason3 = "[m�G";

  counta = 0;
  countb = 0;

  hdr = (HDR *) xo_pool + (xo->pos - xo->top);
  hdr_fpath(fpath, xo->dir, hdr);

  if (!(fpr = fopen(fpath, "r")))
    {
       vmsg("����ƭ��s�p�⥢��");
       return XO_HEAD;
    }

  start_quote = 0;
  while (fgets(buf, sizeof(buf), fpr))
  {
    if (!start_quote)
    {
      if (!strcmp(buf, "--\n"))
        start_quote = 1;
    }
    else
    {
      if (strstr(buf, reason1) && strstr(buf, reason3))
      {
        counta++;
      }
      else if (strstr(buf, reason2) && strstr(buf, reason3))
      {
        countb++;
      }
    }
  }
  fclose(fpr);

  pos = xo->pos;
  ori = hdr->score;

  if((!(counta == 0) && !(countb == 0)) || (!(counta == 0) && (countb == 0)) || ((counta == 0) && !(countb == 0)))
  {
      cur = pos - xo->top;
      hdr = (HDR *) xo_pool + cur;

      hdr->xmode |= POST_SCORE; /* ���i��L���� */
      hdr->score = 0;
      hdr->score = counta-countb;
      currchrono = hdr->chrono;
      rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono);
      move(3 + cur, 7);
      outc(post_attr(hdr));

         /* 090817.cache: �P�B�ˬd�u�H�� */
         if (((counta-countb) == 20) && !(ori == 20)) { //���C�~�P���v
           if((userno = acct_userno(hdr->owner)) > 0)
             {
                extern UCACHE *ushm;
                int online = 0;
                char buf[20];
                UTMP *uentp, *uceil;
                uentp = ushm->uslot;
                uceil = (void *) uentp + ushm->offset;

                do {
                     if(uentp->userno == userno)/* �b�u�W�A�g�J utmp-> */
                       {
                       uentp->goodart++;
                       online = 1;
                       sprintf(buf, "�@�� %s �u��g�� +1", uentp->userid );
                       }
                } while (++uentp <= uceil);

                if(online)
                   vmsg(buf);
                else /* ���b�u�W�A�g�J .ACCT */
                  {
                     ACCT acct;
                     if(acct_load(&acct, hdr->owner) >= 0)
                       {
                          acct.goodart++;
                          acct_save(&acct);
                       sprintf(buf, "�@�� %s �u��g�� +1", acct.userid );
                          vmsg(buf);
                       }
                   }//online
             }//if userno
          }//hdr->score == 20
  }
  else
  {
      cur = pos - xo->top;
      hdr = (HDR *) xo_pool + cur;

      hdr->xmode &= ~POST_SCORE; /* cache.090612: �j��]���L���� */
      hdr->score = 0;
      currchrono = hdr->chrono;
      rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono);
  }

  vmsg("����ƭ��s�p�⧹��");

  return XO_HEAD;
}


/*cache.080520:�ݪO��T*/
/* ----------------------------------------------------- */
/* �B�~�\��Gwrite / score                               */
/* ----------------------------------------------------- */

static int
post_showBRD_setting(xo)
  XO *xo;
{
  char *blist;
  int bno;

  bno = brd_bno(currboard);
  blist = (bshm->bcache + bno)->BM; /* �˵��O�D�M��� */

  grayout(0, b_lines - 15, GRAYOUT_DARK);

  move(b_lines - 15, 0);
  clrtobot();  /* �קK�e���ݯd */

  prints("\033[1;34m"MSG_BLINE"\033[m");
  prints("\n\033[1;33;44m \033[37m�ݪO�]�w�θ�T�d�ߡG %*s \033[m\n", 55,"");

  prints("\n \033[1;37m��\033[m �ݪO�ʽ� - %s",
  (currbattr & BRD_NOTRAN) ? "�����ݪO (�����ݪO�w�]�������K��)" : "\033[1;33m��H�ݪO\033[m (��H�ݪO�w�]���󯸶K��)");
  prints("\n \033[1;37m��\033[m �O���g�� - %s",
  (currbattr & BRD_NOCOUNT) ? "�����O�� (�峹�ƥؤ��|�C�J�έp)" : "\033[1;33m�O��\033[m     (�峹�ƥرN�|�C�J�έp)");
  prints("\n \033[1;37m��\033[m �������D - %s",
  (currbattr & BRD_NOSTAT) ? "�����O�� (�ݪO���|�C�J�������D�έp)" : "\033[1;33m�O��\033[m     (�ݪO�N�|�C�J�������D�έp)");
  prints("\n \033[1;37m��\033[m �벼���G - %s",
  (currbattr & BRD_NOVOTE) ? "�����O�� (�벼���G���|���G�b record �O)" : "\033[1;33m�O��\033[m     (�벼���G�N�|���G�b \033[1;32mrecord\033[m �O)");
  prints("\n \033[1;37m��\033[m �ΦW�\\�� - %s",
  (currbattr & BRD_ANONYMOUS) ? "\033[1;33m�}��\033[m     (�o��峹�i�H�ΦW)" : "����     (�o��峹����ΦW)");
  prints("\n \033[1;37m��\033[m ����\\�� - %s",
  (currbattr & BRD_SCORE) ? "\033[1;33m�}��\033[m     (�i�H���N��)" : "����     (�u��^��)");
  prints("\n \033[1;37m��\033[m �s���]�w - %s",
  (currbattr & BRD_PUSH) ? "\033[1;33m�}��\033[m     (���N��/���פ峹�����ɶ�����)" : "����     (�A�����N��n����\033[1;32m 60 \033[m��/���פ峹�n����\033[1;32m 10 \033[m��)");
  prints("\n \033[1;37m��\033[m �峹���O - %s",
  (currbattr & BRD_PREFIX) ? "\033[1;33m�}��\033[m     (�o��ɥi��ܪO�D�q�w���峹���O)" : "����     (�o��ɵL�峹���O�i�ѿ��)");
  prints("\n \033[1;37m��\033[m �i�O�O�� - %s",
  (currbattr & BRD_USIES) ? "\033[1;33m�}��\033[m     (�O���ϥΪ̶i�J�ݪO����)" : "����     (���|�O���ϥΪ̶i�J�ݪO����)");

  if (cuser.userlevel)
  {
    prints("\n\n �ݪO�W�� [\033[1;33m%s\033m] �O�D [\033[1;33m%s\033[m]", currboard, blist[0] > ' ' ? blist : "�x�D��");
//�Ӫ����O�D�W��|�ɭP�o��W�X�@��G���Ѥ�
//    prints(" �z�ثe %s ���ݪO���޲z�v��",
//      ((cuser.userlevel & PERM_BM) && blist[0] > ' ' && is_bm(blist,
//        cuser.userid)) ? "\033[1;33m�֦�\033[m" : "\033[1;36m�S��\033[m");
  }
  else
  {
    prints("\n\n �ݪO�W�� [\033[1;33m%s\033m] �O�D [\033[1;33m%s\033[m]", currboard, blist[0] > ' ' ? blist : "�x�D��");
//    prints(" �z�ثe������ \033[1;36mguest\033[m �X��");
  }
  vmsg(NULL);

  return XO_HEAD;
}

/*cache.080520:�[�ݤ峹�ݩ�*/
static int
post_state(xo)
  XO *xo;
{
  HDR *ghdr;
  char fpath[64], *dir;
  struct stat st;

  ghdr = (HDR *) xo_pool + (xo->pos - xo->top);

  dir = xo->dir;
  hdr_fpath(fpath, dir, ghdr);

  if (HAS_PERM(PERM_ALLBOARD))
    {

      grayout(0, b_lines - 11, GRAYOUT_DARK);
      move(b_lines - 11, 0);
      clrtobot();

      prints("\033[1;34m"MSG_BLINE"\033[m");
      prints("\n\033[1;33;44m \033[37m�峹�N�X�θ�T�d�ߡG %*s \033[m", 55,"");
      outs("\n\n \033[1;37m��\033[m �峹����: ");
      outs(dir);
      outs("\n \033[1;37m��\033[m �峹�N�X: ");
      outs("\033[1;32m");
      outs(ghdr->xname);
      outs("\033[m");
      outs("\n \033[1;37m��\033[m �峹��m: ");
      outs(fpath);

      if (!stat(fpath, &st))
        prints("\n \033[1;37m��\033[m �̫�s��: %s\n \033[1;37m��\033[m �ɮפj�p: \033[1;32m%d\033[m bytes", Btime(&st.st_mtime), st.st_size);

      prints("\n\n �o�@�g�峹�֭p���� \033[1;33m%d\033[m ��", ((int)st.st_size)/20);
    }
  else if (!(cuser.userlevel))
    {
       vmsg("�z���v������");
       return XO_HEAD;
    }
  else
    {

      grayout(0, b_lines - 9, GRAYOUT_DARK);
      move(b_lines - 9, 0);
      clrtobot();

      prints("\033[1;34m"MSG_BLINE"\033[m");
      prints("\n\033[1;33;44m \033[37m�峹�N�X�θ�T�d�ߡG %*s \033[m", 55,"");
      outs("\n\n \033[1;37m��\033[m �峹�N�X: ");
      outs("\033[1;32m");
      outs(ghdr->xname);
      outs("\033[m");

      if (!stat(fpath, &st))
        prints("\n \033[1;37m��\033[m �̫�s��: %s\n \033[1;37m��\033[m �ɮפj�p: \033[1;32m%d\033[m bytes", Btime(&st.st_mtime), st.st_size);

      prints("\n\n �o�@�g�峹�֭p���� \033[1;33m%d\033[m ��", ((int)st.st_size)/20);
    }

  vmsg(NULL);

  return XO_HEAD;
}

#ifdef HAVE_SCORE
static int
post_delscore(xo)
  XO *xo;
{
  int start_quote = 0, len, nomodify = 0;
  HDR *hdr;
  char fpath[64], fnew[64], userid[32], reason[50], buf[ANSILINELEN],log[ANSILINELEN];
  char *fdlog="run/delscore.log";
  FILE *fpr, *fpw ,*fpL=0;
  struct tm *ptime;
  time_t now;

  if (!cuser.userlevel || !(bbstate & STAT_POST))       /* �������P�o��峹 */
    return XO_NONE;

  if (!(bbstate & STAT_BOARD))
  {
    vmsg("�z���v�������I");
    return XO_FOOT;
  }

  if (!vget(b_lines, 0, "�n�R��������(����r)�G", reason, 50, DOECHO))
    return XO_FOOT;

  if ( !strcmp(reason, "Modify") )
    nomodify = 1;

  hdr = (HDR *) xo_pool + (xo->pos - xo->top);
  hdr_fpath(fpath, xo->dir, hdr);

  if (!(fpr = fopen(fpath, "r")))
    return XO_FOOT;

  time(&now);
  ptime = localtime(&now);

  sprintf(fnew, "%s.new", fpath);
  fpw = fopen(fnew, "w");


  start_quote = 0;
  while (fgets(buf, sizeof(buf), fpr)){
    if (!start_quote){
      if (!strncmp(buf, "\033[1;32m�� Origin:\033[m", 20)) //����
        start_quote = 1;
    }
    else {
                /*�P�_�O�_���ӨϥΪ̪����大�覡���ݱ��Q*/
        if ((bbstate & STAT_BOARD) || !strncmp(buf, userid, len)) { //�O�D�Φۤv������

	    if (strstr(buf, reason)){ // +35: ���ʨ��r��m
	    	if (nomodify && !strncmp(buf, "\033[1;32m�� Modify:\033[m \033[1;37m", 28))
	      	    continue;
    		else if (!strncmp(buf, "\033[1;36m", 7) &&
    			(!strncmp(buf + 20, " \033[1;31m", 8) || !strncmp(buf+20, " \033[1;32m", 8) || !strncmp(buf+20, " \033[1;33m", 8))){
    			/*����榡: 0 - 6 : \033[1;36m
		   	            7 - 19 : ID
		    		    20 - 27: ' ' + \033[1;3(1,2,3)m */
	  	   if(!fpL)
	  	   {
                sprintf(log,"[DELETE][BOARDNAME: %s ][XNAME: %s ][BY %s %02d:%02d ]\n", currboard, hdr->xname, cuser.userid,ptime->tm_hour,ptime->tm_min);
                f_cat(fdlog,log);   // �C���R��log�u�g�J�@��
                fpL = fopen(fdlog, "a");
           }

		   fputs(buf, fpL);

                   sprintf(buf, "\033[1;36m%13s\033[m \033[1;33m��\033[m�G"   /*���榡�ȥ����n���*/
                     "%-51s %02d/%02d/%02d\n",
                   cuser.userid, "�����פw�Q�R��",
                   ptime->tm_year % 100, ptime->tm_mon + 1, ptime->tm_mday);
    	         }
			    /*����P�R�����������Y���X����榡�A�]ID�᭱�����s��ANSI����X*/
	     }
	}
    }
    fputs(buf, fpw);
  }

 if(fpL)
  fclose(fpL);

  fclose(fpw);
  fclose(fpr);

  unlink(fpath);
  rename(fnew, fpath);

  return post_rescore(xo);
}
#endif

static int
post_aid(xo)
  XO *xo;
{
  char *tag, *query, aid[9];
  int currpos, pos, max, match = 0;
  HDR *hdr;

  /* �O�s�ثe�Ҧb����m */
  currpos = xo->pos;
  /* �����ݪO�峹�`�ơF�s�K��AID�O���������i��b�䤤 */
  max = xo->max;

  /* �Y�S���峹�Ψ�L�ҥ~���p */
  if (max <= 0)
    return XO_FOOT;

  /* �ШD�ϥΪ̿�J�峹�N�X(AID) */
  if (!vget(b_lines, 0, "�п�J�峹�N�X(AID)�G #", aid, sizeof(aid), DOECHO))
    return XO_FOOT;
  query = aid;

  for (pos = 0; pos < max; pos++)
  {
    xo->pos = pos;
    xo_load(xo, sizeof(HDR));
    /* �]�wHDR��T */
    hdr = (HDR *) xo_pool + (xo->pos - xo->top);
    tag = hdr->xname;
    /* �Y���������峹�A�h�]�wmatch�ø��X */
    if (!strcmp(query, tag))
    {
      match = 1;
      break;
    }
  }

  /* �S���h��_xo->pos����ܴ��ܤ�r */
  if (!match)
  {
    zmsg("�䤣��峹�A�O���O����ݪO�F�O�H");
    xo->pos = currpos;  /* ��_xo->pos���� */
  }

  return post_load(xo);
}

static int
post_help(xo)
  XO *xo;
{

  xo_help("post");
 /* return post_head(xo); */
  return XO_HEAD;		/* itoc.001029: �P xpost_help �@�� */
}


KeyFunc post_cb[] =
{
  XO_INIT, post_init,
  XO_LOAD, post_load,
  XO_HEAD, post_head,
  XO_BODY, post_body,

  'r', post_browse,
  's', post_switch,
  KEY_TAB, post_gem,
  'z', post_gem,

  'y', post_reply,
  'd', post_delete,
  'v', post_visit,
  'x', post_cross,		/* �b post/mbox �����O�p�g x ��ݪO�A�j�g M ��ϥΪ� */
  'M', post_forward,
  't', post_tag,
  'E', post_edit,
  'T', post_title,
  'm', post_mark,
  '_', post_bottom,
  'D', post_rangedel,
  'i', post_showBRD_setting,  /* cache.080520: �ݪO��T��ܨ�[I]nfo���r ����n�O :p */
  'q', post_state,            /* cache.080520: �[�ݤ峹�ݩ� */
  'O', XoBM_Refuse_pal,       /* cache.090124: �[�K�峹�i���W�� */
  '#', post_aid,              /* cache.090612: �H�峹�N�X(AID)�ֳt�M�� */

#ifdef HAVE_SCORE
  'X', post_score,
  '%', post_editscore,
  'Z', XoXscore,                /* itoc.030530: hpo14.080814: �j�M������� */
  'o', post_resetscore,         /* cache.090127: �ᤩ����S�w�ƥ� */
  'K', post_delscore,           /* cache.090207: �M���S�w���� */
#endif

  'w', post_write,

  'b', post_memo,
  'c', post_copy,
  'g', gem_gather,

  Ctrl('P'), post_add,
  Ctrl('D'), post_prune,
  Ctrl('Q'), xo_uquery,
  Ctrl('O'), xo_usetup,
  Ctrl('S'), post_done_mark,    /* qazq.030815: ����B�z�����аO��*/
  Ctrl('B'), depart_post,       /* cache.080812: ����峹�ܬY�S�w���O */
  Ctrl('W'), post_wiki,         /* cache.090127: wiki */

#ifdef HAVE_REFUSEMARK
  Ctrl('Y'), post_refuse,
#endif

#ifdef HAVE_LABELMARK
  'n', post_label,
  Ctrl('N'), post_delabel,
#endif

#ifdef HAVE_LOCKEDMARK
  'l', post_locked, /* hpo14.20070513: �峹��w�аO�I */
#endif

  'B' | XO_DL, (void *) "bin/manage.so:post_manage",
  'R' | XO_DL, (void *) "bin/vote.so:vote_result",
  'V' | XO_DL, (void *) "bin/vote.so:XoVote",

#ifdef HAVE_TERMINATOR
  Ctrl('X') | XO_DL, (void *) "bin/manage.so:post_terminator",
#endif

  '~', XoXselect,		/* itoc.001220: �j�M�@��/���D */
  'S', XoXsearch,		/* itoc.001220: �j�M�ۦP���D�峹 */
  'a', XoXauthor,		/* itoc.001220: �j�M�@�� */
  '/', XoXtitle,		/* itoc.001220: �j�M���D */
  'f', XoXfull,			/* itoc.030608: ����j�M */
  'G', XoXmark,			/* itoc.010325: �j�M mark �峹 */
  'L', XoXlocal,		/* itoc.010822: �j�M���a�峹 */

#ifdef HAVE_XYNEWS
  'u', XoNews,			/* itoc.010822: �s�D�\Ū�Ҧ� */
#endif

  'h', post_help
};


KeyFunc xpost_cb[] =
{
  XO_INIT, xpost_init,
  XO_LOAD, xpost_load,
  XO_HEAD, xpost_head,
  XO_BODY, post_body,		/* Thor.980911: �@�ΧY�i */

  'r', xpost_browse,
  'y', post_reply,
  't', post_tag,
  'x', post_cross,
  'M', post_forward,
  'c', post_copy,
  'g', gem_gather,
  'm', post_mark,
  'd', post_delete,		/* Thor.980911: ��K�O�D */
  'E', post_edit,		/* itoc.010716: ���� XPOST ���i�H�s����D�B�峹�A�[�K */
  'T', post_title,
  'i', post_showBRD_setting,  /* cache.080520:�ݪO��T��� */ /* ��[I]nfo���r ����n�O :p */
  'q', post_state,            /*cache.080520:�[�ݤ峹�ݩ�*/
#ifdef HAVE_SCORE
  'X', post_score,
  'Z', XoXscore,              /* itoc.030530: hpo14.080814: �j�M������� */
#endif
  'w', post_write,
#ifdef HAVE_REFUSEMARK
  Ctrl('Y'), post_refuse,
#endif
#ifdef HAVE_LABELMARK
  'n', post_label,
#endif

#ifdef HAVE_LOCKEDMARK
  'l', post_locked,     /* hpo14.20070513: �峹��w�аO�I */
#endif

  '~', XoXselect,
  'S', XoXsearch,
  'a', XoXauthor,
  '/', XoXtitle,
  'f', XoXfull,
  'G', XoXmark,
  'L', XoXlocal,

  Ctrl('P'), post_add,
  Ctrl('D'), post_prune,
  Ctrl('Q'), xo_uquery,
  Ctrl('O'), xo_usetup,
  Ctrl('S'), post_done_mark,    /* qazq.030815: ����B�z�����аO��*/

  'h', post_help		/* itoc.030511: �@�ΧY�i */
};


#ifdef HAVE_XYNEWS
KeyFunc news_cb[] =
{
  XO_INIT, news_init,
  XO_LOAD, news_load,
  XO_HEAD, news_head,
  XO_BODY, post_body,

  'r', XoXsearch,

  'h', post_help		/* itoc.030511: �@�ΧY�i */
};
#endif	/* HAVE_XYNEWS */
