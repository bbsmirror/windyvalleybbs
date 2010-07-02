/*-------------------------------------------------------*/
/* rss.c                                                 */
/*-------------------------------------------------------*/
/* target: RSS Reader for BM                             */
/* create: 2009/04/08                                    */
/* update: 2009/06/24                                    */
/* author: hrs113355.bbs@xdbbs.twbbs.org                 */
/* modify: cache.bbs@bbs.ee.ncku.edu.tw                  */
/*-------------------------------------------------------*/

/* 
 * 090617. �ץ���X���禡�M��X 
 * 090624. �ץ����ܰT�� 
 *
 */

#include "bbs.h"

extern BCACHE *bshm;
extern XZ xz[];
extern int TagNum;

extern char xo_pool[];

#if 0

hrs.090408:

RSS���]�w�ɳQ��b�T�Ӧa�� (�HTest�O http://www.abc.com/rss.xml����)

1. brd/Test/rss.conf

    �o�O�U�ݪO���]�w�A�C�ӬݪO���h�� (has-many) �q�\�����}�A
    �H rssfeed_t �榡�x�s�A���Ԩ� include/struct.h

2. etc/rss/http:#%slash%##%slash%#www.abc.com#%slash%#rss.xml

    �o�O�U�� feed ���}�A�q�\�����ݪO�C��A�w��C�� feed ���}�A
    ���������ǬݪO�q�\���A�åB���������]�w�C
    �H rssbrdlist_t �榡�x�s�A���Ԩ� include/struct.h

3. etc/rss/feed.list

    ���� etc/rss/ �U�����Ǻ��}���]�w�ɡA
    �H rssfeedlist_t �榡�x�s�A���Ԩ� include/struct.h

#endif

//todo �Ҽ{�ΦP�@��chrono�@�ѧO

int rss_cmpchrono(rssfeed_t *feed)
{
    return feed->chrono == currchrono;
}

int rss_fcmpchrono(rssfeedlist_t *feedlist)
{
    return feedlist->chrono == currchrono;
}

int rss_bcmpchrono(rssbrdlist_t *brdlist)
{
    return brdlist->chrono == currchrono;
}

#define ESCAPE_SLASH "#%slash%#"

// todo: check all path length
char slashbuf[512];
char * escape_slash(const char * src)
{
    char *ptr;
    const char *copy;

    memset(slashbuf, 0, sizeof(slashbuf));
    copy = src;
    ptr = slashbuf;

    while (*copy)
    {
if (*copy == '/')
{
    strcat(ptr, ESCAPE_SLASH);
    ptr+= strlen(ESCAPE_SLASH);
}
else
    *ptr++ = *copy;

copy++;
    }
    *ptr = '\0';
    return slashbuf;
}

int ValidURL(const char * s)
{
    // if the url...
    if ((strncmp(s, "http://", 7)        // doesn't start with 'http://'
    && strncmp(s, "https://", 8)) // doen't start with 'https://'
|| strstr(s, ESCAPE_SLASH) // has had escape_slash inside
    || strchr(s, '\"'))     // has had quotation mark inside
return 0; // invalid
    else
return 1; // valid
}

/* ----------------------------------------------------- */
/* �s�W/�R��/�s��                                        */
/* ----------------------------------------------------- */

// declarations
static void rss_brdlist_del(rssfeed_t *);
static void rss_feedlist_del(rssfeed_t *);

static void rss_brdlist_add(rssfeed_t *);
static void rss_feedlist_add(rssfeed_t *);

static void rss_brdlist_update(rssfeed_t *, rssfeed_t *);
static void rss_feedlist_update(rssfeed_t *, rssfeed_t *);

static void rss_item(int, rssfeed_t *);


