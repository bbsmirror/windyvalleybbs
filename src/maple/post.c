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


extern int wordsnum;		/* itoc.010408: 計算文章字數 */
extern int TagNum;
extern char xo_pool[];
extern char brd_bits[];


#ifdef HAVE_ANONYMOUS
extern char anonymousid[];	/* itoc.010717: 自定匿名 ID */
#endif


int
cmpchrono(hdr)
  HDR *hdr;
{
  return hdr->chrono == currchrono;
}


/* 090127.cache: 加密文章可見名單 */

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
RefusePal_kill(board, hdr)   /* amaki.030322: 用來砍名單小檔 */
  char *board;
  HDR *hdr;
{
  char fpath[64];

  RefusePal_fpath(fpath, board, 'C', hdr);
  unlink(fpath);
  RefusePal_fpath(fpath, board, 'R', hdr);
  unlink(fpath);
}

int          /* 1:在可見名單上 0:不在可見名單上 */
RefusePal_belong(board, hdr)
  char *board;
  HDR *hdr;
{
  int fsize;
  char fpath[64];
  int *fimage;

  RefusePal_fpath(fpath, board, 'R', hdr);
  if (fimage =(int *)f_img(fpath, &fsize))  /*修正傳遞資料型態不符的warning*/
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
/* FinFunnel.20110219: 原本直接從xo_pool取hdr的方式，每頁的第一篇文章hdr內容
   經過xover後似乎會被動到導致找不到文章編號，暫時採用複製一份hdr的方式 */
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

//加密文章可見名單

static void
change_stamp(folder, hdr)
  char *folder;
  HDR *hdr;
{
  HDR buf;

  /* 為了確定新造出來的 stamp 也是 unique (不和既有的 chrono 重覆)，
     就產生一個新的檔案，該檔案隨便 link 即可。
     這個多產生出來的垃圾會在 expire 被 sync 掉 (因為不在 .DIR 中) */
  hdr_stamp(folder, HDR_LINK | 'A', &buf, "etc/stamp");
  hdr->stamp = buf.chrono;
}

/* ----------------------------------------------------- */
/* 改良 innbbsd 轉出信件、連線砍信之處理程序		 */
/* ----------------------------------------------------- */


void
btime_update(bno)
  int bno;
{
  if (bno >= 0)
    (bshm->bcache + bno)->btime = -1;	/* 讓 class_item() 更新用 */
}


#ifndef HAVE_NETTOOL
static 			/* 給 enews.c 用 */
#endif
void
outgo_post(hdr, board)
  HDR *hdr;
  char *board;
{
  bntp_t bntp;

  memset(&bntp, 0, sizeof(bntp_t));

  if (board)		/* 新信 */
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
  if ((hdr->xmode & POST_OUTGO) &&		/* 外轉信件 */
    (hdr->chrono > ap_start - 7 * 86400))	/* 7 天之內有效 */
  {
    outgo_post(hdr, NULL);
  }
}


static inline int		/* 回傳文章 size 去扣錢 */
move_post(hdr, folder, by_bm, rangedel)	/* 將 hdr 從 folder 搬到別的板 */
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
  /* 置底文及BN_RANGEDELETED中的文章被砍不用 move_post */
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

    /* 直接複製 trailing data：owner(含)以下所有欄位 */

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
/* 改良 cross post 停權					 */
/* ----------------------------------------------------- */


#define MAX_CHECKSUM_POST	20	/* 記錄最近 20 篇文章的 checksum */
#define MAX_CHECKSUM_LINE	6	/* 只取文章前 6 行來算 checksum */


typedef struct
{
  int sum;			/* 文章的 checksum */
  int total;			/* 此文章已發表幾篇 */
}      CHECKSUM;


static CHECKSUM checksum[MAX_CHECKSUM_POST];
static int checknum = 0;


static inline int
checksum_add(str)		/* 回傳本列文字的 checksum */
  char *str;
{
  int i, len, sum;

  len = strlen(str);

  sum = len;	/* 當字數太少時，前四分之一很可能完全相同，所以將字數也加入 sum 值 */
  for (i = len >> 2; i > 0; i--)	/* 只算前四分之一字元的 sum 值 */
    sum += *str++;

  return sum;
}


static inline int		/* 1:是cross-post 0:不是cross-post */
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


static int			/* 1:是cross-post 0:不是cross-post */
checksum_find(fpath)
  char *fpath;
{
  int i, sum;
  char buf[ANSILINELEN];
  FILE *fp;

  sum = 0;
  if (fp = fopen(fpath, "r"))
  {
    for (i = -(LINE_HEADER + 1);;)	/* 前幾列是檔頭 */
    {
      if (!fgets(buf, ANSILINELEN, fp))
	break;

      if (i < 0)	/* 跳過檔頭 */
      {
	i++;
	continue;
      }

      if (*buf == QUOTE_CHAR1 || *buf == '\n' || !strncmp(buf, "※", 2))	 /* 跳過引言 */
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
  int bno;			/* 要轉去的看板 */
{
  char *blist, folder[64];
  ACCT acct;
  HDR hdr;

  if (HAS_PERM(PERM_ALLADMIN))
    return 0;

  /* 板主在自己管理的看板不列入跨貼檢查 */
  blist = (bshm->bcache + bno)->BM;
  if (HAS_PERM(PERM_BM) && blist[0] > ' ' && is_bm(blist, cuser.userid))
    return 0;

  if (checksum_find(fpath))
  {
    /* 如果是 cross-post，那麼轉去 BN_SECURITY 並直接停權 */
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
    mail_self(FN_ETC_CROSSPOST, str_sysop, "Cross-Post 停權", 0);
    vmsg("您因為過度 Cross-Post 已被停權");
    return 1;
  }
  return 0;
}
#endif	/* HAVE_DETECT_CROSSPOST */


/* ----------------------------------------------------- */
/* 發表、回應、編輯、轉錄文章				 */
/* ----------------------------------------------------- */

int
is_author(hdr)
  HDR *hdr;
{
  /* 這裡沒有檢查是不是 guest，注意使用此函式時要特別考慮 guest 情況 */

  /* itoc.070426: 當帳號被清除後，新註冊相同 ID 的帳號並不擁有過去該 ID 發表的文章之所有權 */
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

/* 適當位置加入一 function post_prefix() */

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
              strcpy(buf[j], "#  板主御用");
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

        menu[j] = "Q  取消";
        menu[j + 1] = NULL;

        sprintf(buf_title, "請選擇文章類別 (←)換頁║%c║", page + '1');
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
          vmsg("無法使用此文章類別...請重新選擇！");
        else
          return buf[ch - '1'] + 3 ;
      }
    }
  }
  return NULL;
}

/*080805.cache: ALL POST 備份 板名:AllPost*/
static void
do_allpost(fpath, owner, nick, mode)
  char *fpath;
  char *owner;
  char *nick;
  int mode;
{
  HDR hdr;
  char folder[64];
  char *brdname = "AllPost";       // 板名自定

  brd_fpath(folder, brdname, fn_dir);
  hdr_stamp(folder, HDR_LINK | 'A', &hdr, fpath);

  hdr.xmode = mode & ~POST_OUTGO;  /* 拿掉 POST_OUTGO */
  strcpy(hdr.owner, owner);
  strcpy(hdr.nick, nick);
  strcpy(hdr.title, ve_title);

  rec_bot(folder, &hdr, sizeof(HDR));

  btime_update(brd_bno(brdname));
}

/*080812.cache: 轉錄文章到某特定類別*/

static int
depart_post(xo)
  XO *xo;
{
  char folder[64], fpath[64];
  HDR post, *hdr;
  BRD *brdp, *bend;
  int ans, pos, cur;

  if (!HAS_PERM(PERM_ALLBOARD)/*|| strcmp(currboard, "DepartAll")*/)/* 僅看板站務 不指定看板 */
    {
      vmsg("您的權限不足！");
      return XO_NONE;
    }

  if (vans("請確認是否進行分類看板轉錄？[y/N] ") != 'y')
    {
      vmsg("轉錄取消！");
      return XO_FOOT;
    }

  pos = xo->pos;
  cur = pos - xo->top;
  hdr = (HDR *) xo_pool + cur;

  hdr_fpath(fpath, xo->dir, hdr);

  hdr->xmode ^= POST_CROSSED; /* 給予自訂標記 供post_attr()顯示 */
  currchrono = hdr->chrono;
  rec_put(xo->dir, hdr, sizeof(HDR), pos, cmpchrono);

  brdp = bshm->bcache;
  bend = brdp + bshm->number;

/*cache.080812: 趕時間直接用複製的,寫的很爛*/
  switch (ans = vans("◎ 轉錄至 1)本系 2)班級 3)個人 ？[Q] "))
  {
  case '1':
    while (brdp < bend)
    {
      if (!strcmp(brdp->class, "本系") /*&& strcmp(brdp->brdname, "DepartAll")*/)
      /* 轉錄至指定分類 但不包含本身 */
      {
        brd_fpath(folder, brdp->brdname, fn_dir);
        hdr_stamp(folder, HDR_COPY | 'A', &post, fpath);
        strcpy(post.owner, hdr->owner);
        sprintf(post.title, "[轉錄] %s", hdr->title); /* 加註轉錄字樣 */
        rec_bot(folder, &post, sizeof(HDR));
        btime_update(brd_bno(brdp->brdname)); /* 看板閱讀紀錄更新 */
      }
      brdp++;
    }
    break;

  case '2':
    while (brdp < bend)
    {
      if (!strcmp(brdp->class, "班級") /*&& strcmp(brdp->brdname, "DepartAll")*/)
      /* 轉錄至指定分類 但不包含本身 */
      {
        brd_fpath(folder, brdp->brdname, fn_dir);
        hdr_stamp(folder, HDR_COPY | 'A', &post, fpath);
        strcpy(post.owner, hdr->owner);
        sprintf(post.title, "[轉錄] %s", hdr->title); /* 加註轉錄字樣 */
        rec_bot(folder, &post, sizeof(HDR));
        btime_update(brd_bno(brdp->brdname)); /* 看板閱讀紀錄更新 */
      }
      brdp++;
    }
    break;

  case '3':
    while (brdp < bend)
    {
      if (!strcmp(brdp->class, "個人") /*&& strcmp(brdp->brdname, "DepartAll")*/)
      /* 轉錄至指定分類 但不包含本身 */
      {
        brd_fpath(folder, brdp->brdname, fn_dir);
        hdr_stamp(folder, HDR_COPY | 'A', &post, fpath);
        strcpy(post.owner, hdr->owner);
        sprintf(post.title, "[轉錄] %s", hdr->title); /* 加註轉錄字樣 */
        rec_bot(folder, &post, sizeof(HDR));
        btime_update(brd_bno(brdp->brdname)); /* 看板閱讀紀錄更新 */
      }
      brdp++;
    }
    break;

  default:
    vmsg("取消轉錄！");
    return XO_INIT;
    break;
  }

//不確定是否要加上...再看看
/*
    if (xmode & POST_CROSSED)
    {
      hdr->xmode &= ~POST_CROSSED;
    }
*/

  vmsg("轉錄完成！");
  return XO_INIT; /* 給定POST_XXX 刷新頁面 */
}

void
post_history(xo, hdr)          /* 將 hdr 這篇加入 brh */
  XO *xo;
  HDR *hdr;
{
  int fd;
  time_t prev, chrono, next, this;
  HDR buf;

  chrono = BMAX(hdr->chrono, hdr->stamp);
  if (!brh_unread(chrono))      /* 如果已在 brh 中，就無需動作 */
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

    if (prev > chrono)      /* 沒有下一篇 */
      prev = chrono;
    if (next < chrono)      /* 沒有上一篇 */
      next = chrono;

    brh_add(prev, chrono, next);
  }
}