static void rss_brdlist_del(rssfeed_t *feed)
{
    char fpath[512];
    register int i, size;
    rssbrdlist_t brdlist;

    sprintf(fpath, FN_RSS_FOLDER "%s", escape_slash(feed->feedurl));
    size = rec_num(fpath, sizeof(rssbrdlist_t));

    for (i = 1; i < size; i++)
if (rec_get(fpath, &brdlist, sizeof(brdlist), i) >= 0)
{
    if (!strcmp(brdlist.brdname, currboard)
    && !strcmp(brdlist.owner, feed->owner))
    {
currchrono = brdlist.chrono;
rec_del(fpath, sizeof(rssbrdlist_t), i, rss_bcmpchrono);
break;
    }
}

    if (rec_num(fpath, sizeof(rssbrdlist_t)) <= 1)
    {
unlink(fpath);
rss_feedlist_del(feed);
    }
}

static void rss_feedlist_del(rssfeed_t *feed)
{
    char *fpath = FN_RSS_LIST;
    register int i, size;
    rssfeedlist_t feedlist;

    size = rec_num(fpath, sizeof(rssfeedlist_t));
    for (i = 0; i < size; i++)
if (rec_get(fpath, &feedlist, sizeof(feedlist), i) >= 0)
{
    if (!strcmp(feedlist.feedurl, feed->feedurl))
    {
currchrono = feedlist.chrono;
rec_del(fpath, sizeof(rssfeedlist_t), i, rss_fcmpchrono);
break;
    }
}
}

static void rss_brdlist_add(rssfeed_t *feed)
{
    char fpath[512];
    rssbrdlist_t brdlist;

    memset(&brdlist, 0, sizeof(brdlist));
    str_ncpy(brdlist.brdname, currboard, sizeof(brdlist.brdname));
    str_ncpy(brdlist.owner, feed->owner, sizeof(brdlist.owner));
    str_ncpy(brdlist.prefix, feed->prefix, sizeof(brdlist.prefix));
    brdlist.chrono = time(NULL);
    brdlist.attr = feed->attr;

    sprintf(fpath, FN_RSS_FOLDER "%s", escape_slash(feed->feedurl));
    if (rec_num(fpath, sizeof(brdlist)) <= 0)
    {
rssbrdlist_t update;
memset(&update, 0, sizeof(update));
rec_add(fpath, &update, sizeof(update));
rss_feedlist_add(feed);
    }
    rec_add(fpath, &brdlist, sizeof(brdlist));
}

static void rss_feedlist_add(rssfeed_t *feed)
{
    char *fpath = FN_RSS_LIST;
    rssfeedlist_t feedlist;
    register int i, size;

    size = rec_num(fpath, sizeof(rssfeedlist_t));
    for (i = 0; i < size; i++)
if (rec_get(fpath, &feedlist, sizeof(rssfeedlist_t), i) >= 0 &&
!strcmp(feedlist.feedurl, feed->feedurl))
    return;

    memset(&feedlist, 0, sizeof(feedlist));
    str_ncpy(feedlist.feedurl, feed->feedurl, sizeof(feedlist.feedurl));
    feedlist.chrono = time(NULL);
    rec_add(fpath, &feedlist, sizeof(feedlist));
}

static void rss_brdlist_update(rssfeed_t *feed, rssfeed_t *mfeed) //old & new
{
    int changeURL = strcmp(feed->feedurl, mfeed->feedurl), size, i;
    char fpath[512];
    rssbrdlist_t brdlist;

    sprintf(fpath, FN_RSS_FOLDER "%s", escape_slash(feed->feedurl));
    size = rec_num(fpath, sizeof(rssbrdlist_t));

    if (!changeURL)
    {
for (i = 1; i < size; i++)
    if (rec_get(fpath, &brdlist, sizeof(brdlist), i) >= 0)
    {
if (!strcmp(brdlist.brdname, currboard)
&& !strcmp(brdlist.owner, feed->owner))
{
    currchrono = brdlist.chrono;
    str_ncpy(brdlist.owner, mfeed->owner, sizeof(brdlist.owner));
    str_ncpy(brdlist.prefix, mfeed->prefix, sizeof(brdlist.prefix));
    brdlist.attr = mfeed->attr;
    rec_put(fpath, &brdlist, sizeof(rssbrdlist_t), i, rss_bcmpchrono);
}
    }
    }
    else
    {
rss_brdlist_del(feed);
rss_brdlist_add(mfeed);
rss_feedlist_update(feed, mfeed);
    }
}

static void rss_feedlist_update(rssfeed_t *feed, rssfeed_t *mfeed)
{
    if (!strcmp(feed->feedurl, mfeed->feedurl))
return;

    char *fpath = FN_RSS_LIST;
    register int i, size;
    rssfeedlist_t feedlist;

    size = rec_num(fpath, sizeof(rssfeedlist_t));
    for (i = 0; i < size; i++)
        if (rec_get(fpath, &feedlist, sizeof(feedlist), i) >= 0)
        {
            if (!strcmp(feedlist.feedurl, feed->feedurl))
            {
                currchrono = feedlist.chrono;
str_ncpy(feedlist.feedurl, mfeed->feedurl, sizeof(feedlist.feedurl));
                rec_put(fpath, &feedlist, sizeof(rssfeedlist_t), i, rss_fcmpchrono);
break;
            }
        }
}

static int rss_set(rssfeed_t *feed)
{
    char buf[2];
    int i = 0;
    BRD *brd = bshm->bcache + currbno;

    move(3, 0);
    clrtobot();

    if (!vget(4, 0, "���ҡG", feed->prefix, sizeof(feed->prefix), GCARRY ))
        return -1;

    if (!*(feed->feedurl))
strcpy(feed->feedurl, "http://");
    do
    {
if (i++)
    prints("\n�п�J�Hhttp://��https://�}�Y���}�A�åB�Ф��n��J�S��r��");
    prints("\n�ӷ����}�G\n");
        if (!vget(7, 0, "", feed->feedurl, 80, GCARRY))
            return -1; // todo: expand length
    } while(!ValidURL(feed->feedurl));

    prints("\n�п�J���������G\n");
    if (!vget(10, 0, "", feed->note, sizeof(feed->note), GCARRY ))
        return -1; // todo: expand length

    feed->attr = 0;
    if (!(brd->battr & BRD_NOTRAN) &&
            vget(11, 0, "�аݭn�������峹���ڦs�ɶܡH [y/N] ", buf, 2, LCECHO) &&
            *buf == 'y')
        feed->attr |= RSS_OUTGO;

    vget(13, 0, "�аݷ�峹��s�ɭn����@���� (���i��|�o�쭫�Ƥ��e���峹)�H [y/N] "
    , buf, 2, LCECHO);
    if (*buf == 'y')
feed->attr |= RSS_GETUPDATE;

    vget(15, 0, "�аݤ峹�ͦ��ɬO�_�n����ҦW�٧@���峹���O�H [Y/n] ", buf, 2, LCECHO);
    if (*buf != 'n')
feed->attr |= RSS_USELABEL;

    strncpy(feed->owner, cuser.userid, sizeof(feed->owner));
    return 0;
}

static int rss_add(xo)
    XO *xo;
{
    rssfeed_t feed, check;
    register int i, size;

    memset(&feed, 0, sizeof(feed));
    if (rss_set(&feed) < 0 || vans(msg_sure_ny) != 'y')
return XO_HEAD;

    // check repeat
    size = rec_num (xo->dir, sizeof(rssfeed_t));
    for (i = 0; i < size; i++)
if (rec_get(xo->dir, &check, sizeof(rssfeed_t), i) >= 0 &&
    !strcmp(feed.feedurl, check.feedurl))
{
    vmsg("���~�G�w���ۦP���}");
    return XO_LOAD;
}

    feed.chrono = time(NULL);
    str_stamp(feed.date, &feed.chrono);

    rss_brdlist_add(&feed);
    rec_add(xo->dir, &feed, sizeof(feed));

    return XO_LOAD;
}

static int rss_delete(xo)
    XO *xo;
{
    int pos, cur;
    rssfeed_t *feed;

    pos = xo->pos;
    cur = pos - xo->top;
    feed = (rssfeed_t *) xo_pool + cur;

    if (vans(msg_del_ny) == 'y')
    {
currchrono = feed->chrono;

if (!rec_del(xo->dir, sizeof(rssfeed_t), pos, rss_cmpchrono))
{
    rss_brdlist_del(feed);
    return XO_LOAD;
}
    }
    return XO_FOOT;
}