static int
do_post(xo, title)
  XO *xo;
  char *title;
{
  /* Thor.981105: 進入前需設好 curredit 及 quote_file */
  HDR hdr;
  char fpath[64], *folder, *nick, *rcpt;
  int mode;
  time_t spendtime;
  BRD *brd;

  if (!(bbstate & STAT_POST))
  {
#ifdef NEWUSER_LIMIT
    if (cuser.lastlogin - cuser.firstlogin < 3 * 86400)
      vmsg("新手上路，三日後始可張貼文章");
    else
#endif
      vmsg("對不起，您沒有在此發表文章的權限");
    return XO_FOOT;
  }

  film_out(FILM_POST, 0);

  prints("發表文章於【 %s 】看板", currboard);

#ifdef POST_PREFIX
  /* 借用 mode、rcpt、fpath */

  if (title)
  {
    rcpt = NULL;
  }
  else		/* itoc.020113: 新文章選擇標題分類 */
  {

//改了之後好像就用不到了,以後再修掉
//  #define NUM_PREFIX 6
//    char *prefix[NUM_PREFIX] = {"[公告] ", "[新聞] ", "[閒聊] ", "[文件] ", "[問題] ", "[測試] "};

    move(20, 0);
//    outs("類別：");
//    for (mode = 0; mode < NUM_PREFIX; mode++)
//      prints("%d.%s", mode + 1, prefix[mode]);

//    outs("若BM有設定文章分類將會自動跳出選單");/*080419.cache: 先暫時設定成這樣*/
//    mode = vget(20, 0, "若BM有設定文章分類將會自動跳出選單（按 Enter 繼續）：", fpath, 3, DOECHO) - '1';
//   if (mode >= 0 && mode < NUM_PREFIX)		// 輸入數字選項
//     rcpt = prefix[mode];
//   else					// 空白跳過
//     rcpt = NULL;
  }

//  if (!ve_subject(21, title, rcpt)) //080419.cache: 這樣原本的統一分類就取消了
    if (!ve_subject(22, title, ((title) ? NULL : post_prefix())))
#else

#endif
      return XO_HEAD;

  /* 未具備 Internet 權限者，只能在站內發表文章 */
  /* Thor.990111: 沒轉信出去的看板, 也只能在站內發表文章 */

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
  spendtime = time(0) - spendtime;	/* itoc.010712: 總共花的時間(秒數) */

  /* build filename */

  folder = xo->dir;
  hdr_stamp(folder, HDR_LINK | 'A', &hdr, fpath);

  /* set owner to anonymous for anonymous board */

#ifdef HAVE_ANONYMOUS
  /* Thor.980727: lkchu新增之[簡單的選擇性匿名功能] */
  if (curredit & EDIT_ANONYMOUS)
  {
    rcpt = anonymousid;	/* itoc.010717: 自定匿名 ID */
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

/* qazq.031025: 公開板才做 all_post */
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

//#if 1	/* itoc.010205: post 完文章就記錄，使不出現未閱讀的＋號 */
//  chrono = hdr.chrono;
//  prev = ((mode = rec_num(folder, sizeof(HDR)) - 2) >= 0 && !rec_get(folder, &buf, sizeof(HDR), mode)) ? buf.chrono : chrono;
//  brh_add(prev, chrono, chrono);
//#endif

  post_history(xo, &hdr);

  clear();
  outs("順利貼出文章，");

  if (currbattr & BRD_NOCOUNT || wordsnum < 30)
  {				/* itoc.010408: 以此減少灌水現象 */
    outs("文章不列入紀錄，敬請包涵。");
  }
  else
  {
    /* itoc.010408: 依文章長度/所費時間來決定要給多少錢；幣制才會有意義 */
    mode = BMIN(wordsnum, spendtime) / 10;	/* 每十字/秒 一元 */
    prints("這是您的第 %d 篇文章，得 %d 銀。", ++cuser.numposts, mode);
    addmoney(mode);
  }

  /* 回應到原作者信箱 */

  if (curredit & EDIT_BOTH)
  {
    rcpt = quote_user;

    if (strchr(rcpt, '@'))	/* 站外 */
      mode = bsmtp(fpath, title, rcpt, 0);
    else			/* 站內使用者 */
      mode = mail_him(fpath, rcpt, title, 0);

    outs(mode >= 0 ? "\n\n成功\回應至作者信箱" : "\n\n作者無法收信");
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

  switch (vans("▲ 回應至 (F)看板 (M)作者信箱 (B)二者皆是 (Q)取消？[F] "))
  {
  case 'm':
    hdr_fpath(quote_file, xo->dir, hdr);
    return do_mreply(hdr, 0);

  case 'q':
    return XO_FOOT;

  case 'b':
    /* 若無寄信的權限，則只回看板 */
    if (HAS_PERM(strchr(hdr->owner, '@') ? PERM_INTERNET : PERM_LOCAL))
      curredit = EDIT_BOTH;
    break;
  }

  /* Thor.981105: 不論是轉進的, 或是要轉出的, 都是別站可看到的, 所以回信也都應該轉出 */
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
    if (hdr->xmode & POST_LOCKED)    /* 鎖定文章不能回文 */
      {
        vmsg("被鎖定文章不能回文");
        return XO_NONE;
      }           /* 若要已鎖定文可回文，這段就不要加 */
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
/* 印出 hdr 標題					 */
/* ----------------------------------------------------- */


int
tag_char(chrono)
  int chrono;
{
  return TagNum && !Tagger(chrono, 0, TAG_NIN) ? '*' : ' ';
}


#ifdef HAVE_DECLARE
static inline int
cal_day(date)		/* itoc.010217: 計算星期幾 */
  char *date;
{
#if 0
   蔡勒公式是一個推算哪一天是星期幾的公式.
   這公式是:
         c                y       26(m+1)
    W= [---] - 2c + y + [---] + [---------] + d - 1
         4                4         10
    W → 為所求日期的星期數. (星期日: 0  星期一: 1  ...  星期六: 6)
    c → 為已知公元年份的前兩位數字.
    y → 為已知公元年份的後兩位數字.
    m → 為月數
    d → 為日數
   [] → 表示只取該數的整數部分 (地板函數)
    ps.所求的月份如果是1月或2月,則應視為上一年的13月或14月.
       所以公式中m的取值範圍不是1到12,而是3到14
#endif

  /* 適用 2000/03/01 至 2099/12/31 */

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
  int cc;			/* 印出最多 cc - 1 字的標題 */
{
  /* 回覆/轉錄/原創/閱讀中的同主題回覆/閱讀中的同主題轉錄/閱讀中的同主題原創 */
  static char *type[6] = {"Re", "Fw", "◇", "\033[1;33m=>", "\033[1;33m=>", "\033[1;32m◆"};
  uschar *title, *mark;
  int ch, len;
  int in_chi;		/* 1: 在中文字中 */
#ifdef HAVE_DECLARE
  int square;		/* 1: 要處理方括 */
#endif
#ifdef CHECK_ONLINE
  UTMP *online;
#endif

  /* --------------------------------------------------- */
  /* 印出日期						 */
  /* --------------------------------------------------- */

#ifdef HAVE_DECLARE
  /* itoc.010217: 改用星期幾來上色 */
  prints("\033[1;3%dm%s\033[m ", cal_day(hdr->date) + 1, hdr->date + 3);
#else
  outs(hdr->date + 3);
  outc(' ');
#endif

  /* --------------------------------------------------- */
  /* 印出作者						 */
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
      /* 把超過 len 長度的部分直接切掉 */
      /* itoc.060604.註解: 如果剛好切在中文字的一半就會出現亂碼，不過這情況很少發生，所以就不管了 */
      ch = '.';
    }
    else
    {
      /* 站外的作者把 '@' 換成 '.' */
      if (in_chi || IS_ZHC_HI(ch))	/* 中文字尾碼是 '@' 的不算 */
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
  /* 印出標題的種類					 */
  /* --------------------------------------------------- */

  /* len: 標題是 type[] 裡面的那一種 */
  title = str_ttl(mark = hdr->title);
  len = (title == mark) ? 2 : (*mark == 'R') ? 0 : 1;
  if (!strcmp(currtitle, title))
    len += 3;
  outs(type[len]);
  outc(' ');

  /* --------------------------------------------------- */
  /* 印出標題						 */
  /* --------------------------------------------------- */

  mark = title + cc;

#ifdef HAVE_DECLARE	/* Thor.980508: Declaration, 嘗試使某些title更明顯 */
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

  /* 把超過 cc 長度的部分直接切掉 */
  /* itoc.060604.註解: 如果剛好切在中文字的一半就會出現亂碼，不過這情況很少發生，所以就不管了 */
  while ((ch = *title++) && (title < mark))
  {
#ifdef HAVE_DECLARE
    if (square)
    {
      if (in_chi || IS_ZHC_HI(ch))	/* 中文字的第二碼若是 ']' 不算是方括 */
      {
	in_chi ^= 1;
      }
      else if (ch == ']')
      {
	outs("]\033[m");
	square = 0;			/* 只處理一組方括，方括已經處理完了 */
	continue;
      }
    }
#endif

    outc(ch);
  }

#ifdef HAVE_DECLARE
  if (square || len >= 3)	/* Thor.980508: 變色還原用 */
#else
  if (len >= 3)
#endif
    outs("\033[m");

  outc('\n');
}


/* ----------------------------------------------------- */
/* 看板功能表						 */
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

  /* 已閱讀為小寫，未閱讀為大寫 */
  /* 置底文不會有閱讀記錄 */
  /* 加密文章視為已讀 */
#ifdef HAVE_REFUSEMARK
  attr = ((mode & POST_BOTTOM) || !brh_unread(BMAX(hdr->chrono, hdr->stamp)) || !chkrestrict(hdr)) ? 0x20 : 0;
#else
  attr = ((mode & POST_BOTTOM) || !brh_unread(BMAX(hdr->chrono, hdr->stamp))) ? 0x20 : 0;
#endif

#ifdef HAVE_REFUSEMARK
  if (mode & POST_RESTRICT)
    attr |= (RefusePal_belong(currboard, hdr)) ? 'O' : 'X';
  else
#endif

#ifdef HAVE_LOCKEDMARK
  if (mode & POST_LOCKED)
    attr |= '!';
  else
#endif

 if (mode & POST_DONE)
   attr |= 'S';        /* qazq.030815: 站方處理完標記ｓ */
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
#ifdef HAVE_SCORE /* cache.080314: 推文系統 */
  char buf[10];
  sprintf(buf,"%d",num);
//  prints("%6s%c%c", (hdr->xmode & POST_BOTTOM) ? "Imp" : buf, tag_char(hdr->chrono), post_attr(hdr));

/* cache.081204: 置底文多樣化選擇 */

//採用雙重 token 判斷
  if (hdr->xmode & POST_BOTTOM)
  {
    if (hdr->xmode & POST_BOTTOM1)
      prints("   Imp%c%c", tag_char(hdr->chrono), post_attr(hdr));
    else if (hdr->xmode & POST_BOTTOM2)
      prints("  \033[1;33m重要\033[m%c%c", tag_char(hdr->chrono), post_attr(hdr));
    else if (hdr->xmode & POST_BOTTOM3)
      prints("    \033[1;31m▼\033[m%c%c", tag_char(hdr->chrono), post_attr(hdr));
    else if (hdr->xmode & POST_BOTTOM4)
      prints("    \033[1;32m▼\033[m%c%c", tag_char(hdr->chrono), post_attr(hdr));
    else if (hdr->xmode & POST_BOTTOM5)
      prints("    \033[1;33m▼\033[m%c%c", tag_char(hdr->chrono), post_attr(hdr));
    else if (hdr->xmode & POST_BOTTOM6)
      prints("    \033[1;36m▼\033[m%c%c", tag_char(hdr->chrono), post_attr(hdr));
//舊的置底文只有POST_BOTTOM旗標判斷,要保留以防舊文章沒有置底
    else
      prints("   Imp%c%c", tag_char(hdr->chrono), post_attr(hdr));
  }
  else
    prints("%6d%c%c", num, tag_char(hdr->chrono), post_attr(hdr));

  if (hdr->xmode & POST_SCORE)
  {
    num = hdr->score;
/*080419.cache: 用簡單一點的判斷就好*/
/*080730.cache: 有空重寫這段爛爛的程式碼 = =*/
/*080813.cache: 暫時先這樣--推文多了一堆有的沒的XDD*/
/*100107.FinFunnel: 先修改成這樣*/
	if (num == 0)
		outs(" -- "); //標記曾經有推文過但分數為零
	else if (abs(num) > 120)
		prints("\033[1;3%s \033[m", (num >= 0) ? "3m 爆" : "0m 嫩");
	else if (abs(num) > 99)
		prints("\033[1;3%s \033[m", (num >= 0) ? "1m 爆" : "2m 嫩");
	else if (abs(num) > 19)
		prints("\033[1;3%cm X%d \033[m", (num >= 0) ? '1':'2', abs(num) / 10);
    else if (abs(num) > 9)
      prints("\033[1;3%cm %d \033[m", num >= 0 ? '1' : '2', abs(num));
    else
      prints("\033[1;3%cm  %d \033[m", num >= 0 ? '1' : '2', abs(num));
/*(num <= 99 && num >= -99)改掉,用新的方式判斷*/
  }
  else
  {
    outs("    ");
  }
  hdr_outs(hdr, d_cols + 46);	/* 少一格來放分數 */
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
      if (vans("要新增資料嗎(Y/N)？[N] ") == 'y')
	return post_add(xo);
    }
    else
    {
      vmsg("本看板尚無文章");
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
  return XO_FOOT;	/* itoc.010403: 把 b_lines 填上 feeter */
}


static int
post_head(xo)
  XO *xo;
{
  vs_head(currBM, xo->xyz);
  prints(NECKER_POST, d_cols, "", currbattr & BRD_SCORE ? "○" : "╳", bshm->mantime[currbno]);
  return post_body(xo);
}


/* ----------------------------------------------------- */
/* 資料之瀏覽：browse / history				 */
/* ----------------------------------------------------- */


static int
post_visit(xo)
  XO *xo;
{
  int ans, row, max;
  HDR *hdr;

  ans = vans("設定所有文章 (U)未讀 (V)已讀 (W)前已讀後未讀 (Q)取消？[Q] ");
  if (ans == 'v' || ans == 'u' || ans == 'w')
  {
    row = xo->top;
    max = xo->max - row + 3;
    if (max > b_lines)
      max = b_lines;

    hdr = (HDR *) xo_pool + (xo->pos - row);
    /* brh_visit(ans == 'w' ? hdr->chrono : ans == 'u'); */
    /* weiyu.041010: 在置底文上選 w 視為全部已讀 */
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

/*使用新版的
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

    /* Thor.990204: 為考慮more 傳回值 */
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
	if (do_reply(xo, hdr) == XO_INIT)	/* 有成功地 post 出去了 */
	  return post_init(xo);
      }
      break;

    case 'm':
      if ((bbstate & STAT_BOARD) && !(xmode & POST_MARKED))
      {
	/* hdr->xmode = xmode ^ POST_MARKED; */
	/* 在 post_browse 時看不到 m 記號，所以限制只能 mark */
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
      if (vget(b_lines, 0, "搜尋：", hunt, sizeof(hunt), DOECHO))
      {
	more(fpath, FOOTER_POST);
	goto re_key;
      }
      continue;

    case 'E':
      return post_edit(xo);

    case 'C':	/* itoc.000515: post_browse 時可存入暫存檔 */
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
/* 精華區						 */
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

  XoGem(fpath, "精華區", level);
  return post_init(xo);
}


/* ----------------------------------------------------- */
/* 進板畫面						 */
/* ----------------------------------------------------- */


static int
post_memo(xo)
  XO *xo;
{
  char fpath[64];

  brd_fpath(fpath, currboard, fn_note);
  /* Thor.990204: 為考慮more 傳回值 */
  if (more(fpath, NULL) < 0)
  {
    vmsg("本看板尚無「進板畫面」");
    return XO_HEAD;
  }

  return post_head(xo);
}


/* ----------------------------------------------------- */
/* 功能：tag / switch / cross / forward			 */
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
  return xo->pos + 1 + XO_MOVE; /* lkchu.981201: 跳至下一項 */
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
/* target : 將文章轉錄到自訂的看板列表中                 */
/* create : 2005/09/02                                   */
/* author : hialan.bbs@bbs.math.cycu.edu.tw              */
/*-------------------------------------------------------*/

#define MAX_CROSSBOARD 100      /* 最多能夠轉錄看板數 */

#define STR_CROSSLISTSTART      "<看板名單開始>"
#define STR_CROSSLISTEND        "<看板名單結束>"

static int
cross_loadlist(char *fpath, char boardlist[MAX_CROSSBOARD][BNLEN + 1])
{
  FILE *fp;
  int num, in_list, bno;
  char buf[ANSILINELEN], *brdname;
  BRD *brd;

  if (!(fp = fopen(fpath, "r")))
  {
    vmsg("無法開啟檔案！");
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
      sprintf(buf, "%s 看板不存在！", brdname);
      vmsg(buf);
    }
    else
    {
      brd = bshm->bcache + bno;
      strcpy(brdname, brd->brdname);       /* 改變大小寫 */
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
    vmsg("名單格式不對或是沒有名單！");
    return XO_FOOT;
  }

  clear();
  prints("標題：%s\n", hdr->title);
  outs("將轉至下列看板\n");

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

      /* itoc.030325: 原文轉錄直接 copy 即可 */
      hdr_stamp(xfolder, HDR_COPY | 'A', &xpost, fpath);

      strcpy(xpost.owner, hdr->owner);
      strcpy(xpost.nick, hdr->nick);
      strcpy(xpost.date, hdr->date);    /* 原文轉載保留原日期 */
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
  /* 來源看板 */
  char *dir, *ptr;
  HDR *hdr, xhdr;

  /* 欲轉去的看板 */
  int xbno;
  usint xbattr;
  char xboard[BNLEN + 1], xfolder[64];
  HDR xpost;

  int tag, rc, locus, finish;
  int method;		/* 0:原文轉載 1:從公開看板/精華區/信箱轉錄文章 2:從秘密看板轉錄文章 */
  usint tmpbattr;
  char tmpboard[BNLEN + 1];
  char fpath[64], buf[ANSILINELEN];
  FILE *fpr, *fpw, *fpo;

  if (!cuser.userlevel)	/* itoc.000213: 避免 guest 轉錄去 sysop 板 */
    return XO_NONE;

  tag = AskTag("轉錄");
  if (tag < 0)
    return XO_FOOT;

  dir = xo->dir;

//  if (!ask_board(xboard, BRD_W_BIT, "\n\n\033[1;33m請挑選適當的看板，切勿轉錄超過規定次數。\033[m\n\n") ||
//    (*dir == 'b' && !strcmp(xboard, currboard)))	/* 信箱、精華區中可以轉錄至currboard */
//    return XO_HEAD;

  hdr = tag ? &xhdr : (HDR *) xo_pool + (xo->pos - xo->top);	/* lkchu.981201: 整批轉錄 */

  /* 原作者轉錄自己文章時，可以選擇「原文轉載」 */
//  method = (HAS_PERM(PERM_ALLBOARD) || (!tag && is_author(hdr))) &&
//    (vget(2, 0, "(1)原文轉載 (2)轉錄文章？[1] ", buf, 3, DOECHO) != '2') ? 0 : 1;

  if (!HAS_PERM(PERM_ALLBOARD) && (tag || strcmp(hdr->owner, cuser.userid)))
    method = 1;
  else
  {
    vget(1, 0, HAS_PERM(PERM_ALLBOARD) ?
      "(1)原文轉載 (2)轉錄文章 (3)看板總管CrossPost？[Q] " :
      "(1)原文轉載 (2)轉錄文章？[Q] ", buf, 3, DOECHO);

    method = (buf[0] == '1') ? 0 :
             (buf[0] == '2') ? 1 :
             (buf[0] == '3') ? 3 : -1;

    if (method == 3)
      return cross_announce(xo);

    if (method < 0)
      return XO_HEAD;
  }

  if (!ask_board(xboard, BRD_W_BIT, "\n\n"
    "\033[1;33m請挑選適當的看板，切勿轉錄超過規定次數。\033[m\n\n") ||
    (*dir == 'b' && !strcmp(xboard, currboard)))
    /* 信箱、精華區中可以轉錄至currboard */
    return XO_HEAD;

  if (!tag)	/* lkchu.981201: 整批轉錄就不要一一詢問 */
  {
    if (method)
      sprintf(ve_title, "Fw: %.68s", str_ttl(hdr->title));	/* 已有 Re:/Fw: 字樣就只要一個 Fw: */
    else
      strcpy(ve_title, hdr->title);

    if (!vget(2, 0, "標題：", ve_title, TTLEN + 1, GCARRY))
      return XO_HEAD;
  }

#ifdef HAVE_REFUSEMARK
  rc = vget(2, 0, "(S)存檔 (L)站內 (X)密封 (Q)取消？[Q] ", buf, 3, LCECHO);
  if (rc != 'l' && rc != 's' && rc != 'x')
#else
  rc = vget(2, 0, "(S)存檔 (L)站內 (Q)取消？[Q] ", buf, 3, LCECHO);
  if (rc != 'l' && rc != 's')
#endif
    return XO_HEAD;

  if (method && *dir == 'b')	/* 從看板轉出，先檢查此看板是否為秘密板 */
  {
    /* 借用 tmpbattr */
    tmpbattr = (bshm->bcache + currbno)->readlevel;
    if (tmpbattr == PERM_SYSOP || tmpbattr == PERM_BOARD)
      method = 2;
  }

  xbno = brd_bno(xboard);
  xbattr = (bshm->bcache + xbno)->battr;

  /* Thor.990111: 在可以轉出前，要檢查有沒有轉出的權力? */
  if ((rc == 's') && (!HAS_PERM(PERM_INTERNET) || (xbattr & BRD_NOTRAN)))
    rc = 'l';

  /* 備份 currboard */
  if (method)
  {
    /* itoc.030325: 一般轉錄呼叫 ve_header，會使用到 currboard、currbattr，先備份起來 */
    strcpy(tmpboard, currboard);
    strcpy(currboard, xboard);
    tmpbattr = currbattr;
    currbattr = xbattr;
  }

  locus = 0;
  do	/* lkchu.981201: 整批轉錄 */
  {
    if (tag)
    {
      EnumTag(hdr, dir, locus, sizeof(HDR));

      if (method)
	sprintf(ve_title, "Fw: %.68s", str_ttl(hdr->title));	/* 已有 Re:/Fw: 字樣就只要一個 Fw: */
      else
	strcpy(ve_title, hdr->title);
    }

    if (hdr->xmode & GEM_FOLDER)	/* 非 plain text 不能轉 */
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

    if (method)		/* 一般轉錄 */
    {
      /* itoc.030325: 一般轉錄要重新加上 header */
      fpw = fdopen(hdr_stamp(xfolder, 'A', &xpost, buf), "w");
      ve_header(fpw);

      /* itoc.040228: 如果是從精華區轉錄出來的話，會顯示轉錄自 [currboard] 看板，
	 然而 currboard 未必是該精華區的看板。不過不是很重要的問題，所以就不管了 :p */
      fprintf(fpw, "※ 本文轉錄自 [%s] %s\n\n",
	*dir == 'u' ? cuser.userid : method == 2 ? "秘密" : tmpboard,
	*dir == 'u' ? "信箱" : "看板");

      /* Kyo.051117: 若是從秘密看板轉出的文章，刪除文章第一行所記錄的看板名稱 */
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
    else		/* 原文轉錄 */
    {
      /* itoc.030325: 原文轉錄直接 copy 即可 */
      hdr_stamp(xfolder, HDR_COPY | 'A', &xpost, fpath);

      strcpy(xpost.owner, hdr->owner);
      strcpy(xpost.nick, hdr->nick);
      strcpy(xpost.date, hdr->date);	/* 原文轉載保留原日期 */
    }

/* 20081130.cache: 轉錄文章會在原文章紀錄 */
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

      sprintf(msg, "轉錄到%s看板",
             (checklevel == PERM_SYSOP || checklevel == PERM_BOARD) ?
             "某秘密" : xboard); /* 檢查目標看板閱讀權限 */

      fprintf(fpo, "\033[1;36m%13s\033[m \033[1;33m→\033[m：%-51s %02d/%02d/%02d\n",
      cuser.userid, msg, ptime->tm_year % 100, ptime->tm_mon + 1, ptime->tm_mday);
      /* 格式自訂 盡量與推文類似 */

      fclose(fpo);

    } /* 轉錄原文註記錄 070317 guessi */
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

  /* Thor.981205: check 被轉的板有沒有列入紀錄? */
  if (!(xbattr & BRD_NOCOUNT))
    cuser.numposts += tag ? tag : 1;	/* lkchu.981201: 要算 tag */

  /* 復原 currboard、currbattr */
  if (method)
  {
    strcpy(currboard, tmpboard);
    currbattr = tmpbattr;
  }

  vmsg("轉錄完成");
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

  if (hdr->xmode & GEM_FOLDER)	/* 非 plain text 不能轉 */
    return XO_NONE;

#ifdef HAVE_REFUSEMARK
  if (!chkrestrict(hdr))
    return XO_NONE;
#endif

  if (acct_get("轉達信件給：", &muser) > 0)
  {
    strcpy(quote_user, hdr->owner);
    strcpy(quote_nick, hdr->nick);
    hdr_fpath(quote_file, xo->dir, hdr);
    sprintf(ve_title, "%.64s (fwd)", hdr->title);
    move(1, 0);
    clrtobot();
    prints("轉達給: %s (%s)\n標  題: %s\n", muser.userid, muser.username, ve_title);


  if (!HAS_PERM(PERM_ALLBOARD))
  {
    result = mail_send(muser.userid);

    if (result >= 0)
    {
      char fpath[64];
      FILE *fp;

      hdr_fpath(fpath, xo->dir, hdr);

#if 0
這邊不確定能否直接使用上面標黃色的變數當作 FILE的檔案路徑
因為 hdr_path 抓的都一樣
如果可以，則下面的 fpath 請改成 quote_file
#endif

      if (fp = fopen(fpath, "a"))
      {
        char msg[32];

        time_t now;
        struct tm *ptime;

        time(&now);
        ptime = localtime(&now);

        sprintf(msg, "轉寄到%s信箱", (muser.userid == cuser.userid) ?
          muser.userid : "某私人");

      fprintf(fp, "\033[1;36m%13s\033[m \033[1;33m→\033[m：%-51s %02d/%02d/%02d\n",
      cuser.userid, msg, ptime->tm_year % 100, ptime->tm_mon + 1, ptime->tm_mday);
          /* 格式自訂 盡量與推文類似 */

        fclose(fp);

      } /* 轉錄原文註記錄 070510 hpo14 */
    }
  }
  else
    mail_send(muser.userid);

    *quote_file = '\0';
  }
  return XO_HEAD;
}


/* ----------------------------------------------------- */
/* 板主功能：mark / delete / label			 */
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
    if (xmode & POST_DELETE)	/* 待砍的文章不能 mark */
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
             ans = vans("◎ 是否將本篇取消優文 Y)是 N)否 ？(預設=否) ");

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
                             /* 在線上，寫入 utmp-> */
                               if(uentp->goodart > 0)
                                 uentp->goodart--;
                               online = 1;
                               sprintf(buf, "設定完畢！ %s 優文篇數減為 %d 篇。", uentp->userid, uentp->goodart);
                             }
                        } while (++uentp <= uceil);

                      if(online)
                         vmsg(buf);
                      else        /* 不在線上，寫入 .ACCT */
                        {
                           ACCT acct;
                           if(acct_load(&acct, hdr->owner) >= 0)
                             {
                                if(acct.goodart > 0)
                                  acct.goodart--;
                                acct_save(&acct);
                                sprintf(buf, "設定完畢！ %s 優文篇數減為 %d 篇。", acct.userid, acct.goodart);
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

             ans = vans("◎ 是否將本篇設為優文 Y)是 N)否 ？(預設=否) ");
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
                             /* 在線上，寫入 utmp-> */
                               uentp->goodart++;
                               online = 1;
                               sprintf(buf, "設定完畢！ %s 優文篇數增為 %d 篇。", uentp->userid, uentp->goodart);
                             }
                        } while (++uentp <= uceil);

                      if(online)
                        vmsg(buf);
                      else        /* 不在線上，寫入 .ACCT */
                        {
                           ACCT acct;
                           if(acct_load(&acct, hdr->owner) >= 0)
                              {
                                 acct.goodart++;
                                 acct_save(&acct);
                                 sprintf(buf, "設定完畢！ %s 優文篇數增為 %d 篇。", acct.userid, acct.goodart);
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

  if (!cuser.userlevel) /* itoc.020114: guest 不能對其他 guest 的文章維基 */
    return XO_NONE;

  pos = xo->pos;
  cur = pos - xo->top;
  hdr = (HDR *) xo_pool + cur;

  if (HAS_PERM(PERM_ALLBOARD) || !strcmp(hdr->owner, cuser.userid) || (bbstate & STAT_BOARD))
  {
    xmode = hdr->xmode;

#ifdef HAVE_LABELMARK
    if (xmode & POST_DELETE)    /* 待砍的文章不能 wiki */
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

static int          /* qazq.030815: 站方處理完成標記Ｓ*/
post_done_mark(xo)
  XO *xo;
{
  if (HAS_PERM(PERM_ALLADMIN)) /* qazq.030815: 只有站長可以標記 */
  {
    HDR *hdr;
    int pos, cur, xmode;

    pos = xo->pos;
    cur = pos - xo->top;
    hdr = (HDR *) xo_pool + cur;
    xmode = hdr->xmode;

#ifdef HAVE_LABELMARK
    if (xmode & POST_DELETE)    /* 待砍的文章不能 mark */
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

  switch (choice = vans("確定要把這篇文章置底嗎 ？[Y/N/C]? [N] "))
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
/* cache.081205: 置底符號選擇 */
      switch (ans = vans("◎ 置底選擇 1)Imp 2)重要 3)紅▼ 4)綠▼ 5)黃▼ 6)藍▼ ？(預設=Imp) "))
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
/* cache.081205: 置底符號選擇 */

   default:
     return XO_FOOT;
  }

    strcpy(post.owner, hdr->owner);
    strcpy(post.nick, hdr->nick);
    strcpy(post.title, hdr->title);

    rec_add(xo->dir, &post, sizeof(HDR));
    //btime_update(currbno);  /* 置底文章不會列入未讀 */


    return post_load(xo);	/* 立刻顯示置底文章 */
  }
  return XO_NONE;
}


#ifdef HAVE_REFUSEMARK
static int
post_refuse(xo)		/* itoc.010602: 文章加密 */
  XO *xo;
{
  HDR *hdr;
  int pos, cur;

  if (!cuser.userlevel)	/* itoc.020114: guest 不能對其他 guest 的文章加密 */
    return XO_NONE;

  pos = xo->pos;
  cur = pos - xo->top;
  hdr = (HDR *) xo_pool + cur;

//#ifdef HAVE_LOCKEDMARK
//    if (xmode & POST_LOCKED)    /* 鎖定文章不能加密 */
//      return XO_NONE;           /* 若要已鎖定文可加密，這段就不要加 */
//#endif

  if (is_author(hdr) || (bbstate & STAT_BM))
  {

/* 090127.cache: 文章加密名單 */
    if (hdr->xmode & POST_RESTRICT)
    {
#if 0 //不詢問直接砍掉可見名單
      if (vans("解除文章保密會刪除全部可見名單，您確定嗎(Y/N)？[N] ") != 'y')
      {
        move(3 + cur, 7);
        outc(post_attr(hdr));
        return XO_FOOT;
      }
#endif
      RefusePal_kill(currboard, hdr);
    }
//文章加密名單

    hdr->xmode ^= POST_RESTRICT;
    currchrono = hdr->chrono;
    rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono);

    move(3 + cur, 7);
    outc(post_attr(hdr));

//密文後要自己按 Shift+O 去新增加密文章可見名單
//    if (hdr->xmode & POST_RESTRICT)
//      return XoBM_Refuse_pal(xo);
  }

  return XO_NONE;
}
#endif


#ifdef HAVE_LOCKEDMARK
static int
post_locked(xo)     /* hpo14.20070513: 文章鎖定 */
  XO *xo;           /* 除 STAT_BOARD 權限外無法刪除/編輯 */
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
//    if (xmode & POST_DELETE)    /* 待砍文章不能鎖定 */
//      return XO_NONE;
//#endif

#ifdef HAVE_REFUSEMARK
    if (xmode & POST_RESTRICT)  /* 加密文章不能鎖定 */
      return XO_NONE;           /* 若要加密文可鎖定，這段就不要加 */
#endif

    if (vans(xmode & POST_LOCKED ? "解除文章鎖定 [Y/N]? [N] " : "鎖定此篇文章 [Y/N]? [N] ") != 'y')
      return XO_FOOT;

    if (xmode & POST_LOCKED)
    {
      hdr->xmode &= ~POST_LOCKED;
      hdr->xmode &= ~POST_MARKED;
    }
    else
    {
      /* 鎖定的文章，通常很重要，所以加 POST_MARKED */
      if (xmode & POST_MARKED)
        hdr->xmode ^= ( POST_LOCKED );
      else
        hdr->xmode ^= ( POST_LOCKED | POST_MARKED );
    }

//#ifdef HAVE_POST_BOTTOM         /* hpo14.070523:第一種置底的鎖定同步 */
//      if (hdr->xmode & POST_BOTTOM2)
//        hdr->xmode ^= POST_BOTTOM;  /* 避免正本變成謄本 */
//#endif

    currchrono = hdr->chrono;
    rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos,cmpchrono);

//#ifdef HAVE_POST_BOTTOM         /* hpo14.070523:第一種置底的鎖定同步 */
//    if (hdr->xmode & POST_BOTTOM)   /* 同步置底的謄本 */
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

    if (xmode & (POST_MARKED | POST_RESTRICT | POST_LOCKED))	/* mark 或 加密的文章不能待砍 */
      return XO_NONE;

    hdr->xmode = xmode ^ POST_DELETE;
    currchrono = hdr->chrono;
    rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono);

    move(3 + cur, 7);
    outc(post_attr(hdr));

    return pos + 1 + XO_MOVE;	/* 跳至下一項 */
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

  if (vans("確定要刪除待砍文章嗎(Y/N)？[N] ") != 'y')
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
      /* 連線砍信 */
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
    if (hdr->xmode & POST_LOCKED)    /* 鎖定文章不能砍文 */
      {
        vmsg("被鎖定文章不能砍文");
        return XO_NONE;
      }           /* 若要已鎖定文可砍文，這段就不要加 */
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
       ans = vans("◎ 是否將本篇設為劣文 Y)是 N)否 ？(預設=否) ");
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
                          /* 在線上，寫入 utmp-> */
                             uentp->badart++;
                             online = 1;
                             sprintf(buf, "設定完畢！ %s 劣文篇數增為 %d 篇。", uentp->userid, uentp->badart);
                           }
                     } while (++uentp <= uceil);

                   if(online)
                      vmsg(buf);
                   else        /* 不在線上，寫入 .ACCT */
                     {
                        ACCT acct;
                        if(acct_load(&acct, hdr->owner) >= 0)
                          {
                             acct.badart++;
                             acct_save(&acct);
                             sprintf(buf, "設定完畢！ %s 劣文篇數增為 %d 篇。", acct.userid, acct.badart);
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
	/* itoc.010711: 砍文章要扣錢，算檔案大小 */
	pos = pos >> 3;	/* 相對於 post 時 wordsnum / 10 */

	/* itoc.010830.註解: 漏洞: 若 multi-login 砍不到另一隻的錢 */
	if (cuser.money > pos)
	  cuser.money -= pos;
	else
	  cuser.money = 0;

	if (cuser.numposts > 0)
	  cuser.numposts--;
	sprintf(buf, "%s，您的文章減為 %d 篇", MSG_DEL_OK, cuser.numposts);
	vmsg(buf);
      }

      if (xo->key == XZ_XPOST)
      {
	vmsg("原列表經刪除後混亂，請重進串接模式！");
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
    vmsg("原列表經批次刪除後混亂，請重進串接模式！");
    return XO_QUIT;
  }

  return ret;
}