//todo: tag delete
static int vfyfeed(feed, pos)
  rssfeed_t *feed;
  int pos;
{
      return Tagger(feed->chrono, pos, TAG_NIN);
}


static void delfeed(xo, feed)
  XO *xo;
  rssfeed_t *feed;
{
    rss_brdlist_del(feed);
}

static int
rss_prune(xo)
  XO *xo;
{
  return xo_prune(xo, sizeof(rssfeed_t), vfyfeed, delfeed);
}

static int rss_rangedel(xo)
    XO *xo;
{
    return xo_rangedel(xo, sizeof(rssfeed_t), 0, delfeed);
}

static int rss_edit(XO *xo)
{
    rssfeed_t *feed, mfeed;
    int pos, cur;

    pos = xo->pos;
    cur = pos - xo->top;
    feed = (rssfeed_t *) xo_pool + cur;
    memcpy(&mfeed, feed, sizeof(mfeed));

    if (rss_set(&mfeed) < 0)
return XO_HEAD;

    if (memcmp(feed, &mfeed, sizeof(rssfeed_t)) && vans(msg_sure_ny) == 'y')
    {
rss_brdlist_update(feed, &mfeed);
rss_feedlist_update(feed, &mfeed);

        memcpy(feed, &mfeed, sizeof(rssfeed_t));
        currchrono = feed->chrono;
        rec_put(xo->dir, feed, sizeof(rssfeed_t), pos, rss_cmpchrono);
    }
    return XO_HEAD;
}

static int rss_edit_feedurl(XO *xo)
{
    rssfeed_t *feed, mfeed;
    int pos, cur;

    pos = xo->pos;
    cur = pos - xo->top;
    feed = (rssfeed_t *) xo_pool + cur;
    memcpy(&mfeed, feed, sizeof(mfeed));

    vget(b_lines, 0, "�ӷ����}�G", mfeed.feedurl, 70, GCARRY );
    // todo: expand the length

    if (memcmp(&mfeed,feed, sizeof(rssfeed_t)) && vans(msg_sure_ny) == 'y')
    {
        memcpy(feed, &mfeed, sizeof(rssfeed_t));
currchrono = feed->chrono;
currchrono = feed->chrono;
rec_put(xo->dir, feed, sizeof(rssfeed_t), pos, rss_cmpchrono);
rss_brdlist_update(feed, &mfeed);
rss_feedlist_update(feed, &mfeed);
    }
    return XO_HEAD;
}

static int rss_edit_prefix(XO *xo)
{
    rssfeed_t *feed, mfeed;
    int pos, cur;

    pos = xo->pos;
    cur = pos - xo->top;
    feed = (rssfeed_t *) xo_pool + cur;
    memcpy(&mfeed, feed, sizeof(mfeed));

    vget(b_lines, 0, "���ҡG", mfeed.prefix, sizeof(mfeed.prefix), GCARRY );
    if (memcmp(&mfeed,feed, sizeof(rssfeed_t)) && vans(msg_sure_ny) == 'y')
    {
        memcpy(feed, &mfeed, sizeof(rssfeed_t));
currchrono = feed->chrono;
rec_put(xo->dir, feed, sizeof(rssfeed_t), pos, rss_cmpchrono);
rss_brdlist_update(feed, &mfeed);
rss_feedlist_update(feed, &mfeed);
    }
    return XO_HEAD;
}

static int rss_edit_filter(XO *xo)
{
    rssbrdlist_t brdlist, newbrdlist;
    rssfeed_t *feed;
    register int pos, cur, i, size;
    char fpath[512];

    pos = xo->pos;
    cur = pos - xo->top;
    feed = (rssfeed_t *) xo_pool + cur;
    sprintf (fpath, FN_RSS_FOLDER "%s", escape_slash(feed->feedurl));
    size = rec_num(fpath, sizeof(rssbrdlist_t));

    // todo: error control
    for (i = 1; i < size; i++)
if (rec_get(fpath, &brdlist, sizeof(brdlist), i) >= 0)
    if (!strcmp(brdlist.brdname, currboard) &&
    !strcmp(brdlist.owner, feed->owner))
break;
    memcpy(&newbrdlist, &brdlist, sizeof(brdlist));

    move(3, 0);
    clrtobot();

    outs("�s��զW��: �H�׽u���j�զW��L�o����r");
    outs("\033[1;30m�Ҧp: �Ш|/�F�v/�g��/�~��");
    vget(6, 0, "", newbrdlist.white, sizeof(newbrdlist.white), GCARRY);

    outs("�s��¦W��: �H�׽u���j�¦W��L�o����r");
    outs("\033[1;30m�Ҧp: �ȿ���k/�s�i/���/spam/foo/bar");
    vget(10, 0, "", newbrdlist.black, sizeof(newbrdlist.black), GCARRY);

    if (memcmp(&brdlist, &newbrdlist, sizeof(brdlist)) && vans(msg_sure_ny) == 'y')
    {
rec_put(fpath, &newbrdlist, sizeof(newbrdlist), i, NULL);
    }
    return XO_HEAD;
}

static int rss_query(XO *xo)
{
    rssbrdlist_t brdlist;
    rssfeed_t *feed;
    register int pos, cur, size, i;
    char fpath[256];
    time_t update;

    pos = xo->pos;
    cur = pos - xo->top;
    feed = (rssfeed_t *) xo_pool + cur;

    sprintf (fpath, FN_RSS_FOLDER "%s", escape_slash(feed->feedurl));
    size = rec_num(fpath, sizeof(rssbrdlist_t));

    if (rec_get(fpath, &brdlist, sizeof(brdlist), 0) >= 0)
update = brdlist.update;
    else
update = 0;

    brdlist.update = 0;
    for (i = 1; i < size; i++)
            if (rec_get(fpath, &brdlist, sizeof(brdlist), i) >= 0)
                if (!strcmp(brdlist.brdname, currboard)
                        && !strcmp(brdlist.owner, feed->owner))
break;

    move(3, 0);
    clrtobot();

    move(4, 0);

    if (feed->attr & RSS_STOP)
      prints("\033[1;31m [�ثe�����������A]" ANSIRESET "\n");
    prints( "��    ��: %s" ANSIRESET "\n", feed->prefix);
    prints( "�q�\\�H��: %s" ANSIRESET "\n", feed->owner);
    prints( "�q�\\�ӷ�: %s" ANSIRESET "\n", feed->feedurl);
    prints( "�إߤ��: %s" ANSIRESET "\n", Btime(&(feed->chrono)));
    prints( "�̷s�峹: %s" ANSIRESET "\n",
    (brdlist.update <= 0 ? "�|���}�l" : Btime(&(brdlist.update))));
    prints( "�̪���: %s" ANSIRESET "\n",
    (update <= 0 ? "�|���}�l" : Btime(&(update))));
    prints( "���ڦs��: %s" ANSIRESET "\n", (feed->attr & RSS_OUTGO ? "��" : "��"));
    prints( "�����s: %s" ANSIRESET "\n", (feed->attr & RSS_GETUPDATE ? "��" : "��"));
    prints( "�ϥμ��ҦW�@���峹���O: %s" ANSIRESET "\n", (feed->attr & RSS_USELABEL ? "��" : "��"));
    prints( "�ԭz����: %s" ANSIRESET "\n\n", feed->note);

    if (*(brdlist.white))
prints( "�L�o�զW��:\n \033[0;30;47m %-*s" ANSIRESET "\n", sizeof(brdlist.white), brdlist.white);

    if (*(brdlist.black))
prints( "�L�o�¦W��:\n \033[0;30;47m %-*s" ANSIRESET "\n", sizeof(brdlist.black), brdlist.black);

    vmsg(NULL);
    return XO_HEAD;
}