static int
post_copy(xo)	   /* itoc.010924: 取代 gem_gather */
  XO *xo;
{
  int tag;

  tag = AskTag("看板文章拷貝");

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
      zmsg("檔案標記完成。[注意] 您必須先離開串接模式才能進入精華區。");
      return XO_FOOT;
    }
    else
#endif
    {
      zmsg("拷貝完成。[注意] 貼上後才能刪除原文！");
      return post_gem(xo);	/* 拷貝完直接進精華區 */
    }
  }

  zmsg("檔案標記完成。[注意] 您只能在擔任(小)板主所在或個人精華區貼上。");
  return XO_FOOT;
}


/* ----------------------------------------------------- */
/* 站長功能：edit / title				 */
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

  //brd_fpath(folder, BN_MODIFY, fn_dir); //不用再Modify板另外發篇新文

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
                vmsg("文章已被鎖定，僅供瀏覽");
                  return XO_NONE;
              }
#endif

}

         //   hdr_stamp(folder, HDR_COPY | 'A', &xpost, fpath);
         //   strcpy(xpost.owner, hdr->owner);
         //   strcpy(xpost.nick, hdr->nick);
         //   strcpy(xpost.title, hdr->title);


      if(sedit(fpath,hdr))/*需要用到edit.c裡的結構和函式*/
      {
              return XO_HEAD;
      }

      //借用文章合併功能
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
            vmsg("\033[1;33m ▲ 檔案已被修改過! ▲\033[m");
            outs("進行自動合併 [Smart Merge]...");

            if (AppendTail(genbuf, fpath, oldsz) == 0)
            {

                oldmt = newmt;
                outs("\033[1;32m合併成功\033[m");

            }
            else
            {

                vmsg("合併失敗");

            }
        }
        if (oldmt == newmt)
        {
           f_mv(fpath, genbuf);



        }else
        {   return XO_HEAD;//合併失敗就還原文章不重編了。
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

//081226.cache: 修改文章備份
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

//081226.cache: 修改文章備份
  brd_fpath(folder, BN_MODIFY, fn_dir);

  f_cp(genbuf, fpath, O_APPEND);                /* do Copy */
  stat(fpath, &st);        oldsz = st.st_size;
  stat(genbuf, &st);        oldmt = st.st_mtime;

  while (1)
  {
        unsigned char oldsum[SMHASHLEN] = {0}, newsum[SMHASHLEN] = {0};

        if (hash_partial_file(fpath, oldsz, oldsum) != HASHPF_RET_OK)
            SmartMerge = 0;

        if (HAS_PERM(PERM_SYSOP))                        /* 站長修改 */
        {

           hdr_stamp(folder, HDR_COPY | 'A', &xpost, fpath);
           strcpy(xpost.owner, hdr->owner);
           strcpy(xpost.nick, hdr->nick);
           strcpy(xpost.title, hdr->title);

           if(!vedit(fpath, 0))
             {
                if(vans("是否留下修改文章記錄？[Y/n] ") != 'n')
                  DoRecord = 1;
             }
           else
             {
               hdr_fpath(fpath, folder, &xpost);
               unlink(fpath);
               return XO_HEAD;
             }
        }//站長修改
    else if (cuser.userlevel && (is_author(hdr) || (hdr->xmode & POST_WIKI)) )	/* 原作者修改 */
        {
          #ifdef HAVE_LOCKEDMARK
            if(hdr->xmode & POST_LOCKED)
              {
                vmsg("文章已被鎖定，僅供瀏覽");    /* 敘述自訂 */
                  return XO_NONE;
              }
          #endif

            hdr_stamp(folder, HDR_COPY | 'A', &xpost, fpath);
            strcpy(xpost.owner, hdr->owner);
            strcpy(xpost.nick, hdr->nick);
            strcpy(xpost.title, hdr->title);

            if (!vedit(fpath, 0))        /* 若非取消則加上修改資訊 */
            {
                DoRecord = 1;
            }
            else
            {
                return XO_HEAD;
            }
        }
        else                /* itoc.010301: 提供使用者修改(但不能儲存)其他人發表的文章 */
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
            outs("\033[1;33m ▲ 檔案已被修改過! ▲\033[m\n\n");
            outs("進行自動合併 [Smart Merge]...\n\n");

            if (AppendTail(genbuf, fpath, oldsz) == 0)
            {

                oldmt = newmt;
                outs("\033[1;32m合併成功\，新修改(或推文)已加入您的文章中。\033[m\n");
                MergeDone = 1;
                vmsg("合併過程中異動的推文數將自動重新計算");
            }
            else
            {
                outs("\033[1;31m自動合併失敗。 請改用人工手動編輯合併。\033[m");
                vmsg("合併失敗");
            }
        }
        if (oldmt != newmt)
        {
            int c = 0;

            clear();
            move(b_lines-8, 0);
            outs("\033[1;33m ▲ 檔案已被修改過! ▲\033[m\n\n");

            outs("可能是您在編輯的過程中有人進行推文或修文。\n\n"
                 "您可以選擇直接覆蓋\檔案(y)、放棄(n)，\n"
                 " 或是\033[1m重新編輯\033[m(新文會被貼到剛編的檔案後面)(e)。\n");
            c = vans("要直接覆蓋\檔案/取消/重編嗎? [Y/n/e]");

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
                    vmsg("抱歉，檔案已損毀。");
                    if(src) fclose(src);
                    unlink(fpath); // fpath is a temp file
                    return XO_HEAD;
                }

                if(src)
                {
                    int c = 0;

                    fprintf(fp, MSG_SEPERATOR "\n");
                    fprintf(fp, "以下為被修改過的最新內容: ");
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
              sprintf(msg, "參與 %s 編輯", "wiki");
              fprintf(fp, "\033[1;36m%13s\033[m \033[1;33m→\033[m：%-51s %02d/%02d/%02d\n",
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

    //複製到 modify 看板
    rec_bot(folder, &xpost, sizeof(HDR));
    btime_update(brd_bno(BN_MODIFY));

      /* 090127.cache: wiki紀錄 */
      if (hdr->xmode & POST_WIKI)
        {
          /* bluesway.070323: 借fpath 來存精華區路徑 */
          gem_fpath(fpath, currboard, fn_dir);
          gem_log(fpath, "Wiki", hdr);
        }

      if (MergeDone)
         vmsg("合併完成");

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
    return XO_HEAD;        /* itoc.021226: XZ_POST 和 XZ_XPOST 共用 post_edit() */

}


void
header_replace(xo, hdr)		/* itoc.010709: 修改文章標題順便修改內文的標題 */
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

  fgets(buf, sizeof(buf), fpr);		/* 加入作者 */
  fputs(buf, fpw);

  fgets(buf, sizeof(buf), fpr);		/* 加入標題 */
  if (!str_ncmp(buf, "標", 2))		/* 如果有 header 才改 */
  {
    strcpy(buf, buf[2] == ' ' ? "標  題: " : "標題: ");
    strcat(buf, hdr->title);
    strcat(buf, "\n");
  }
  fputs(buf, fpw);

  while(fgets(buf, sizeof(buf), fpr))	/* 加入其他 */
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

  if (!cuser.userlevel)	/* itoc.000213: 避免 guest 在 sysop 板改標題 */
    return XO_NONE;

  pos = xo->pos;
  cur = pos - xo->top;
  fhdr = (HDR *) xo_pool + cur;
  memcpy(&mhdr, fhdr, sizeof(HDR));

  if (!is_author(&mhdr) && !HAS_PERM(PERM_ALLBOARD))
    return XO_NONE;

  vget(b_lines, 0, "標題：", mhdr.title, TTLEN + 1, GCARRY);

  if (HAS_PERM(PERM_ALLBOARD))  /* itoc.000213: 原作者只能改標題 */
  {
    vget(b_lines, 0, "作者：", mhdr.owner, 73 /* sizeof(mhdr.owner) */, GCARRY);
		/* Thor.980727: sizeof(mhdr.owner) = 80 會超過一行 */
    vget(b_lines, 0, "暱稱：", mhdr.nick, sizeof(mhdr.nick), GCARRY);
    vget(b_lines, 0, "日期：", mhdr.date, sizeof(mhdr.date), GCARRY);
  }

  if (memcmp(fhdr, &mhdr, sizeof(HDR)) && vans(msg_sure_ny) == 'y')
  {
    memcpy(fhdr, &mhdr, sizeof(HDR));
    currchrono = fhdr->chrono;
    rec_put(xo->dir, fhdr, sizeof(HDR), xo->key == XZ_XPOST ? fhdr->xid : pos, cmpchrono);

    move(3 + cur, 0);
    post_item(++pos, fhdr);

    /* itoc.010709: 修改文章標題順便修改內文的標題 */
    header_replace(xo, fhdr);
  }
  return XO_FOOT;
}


/* ----------------------------------------------------- */
/* 額外功能：write / score				 */
/* ----------------------------------------------------- */


int
post_write(xo)			/* itoc.010328: 丟線上作者水球 */
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
     /* 直接把 .DIR 中的 score 更新，不管 XO 裡面的 score 是記錄多少 */
      hdd->score += curraddscore;
  }
  else
  {
    if (hdd->score > -127)
     /* 直接把 .DIR 中的 score 更新，不管 XO 裡面的 score 是記錄多少 */
      hdd->score += curraddscore;
  }
}
}