/* ----------------------------------------------------- */
/* �N��s�ɶ��k�s                                        */
/* ----------------------------------------------------- */

static int rss_reset(XO *xo)
{
    rssbrdlist_t brdlist;
    rssfeed_t *feed;
    register int pos, cur, size, i;
    char fpath[512];

    if (vans("�T�w�n�N��s�ɶ��k�s�A�������� [y/N]�H ") != 'y')
return XO_FOOT;

    pos = xo->pos;
    cur = pos - xo->top;
    feed = (rssfeed_t *) xo_pool + cur;

    sprintf (fpath, FN_RSS_FOLDER "%s", escape_slash(feed->feedurl));
    size = rec_num(fpath, sizeof(rssbrdlist_t));

    for (i = 1; i < size; i++)
            if (rec_get(fpath, &brdlist, sizeof(brdlist), i) >= 0)
            {
                if (!strcmp(brdlist.brdname, currboard)
                        && !strcmp(brdlist.owner, feed->owner))
                {
                    currchrono = brdlist.chrono;
                    brdlist.update = 0;
                    rec_put(fpath, &brdlist, sizeof(rssbrdlist_t), i, rss_bcmpchrono);
    break;
                }
            }
    vmsg("�k�s���\\!");
    return XO_FOOT;
}

/* ----------------------------------------------------- */
/* �Ȱ����                                              */
/* ----------------------------------------------------- */

static int rss_stop(XO *xo)
{
    rssfeed_t *feed, mfeed;
    register int pos, cur;

    pos = xo->pos;
    cur = pos - xo->top;
    feed = (rssfeed_t *) xo_pool + cur;

    memcpy(&mfeed, feed, sizeof(mfeed));

    if (mfeed.attr & RSS_STOP && vans("�{�b�O�Ȱ�������A�A�n��_����� [y/N] ") == 'y')
mfeed.attr &= ~RSS_STOP;
    else if (!(mfeed.attr & RSS_STOP) && vans("�n�Ȱ�����o�� feed �� [y/N]�H ") == 'y')
mfeed.attr |= RSS_STOP;

    if (memcmp(&mfeed, feed, sizeof(mfeed)))
    {
memcpy(feed, &mfeed, sizeof(rssfeed_t));
currchrono = feed->chrono;
rec_put(xo->dir, feed, sizeof(rssfeed_t), pos, rss_cmpchrono);
rss_brdlist_update(feed, &mfeed);
    }

    return XO_HEAD;
}

/*------------------------------------------------------ */
/*��H�]�w                                               */ 
/*------------------------------------------------------ */

static int rss_outgo(XO *xo)
{
    rssfeed_t *feed, mfeed;
    register int pos, cur;

    pos = xo->pos;
    cur = pos - xo->top;
    feed = (rssfeed_t *) xo_pool + cur;

    memcpy(&mfeed, feed, sizeof(mfeed));

    if (mfeed.attr & RSS_OUTGO && vans("�n�������ڦs�ɶ� [y/N] ") == 'y')
mfeed.attr &= ~RSS_OUTGO;
    else if (!(mfeed.attr & RSS_OUTGO) && vans("�n�}�ү��ڦs�ɶ� [y/N]�H ") == 'y')
mfeed.attr |= RSS_OUTGO;

    if (memcmp(&mfeed, feed, sizeof(mfeed)))
    {
memcpy(feed, &mfeed, sizeof(rssfeed_t));
currchrono = feed->chrono;
rec_put(xo->dir, feed, sizeof(rssfeed_t), pos, rss_cmpchrono);
rss_brdlist_update(feed, &mfeed);
    }

    return XO_HEAD;
       
       
}

/* ----------------------------------------------------- */
/* ���ʶ���                                              */
/* ----------------------------------------------------- */

static int rss_move(XO *xo)
{
  rssfeed_t *feed;
  char buf[40];
  int pos, newOrder;

  pos = xo->pos;
  sprintf(buf, "�п�J�� %d �ﶵ���s��m�G", pos + 1);
  if (!vget(b_lines, 0, buf, buf, 5, DOECHO))
    return XO_FOOT;

  feed = (rssfeed_t *) xo_pool + pos - xo->top;

  newOrder = atoi(buf) - 1;
  if (newOrder < 0)
    newOrder = 0;
  else if (newOrder >= xo->max)
    newOrder = xo->max - 1;

  if (newOrder != pos)
  {
    if (!rec_del(xo->dir, sizeof(rssfeed_t), pos, NULL))
    {
      rec_ins(xo->dir, feed, sizeof(rssfeed_t), newOrder, 1);
      xo->pos = newOrder;
      return XO_LOAD;
    }
  }
  return XO_FOOT;
}


/* ----------------------------------------------------- */
/* �s�WTag                                               */
/* ----------------------------------------------------- */

static int
rss_tag(xo)
  XO *xo;
{
  rssfeed_t *feed;
  int tag, pos, cur;

  pos = xo->pos;
  cur = pos - xo->top;
  feed = (rssfeed_t *) xo_pool + cur;

  if (tag = Tagger(feed->chrono, pos, TAG_TOGGLE))
  {
     move(3 + cur, 0);
     rss_item(pos + 1, feed);
  }

  /* return XO_NONE; */
  return xo->pos + 1 + XO_MOVE;
}

/* ----------------------------------------------------- */
/* �ݪO�\��� */
/* ----------------------------------------------------- */


static int rss_body();
static int rss_head();

static int
rss_init(xo)
    XO *xo;
{
    xo_load(xo, sizeof(rssfeed_t));
    return rss_head(xo);
}

static int
rss_load(xo)
    XO *xo;
{
#ifdef HAVE_BRD_EXPLAIN
    XO_TALL = b_lines - 3;
#endif
    xo_load(xo, sizeof(rssfeed_t));
    return rss_body(xo);
}


static void
rss_item(int num, rssfeed_t *feed)
{
    clrtoeol();
    prints("%6d%c"
    "\033[1;3%dm %s" ANSIRESET
    "%s %-15.15s %-48.48s" ANSIRESET "\n"
    , num, (TagNum && !Tagger(feed->chrono, 0, TAG_NIN) ? '*' : ' ')
    , (*(feed->date+6) + *(feed->date+7)) % 8
    , feed->date+3, ( feed->attr & RSS_STOP ? "\033[31m" : "" )
    , feed->prefix, feed->note);
}

static int
rss_body(xo)
    XO *xo;
{
    rssfeed_t *feed;
    int num, max, tail;

    max = xo->max;
    if (max <= 0)
    {
if (vans("�n�s�W�����ƶ�(Y/N)�H[N] ") == 'y')
    return rss_add(xo);
return XO_QUIT;
    }

    feed = (rssfeed_t *) xo_pool;
    num = xo->top;
    tail = num + XO_TALL;
    if (max > tail)
max = tail;

    move(3, 0);
    do
    {
rss_item(++num, feed++);
    } while (num < max);
    clrtobot();

    return XO_FOOT;/* itoc.010403: �� b_lines ��W feeter */
}


static int
rss_head(xo)
    XO *xo;
{
    vs_head("����q�\\��", str_site);
    prints(NECKER_RSS, d_cols, "");
    return rss_body(xo);
}


static int
rss_help(xo)
    XO *xo;
{
    xo_help("rss");
    return rss_head(xo);
}


KeyFunc rss_cb[] =
{
    XO_INIT, rss_init,
    XO_LOAD, rss_load,
    XO_HEAD, rss_head,
    XO_BODY, rss_body,

    'r', rss_query,

    'd', rss_delete,
    'E', rss_edit,
    'H', rss_edit_feedurl,
    'T', rss_edit_prefix,
    'o', rss_edit_filter,
    'D', rss_rangedel,

    'U', rss_reset,
    'K', rss_stop,
    'g', rss_outgo,
    's', rss_load,
    'm', rss_move,

    Ctrl('P'), rss_add,

    'h', rss_help
};