/* 080813.cache: 板主可以清除推文的分數欄 */

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
    switch (vans("◎評分設定 1)自訂 2)清除 3)重新計算 [Q] "))
    {
    case '1':
      if (!vget(b_lines, 0, "請輸入數字：", ans, 3, DOECHO))
        return XO_FOOT;
      pm = vans("請選擇正負 1)正 2)負 [Q] ");
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
        vmsg("調整失敗");
        return XO_FOOT;
      }
      hdr->xmode = xmode |= POST_SCORE; /* 原文可能無評分 */
      hdr->score = score;
      break;

    case '2':
      hdr->xmode = xmode & ~POST_SCORE; /* 清除就不需要POST_SCORE了 */
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
    vmsg("本篇文章沒有推文");
    return XO_FOOT;
  }

  }
  else
    {
      vmsg("您的權限不足！");
      return XO_FOOT;
    }
}

int
post_score(xo)
  XO *xo;
{
  static time_t next2 = 0;   /* 080417.cache: 下次可推文時間 */
  static time_t next = 0;    /* 080417.cache: 推文變註解時間 */
  HDR *hdr;
  int pos, cur, ans, vtlen, maxlen , cando, eof, plus, cmt, userno;
  char *dir, *userid, *verb, fpath[64], reason[80], defverb[2], pushverb, vtbuf[18];
  FILE *fp;
#ifdef HAVE_ANONYMOUS
  char uid[IDLEN + 1];
#endif

  if (!(currbattr & BRD_SCORE) || !cuser.userlevel || !(bbstate & STAT_POST))/* 推文視同發表文章 */
    return XO_NONE;

 /*通知還有多少秒才可以推文*/
 if ((ans = next - time(NULL)) > 0)
 {
   sprintf(fpath, "還有 %d 秒才可以評論文章喔", ans);
   vmsg(fpath);
   return XO_FOOT;
 }

  pos = xo->pos;
  cur = pos - xo->top;
  hdr = (HDR *) xo_pool + cur;

#ifdef HAVE_LOCKEDMARK
  if((hdr->xmode & POST_LOCKED) && !(bbstate & STAT_BM))  /* 鎖定文章不能評分 */
    {
      vmsg("被鎖定文章不能推文");
      return XO_NONE;
    }
#endif

#ifdef HAVE_REFUSEMARK
  if (!chkrestrict(hdr))
    return XO_NONE;
#endif

 if (next2 < time(NULL))
 {

//初始化以防萬一
   cando = 0;
   plus = 0;
   cmt = 0; /*090710.syntaxerror: 增加註解的旗標*/

/*080419.cache: 暫時修改成這樣*/
//switch (ans = vans("◎ 評論 1)推文 2)噓文 3)自定推 4)自定噓 5)註解(尚未開放)？[Q] "))
  switch (ans = vans("◎ 評論 1)推文 2)噓文 3)註解 ？[Q] "))
  {
  case '1':
    verb = "\033[1;31m推\033[m";
    vtlen = 2;
    cando = 1;
    plus = 1;
    break;

  case '2':
    verb = "\033[1;32m噓\033[m";
    vtlen = 2;
    cando = 1;
    break;

  case '3':
    verb = "\033[1;33m→\033[m";
    cmt = 1;
    cando = 0;
    break;

  case '4':
    pushverb = vget(b_lines, 0, "請輸入自訂的正面動詞：", defverb, 3, DOECHO);
    eof = strlen(defverb);
    if(eof<2)
      {
        zmsg("動詞須為一個中文字元或者兩個英文字元");
        return XO_FOOT;
      }
    sprintf(verb = vtbuf, "\033[1;31m%s\033[m", defverb);
    vtlen = 2;
    cando = 1;
    plus = 1;
    break;

  case '5':
     pushverb = vget(b_lines, 0, "請輸入自訂的負面動詞：", defverb, 3, DOECHO);
     eof = strlen(defverb);
     if(eof<2)
       {
         zmsg("動詞須為一個中文字元或者兩個英文字元");
         return XO_FOOT;
       }
     sprintf(verb = vtbuf, "\033[1;32m%s\033[m", defverb);
     vtlen = 2;
     cando = 1;
     break;
//舊的 itoc 自訂動詞
//  case '3':
//  case '4':
//    if (!vget(b_lines, 0, "請輸入自訂內容：", fpath, 3, DOECHO))
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
   verb = "\033[1;33m→\033[m";
 }

#ifdef HAVE_ANONYMOUS
  if (currbattr & BRD_ANONYMOUS)
    maxlen = 64 - IDLEN - vtlen + 2;
  else
#endif
    //maxlen = 64 - strlen(cuser.userid) - vtlen;
    maxlen = 64 - 10 - vtlen;
//改掉strlen(cuser.userid)直接算長度
  if (!vget(b_lines, 0, "請 輸 入 理 由 ： ", reason, maxlen, DOECHO))
    return XO_FOOT;

#ifdef HAVE_ANONYMOUS
  if (currbattr & BRD_ANONYMOUS)
  {
    userid = uid;
    if (!vget(b_lines, 0, "請輸入您想用的ID，也可直接按[Enter]，或是按[r]用真名：", userid, IDLEN, DOECHO))
      userid = STR_ANONYMOUS;
    else if (userid[0] == 'r' && userid[1] == '\0')
      userid = cuser.userid;
    else
      strcat(userid, ".");		/* 自定的話，最後加 '.' */
    //maxlen = 64 - strlen(userid) - vtlen;
    maxlen = 64 - 10 - vtlen;
//改掉strlen(userid)直接算長度
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

    fprintf(fp, "\033[1;36m%13s %s：%-*s%02d/%02d/%02d\n",
      userid, verb, maxlen, reason,
      ptime->tm_year % 100, ptime->tm_mon + 1, ptime->tm_mday);
    fclose(fp);
  }

  curraddscore = 0;

 if (cando > 0)
   {
   if (plus == 1/*(ans - '0') & 0x01*/)	/* 加分 */
     {
       if (hdr->score < 127) { /* cache.080730: 爆的文章要推噓20次以上才會改變 */
         curraddscore = 1;

         if ((hdr->score == 19) && !(currbattr & BRD_PUSH)) { /* cache.090817: 限制非連推看板 */
           if((userno = acct_userno(hdr->owner)) > 0)
             {
                extern UCACHE *ushm;
                int online = 0;
                char buf[20];
                UTMP *uentp, *uceil;
                uentp = ushm->uslot;
                uceil = (void *) uentp + ushm->offset;

                do {
                     if(uentp->userno == userno)/* 在線上，寫入 utmp-> */
                       {
                       uentp->goodart++;
                       online = 1;
                       sprintf(buf, "作者 %s 優文篇數 +1", uentp->userid );
                       }
                } while (++uentp <= uceil);

                if(online)
                   vmsg(buf);
                else /* 不在線上，寫入 .ACCT */
                  {
                     ACCT acct;
                     if(acct_load(&acct, hdr->owner) >= 0)
                       {
                          acct.goodart++;
                          acct_save(&acct);
                       sprintf(buf, "作者 %s 優文篇數 +1", acct.userid );
                          vmsg(buf);
                       }
                   }//online
             }//if userno
          }//hdr->score == 19
        }//hdr->score < 127
     }//if
   else				/* 扣分 */
     {
       if (hdr->score > -127) { /* cache.080730: 爆的文章要推噓20次以上才會改變 */
         curraddscore = -1;

         if ((hdr->score == 20) && !(currbattr & BRD_PUSH)) { /* cache.090817: 限制非連推看板 */
           if((userno = acct_userno(hdr->owner)) > 0)
             {
                extern UCACHE *ushm;
                int online = 0;
                char buf[20];
                UTMP *uentp, *uceil;
                uentp = ushm->uslot;
                uceil = (void *) uentp + ushm->offset;

                do {
                     if(uentp->userno == userno)/* 在線上，寫入 utmp-> */
                       {
                       uentp->goodart--;
                       online = 1;
                       sprintf(buf, "作者 %s 優文篇數 -1", uentp->userid );
                       }
                } while (++uentp <= uceil);

                if(online)
                   vmsg(buf);
                else /* 不在線上，寫入 .ACCT */
                  {
                     ACCT acct;
                     if(acct_load(&acct, hdr->owner) >= 0)
                       {
                          acct.goodart--;
                          acct_save(&acct);
                       sprintf(buf, "作者 %s 優文篇數 -1", acct.userid );
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
      next2 = time(NULL) + 0;       /* 080731.cache: 可以連推 */
      next = time(NULL)  + 0;
    }

  else
    {
      next2 = time(NULL) + 60;      /* 每 60 秒方可推噓文一次 */
      next = time(NULL)  + 10;      /* 每 10 秒方可評論一次 */
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

  }else if(cmt)  //記錄註解的時間
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
  reason3 = "[m：";

  counta = 0;
  countb = 0;

  hdr = (HDR *) xo_pool + (xo->pos - xo->top);
  hdr_fpath(fpath, xo->dir, hdr);

  if (!(fpr = fopen(fpath, "r")))
    {
       vmsg("推文數重新計算失敗");
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

      hdr->xmode |= POST_SCORE; /* 原文可能無評分 */
      hdr->score = 0;
      hdr->score = counta-countb;
      currchrono = hdr->chrono;
      rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono);
      move(3 + cur, 7);
      outc(post_attr(hdr));

         /* 090817.cache: 同步檢查優劣文 */
         if (((counta-countb) == 20) && !(ori == 20)) { //降低誤判機率
           if((userno = acct_userno(hdr->owner)) > 0)
             {
                extern UCACHE *ushm;
                int online = 0;
                char buf[20];
                UTMP *uentp, *uceil;
                uentp = ushm->uslot;
                uceil = (void *) uentp + ushm->offset;

                do {
                     if(uentp->userno == userno)/* 在線上，寫入 utmp-> */
                       {
                       uentp->goodart++;
                       online = 1;
                       sprintf(buf, "作者 %s 優文篇數 +1", uentp->userid );
                       }
                } while (++uentp <= uceil);

                if(online)
                   vmsg(buf);
                else /* 不在線上，寫入 .ACCT */
                  {
                     ACCT acct;
                     if(acct_load(&acct, hdr->owner) >= 0)
                       {
                          acct.goodart++;
                          acct_save(&acct);
                       sprintf(buf, "作者 %s 優文篇數 +1", acct.userid );
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

      hdr->xmode &= ~POST_SCORE; /* cache.090612: 強制設為無推文 */
      hdr->score = 0;
      currchrono = hdr->chrono;
      rec_put(xo->dir, hdr, sizeof(HDR), xo->key == XZ_XPOST ? hdr->xid : pos, cmpchrono);
  }

  vmsg("推文數重新計算完畢");

  return XO_HEAD;
}


/*cache.080520:看板資訊*/
/* ----------------------------------------------------- */
/* 額外功能：write / score                               */
/* ----------------------------------------------------- */

static int
post_showBRD_setting(xo)
  XO *xo;
{
  char *blist;
  int bno;

  bno = brd_bno(currboard);
  blist = (bshm->bcache + bno)->BM; /* 檢視板主清單用 */

  grayout(0, b_lines - 15, GRAYOUT_DARK);

  move(b_lines - 15, 0);
  clrtobot();  /* 避免畫面殘留 */

  prints("\033[1;34m"MSG_BLINE"\033[m");
  prints("\n\033[1;33;44m \033[37m看板設定及資訊查詢： %*s \033[m\n", 55,"");

  prints("\n \033[1;37m★\033[m 看板性質 - %s",
  (currbattr & BRD_NOTRAN) ? "站內看板 (站內看板預設為本站貼文)" : "\033[1;33m轉信看板\033[m (轉信看板預設為跨站貼文)");
  prints("\n \033[1;37m★\033[m 記錄篇數 - %s",
  (currbattr & BRD_NOCOUNT) ? "不做記錄 (文章數目不會列入統計)" : "\033[1;33m記錄\033[m     (文章數目將會列入統計)");
  prints("\n \033[1;37m★\033[m 熱門話題 - %s",
  (currbattr & BRD_NOSTAT) ? "不做記錄 (看板不會列入熱門話題統計)" : "\033[1;33m記錄\033[m     (看板將會列入熱門話題統計)");
  prints("\n \033[1;37m★\033[m 投票結果 - %s",
  (currbattr & BRD_NOVOTE) ? "不做記錄 (投票結果不會公佈在 record 板)" : "\033[1;33m記錄\033[m     (投票結果將會公佈在 \033[1;32mrecord\033[m 板)");
  prints("\n \033[1;37m★\033[m 匿名功\能 - %s",
  (currbattr & BRD_ANONYMOUS) ? "\033[1;33m開啟\033[m     (發表文章可以匿名)" : "關閉     (發表文章不能匿名)");
  prints("\n \033[1;37m★\033[m 推文功\能 - %s",
  (currbattr & BRD_SCORE) ? "\033[1;33m開啟\033[m     (可以推噓文)" : "關閉     (只能回文)");
  prints("\n \033[1;37m★\033[m 連推設定 - %s",
  (currbattr & BRD_PUSH) ? "\033[1;33m開啟\033[m     (推噓文/評論文章不受時間限制)" : "關閉     (再次推噓文要等待\033[1;32m 60 \033[m秒/評論文章要等待\033[1;32m 10 \033[m秒)");
  prints("\n \033[1;37m★\033[m 文章類別 - %s",
  (currbattr & BRD_PREFIX) ? "\033[1;33m開啟\033[m     (發文時可選擇板主訂定的文章類別)" : "關閉     (發文時無文章類別可供選擇)");
  prints("\n \033[1;37m★\033[m 進板記錄 - %s",
  (currbattr & BRD_USIES) ? "\033[1;33m開啟\033[m     (記錄使用者進入看板次數)" : "關閉     (不會記錄使用者進入看板次數)");

  if (cuser.userlevel)
  {
    prints("\n\n 看板名稱 [\033[1;33m%s\033m] 板主 [\033[1;33m%s\033[m]", currboard, blist[0] > ' ' ? blist : "徵求中");
//太長的板主名單會導致這邊超出一行故註解之
//    prints(" 您目前 %s 此看板的管理權限",
//      ((cuser.userlevel & PERM_BM) && blist[0] > ' ' && is_bm(blist,
//        cuser.userid)) ? "\033[1;33m擁有\033[m" : "\033[1;36m沒有\033[m");
  }
  else
  {
    prints("\n\n 看板名稱 [\033[1;33m%s\033m] 板主 [\033[1;33m%s\033[m]", currboard, blist[0] > ' ' ? blist : "徵求中");
//    prints(" 您目前身分為 \033[1;36mguest\033[m 訪客");
  }
  vmsg(NULL);

  return XO_HEAD;
}

/*cache.080520:觀看文章屬性*/
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
      prints("\n\033[1;33;44m \033[37m文章代碼及資訊查詢： %*s \033[m", 55,"");
      outs("\n\n \033[1;37m★\033[m 文章索引: ");
      outs(dir);
      outs("\n \033[1;37m★\033[m 文章代碼: ");
      outs("\033[1;32m");
      outs(ghdr->xname);
      outs("\033[m");
      outs("\n \033[1;37m★\033[m 文章位置: ");
      outs(fpath);

      if (!stat(fpath, &st))
        prints("\n \033[1;37m★\033[m 最後編輯: %s\n \033[1;37m★\033[m 檔案大小: \033[1;32m%d\033[m bytes", Btime(&st.st_mtime), st.st_size);

      prints("\n\n 這一篇文章累計價值 \033[1;33m%d\033[m 銀", ((int)st.st_size)/20);
    }
  else if (!(cuser.userlevel))
    {
       vmsg("您的權限不足");
       return XO_HEAD;
    }
  else
    {

      grayout(0, b_lines - 9, GRAYOUT_DARK);
      move(b_lines - 9, 0);
      clrtobot();

      prints("\033[1;34m"MSG_BLINE"\033[m");
      prints("\n\033[1;33;44m \033[37m文章代碼及資訊查詢： %*s \033[m", 55,"");
      outs("\n\n \033[1;37m★\033[m 文章代碼: ");
      outs("\033[1;32m");
      outs(ghdr->xname);
      outs("\033[m");

      if (!stat(fpath, &st))
        prints("\n \033[1;37m★\033[m 最後存取: %s\n \033[1;37m★\033[m 檔案大小: \033[1;32m%d\033[m bytes", Btime(&st.st_mtime), st.st_size);

      prints("\n\n 這一篇文章累計價值 \033[1;33m%d\033[m 銀", ((int)st.st_size)/20);
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

  if (!cuser.userlevel || !(bbstate & STAT_POST))       /* 評分視同發表文章 */
    return XO_NONE;

  if (!(bbstate & STAT_BOARD))
  {
    vmsg("您的權限不足！");
    return XO_FOOT;
  }

  if (!vget(b_lines, 0, "要刪除的評論(關鍵字)：", reason, 50, DOECHO))
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
      if (!strncmp(buf, "\033[1;32m□ Origin:\033[m", 20)) //站籤
        start_quote = 1;
    }
    else {
                /*判斷是否為該使用者的推文之方式有待探討*/
        if ((bbstate & STAT_BOARD) || !strncmp(buf, userid, len)) { //板主或自己的推文

	    if (strstr(buf, reason)){ // +35: 移動到文字位置
	    	if (nomodify && !strncmp(buf, "\033[1;32m◆ Modify:\033[m \033[1;37m", 28))
	      	    continue;
    		else if (!strncmp(buf, "\033[1;36m", 7) &&
    			(!strncmp(buf + 20, " \033[1;31m", 8) || !strncmp(buf+20, " \033[1;32m", 8) || !strncmp(buf+20, " \033[1;33m", 8))){
    			/*推文格式: 0 - 6 : \033[1;36m
		   	            7 - 19 : ID
		    		    20 - 27: ' ' + \033[1;3(1,2,3)m */
	  	   if(!fpL)
	  	   {
                sprintf(log,"[DELETE][BOARDNAME: %s ][XNAME: %s ][BY %s %02d:%02d ]\n", currboard, hdr->xname, cuser.userid,ptime->tm_hour,ptime->tm_min);
                f_cat(fdlog,log);   // 每次刪除log只寫入一次
                fpL = fopen(fdlog, "a");
           }

		   fputs(buf, fpL);

                   sprintf(buf, "\033[1;36m%13s\033[m \033[1;33m→\033[m："   /*此格式暫先不要更動*/
                     "%-51s %02d/%02d/%02d\n",
                   cuser.userid, "此評論已被刪除",
                   ptime->tm_year % 100, ptime->tm_mon + 1, ptime->tm_mday);
    	         }
			    /*維基與刪除紀錄本身即不合推文格式，因ID後面直接連接ANSI控制碼*/
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

  /* 保存目前所在的位置 */
  currpos = xo->pos;
  /* 紀錄看板文章總數；新貼文AID是未知的不可能在其中 */
  max = xo->max;

  /* 若沒有文章或其他例外狀況 */
  if (max <= 0)
    return XO_FOOT;

  /* 請求使用者輸入文章代碼(AID) */
  if (!vget(b_lines, 0, "請輸入文章代碼(AID)： #", aid, sizeof(aid), DOECHO))
    return XO_FOOT;
  query = aid;

  for (pos = 0; pos < max; pos++)
  {
    xo->pos = pos;
    xo_load(xo, sizeof(HDR));
    /* 設定HDR資訊 */
    hdr = (HDR *) xo_pool + (xo->pos - xo->top);
    tag = hdr->xname;
    /* 若找到對應的文章，則設定match並跳出 */
    if (!strcmp(query, tag))
    {
      match = 1;
      break;
    }
  }

  /* 沒找到則恢復xo->pos並顯示提示文字 */
  if (!match)
  {
    zmsg("找不到文章，是不是找錯看板了呢？");
    xo->pos = currpos;  /* 恢復xo->pos紀錄 */
  }

  return post_load(xo);
}

static int
post_help(xo)
  XO *xo;
{

  xo_help("post");
 /* return post_head(xo); */
  return XO_HEAD;		/* itoc.001029: 與 xpost_help 共用 */
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
  'x', post_cross,		/* 在 post/mbox 中都是小寫 x 轉看板，大寫 M 轉使用者 */
  'M', post_forward,
  't', post_tag,
  'E', post_edit,
  'T', post_title,
  'm', post_mark,
  '_', post_bottom,
  'D', post_rangedel,
  'i', post_showBRD_setting,  /* cache.080520: 看板資訊顯示取[I]nfo首字 比較好記 :p */
  'q', post_state,            /* cache.080520: 觀看文章屬性 */
  'O', XoBM_Refuse_pal,       /* cache.090124: 加密文章可見名單 */
  '#', post_aid,              /* cache.090612: 以文章代碼(AID)快速尋文 */

#ifdef HAVE_SCORE
  'X', post_score,
  '%', post_editscore,
  'Z', XoXscore,                /* itoc.030530: hpo14.080814: 搜尋推文分數 */
  'o', post_resetscore,         /* cache.090127: 賦予推文特定數目 */
  'K', post_delscore,           /* cache.090207: 清除特定推文 */
#endif

  'w', post_write,

  'b', post_memo,
  'c', post_copy,
  'g', gem_gather,

  Ctrl('P'), post_add,
  Ctrl('D'), post_prune,
  Ctrl('Q'), xo_uquery,
  Ctrl('O'), xo_usetup,
  Ctrl('S'), post_done_mark,    /* qazq.030815: 站方處理完成標記Ｓ*/
  Ctrl('B'), depart_post,       /* cache.080812: 轉錄文章至某特定類別 */
  Ctrl('W'), post_wiki,         /* cache.090127: wiki */

#ifdef HAVE_REFUSEMARK
  Ctrl('Y'), post_refuse,
#endif

#ifdef HAVE_LABELMARK
  'n', post_label,
  Ctrl('N'), post_delabel,
#endif

#ifdef HAVE_LOCKEDMARK
  'l', post_locked, /* hpo14.20070513: 文章鎖定標記！ */
#endif

  'B' | XO_DL, (void *) "bin/manage.so:post_manage",
  'R' | XO_DL, (void *) "bin/vote.so:vote_result",
  'V' | XO_DL, (void *) "bin/vote.so:XoVote",

#ifdef HAVE_TERMINATOR
  Ctrl('X') | XO_DL, (void *) "bin/manage.so:post_terminator",
#endif

  '~', XoXselect,		/* itoc.001220: 搜尋作者/標題 */
  'S', XoXsearch,		/* itoc.001220: 搜尋相同標題文章 */
  'a', XoXauthor,		/* itoc.001220: 搜尋作者 */
  '/', XoXtitle,		/* itoc.001220: 搜尋標題 */
  'f', XoXfull,			/* itoc.030608: 全文搜尋 */
  'G', XoXmark,			/* itoc.010325: 搜尋 mark 文章 */
  'L', XoXlocal,		/* itoc.010822: 搜尋本地文章 */

#ifdef HAVE_XYNEWS
  'u', XoNews,			/* itoc.010822: 新聞閱讀模式 */
#endif

  'h', post_help
};


KeyFunc xpost_cb[] =
{
  XO_INIT, xpost_init,
  XO_LOAD, xpost_load,
  XO_HEAD, xpost_head,
  XO_BODY, post_body,		/* Thor.980911: 共用即可 */

  'r', xpost_browse,
  'y', post_reply,
  't', post_tag,
  'x', post_cross,
  'M', post_forward,
  'c', post_copy,
  'g', gem_gather,
  'm', post_mark,
  'd', post_delete,		/* Thor.980911: 方便板主 */
  'E', post_edit,		/* itoc.010716: 提供 XPOST 中可以編輯標題、文章，加密 */
  'T', post_title,
  'i', post_showBRD_setting,  /* cache.080520:看板資訊顯示 */ /* 取[I]nfo首字 比較好記 :p */
  'q', post_state,            /*cache.080520:觀看文章屬性*/
#ifdef HAVE_SCORE
  'X', post_score,
  'Z', XoXscore,              /* itoc.030530: hpo14.080814: 搜尋推文分數 */
#endif
  'w', post_write,
#ifdef HAVE_REFUSEMARK
  Ctrl('Y'), post_refuse,
#endif
#ifdef HAVE_LABELMARK
  'n', post_label,
#endif

#ifdef HAVE_LOCKEDMARK
  'l', post_locked,     /* hpo14.20070513: 文章鎖定標記！ */
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
  Ctrl('S'), post_done_mark,    /* qazq.030815: 站方處理完成標記Ｓ*/

  'h', post_help		/* itoc.030511: 共用即可 */
};


#ifdef HAVE_XYNEWS
KeyFunc news_cb[] =
{
  XO_INIT, news_init,
  XO_LOAD, news_load,
  XO_HEAD, news_head,
  XO_BODY, post_body,

  'r', XoXsearch,

  'h', post_help		/* itoc.030511: 共用即可 */
};
#endif	/* HAVE_XYNEWS */
