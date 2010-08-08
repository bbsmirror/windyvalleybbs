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
 * 090617. 修正輸出的函式和色碼
 * 090624. 修正提示訊息
 *
 */

#include "bbs.h"

extern BCACHE *bshm;
extern XZ xz[];
extern int TagNum;

extern char xo_pool[];

#if 0

hrs.090408:

RSS的設定檔被放在三個地方 (以Test板 http://www.abc.com/rss.xml為例)

1. brd/Test/rss.conf

    這是各看板的設定，每個看板有多個 (has-many) 訂閱的網址，
    以 rssfeed_t 格式儲存，欄位詳見 include/struct.h

2. etc/rss/http:#%slash%##%slash%#www.abc.com#%slash%#rss.xml

    這是各個 feed 網址，訂閱它的看板列表，針對每個 feed 網址，
    紀錄有哪些看板訂閱它，並且紀錄部份設定。
    以 rssbrdlist_t 格式儲存，欄位詳見 include/struct.h

3. etc/rss/feed.list

    紀錄 etc/rss/ 下有哪些網址的設定檔，
    以 rssfeedlist_t 格式儲存，欄位詳見 include/struct.h

#endif

//todo 考慮用同一個chrono作識別

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
/* 新增/刪除/編輯                                        */
/* ----------------------------------------------------- */

// declarations
static void rss_brdlist_del(rssfeed_t *);
static void rss_feedlist_del(rssfeed_t *);

static void rss_brdlist_add(rssfeed_t *);
static void rss_feedlist_add(rssfeed_t *);

static void rss_brdlist_update(rssfeed_t *, rssfeed_t *);
static void rss_feedlist_update(rssfeed_t *, rssfeed_t *);

static void rss_item(int, rssfeed_t *);

static void blist_add();
static void blist_del();


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

    if (!vget(4, 0, "標籤：", feed->prefix, sizeof(feed->prefix), GCARRY ))
        return -1;

    if (!*(feed->feedurl))
strcpy(feed->feedurl, "http://");
    do
    {
if (i++)
    prints("\n請輸入以http://或https://開頭網址，並且請不要輸入特殊字元");
    prints("\n來源網址：\n");
        if (!vget(7, 0, "", feed->feedurl, 80, GCARRY))
            return -1; // todo: expand length
    } while(!ValidURL(feed->feedurl));

    prints("\n請輸入相關說明：\n");
    if (!vget(10, 0, "", feed->note, sizeof(feed->note), GCARRY ))
        return -1; // todo: expand length

    feed->attr = 0;
    if (!(brd->battr & BRD_NOTRAN) &&
            vget(11, 0, "請問要把抓取的文章站際存檔嗎？ [y/N] ", buf, 2, LCECHO) &&
            *buf == 'y')
        feed->attr |= RSS_OUTGO;

    vget(13, 0, "請問當文章更新時要重抓一份嗎 (有可能會得到重複內容的文章)？ [y/N] "
    , buf, 2, LCECHO);
    if (*buf == 'y')
feed->attr |= RSS_GETUPDATE;

    vget(15, 0, "請問文章生成時是否要把標籤名稱作為文章類別？ [Y/n] ", buf, 2, LCECHO);
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
    vmsg("錯誤：已有相同網址");
    return XO_LOAD;
}

if ( size == 0)
   blist_add(); // 新增第一筆資料時紀錄到brdlist.list

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

    vget(b_lines, 0, "來源網址：", mfeed.feedurl, 70, GCARRY );
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

    vget(b_lines, 0, "標籤：", mfeed.prefix, sizeof(mfeed.prefix), GCARRY );
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

    outs("編輯白名單: 以斜線分隔白名單過濾關鍵字");
    outs("\033[1;30m例如: 教育/政治/經濟/外交");
    vget(6, 0, "", newbrdlist.white, sizeof(newbrdlist.white), GCARRY);

    outs("編輯黑名單: 以斜線分隔黑名單過濾關鍵字");
    outs("\033[1;30m例如: 賺錢方法/廣告/減肥/spam/foo/bar");
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
      prints("\033[1;31m [目前為停止抓取狀態]" ANSIRESET "\n");
    prints( "標    籤: %s" ANSIRESET "\n", feed->prefix);
    prints( "訂閱\人員: %s" ANSIRESET "\n", feed->owner);
    prints( "訂閱\來源: %s" ANSIRESET "\n", feed->feedurl);
    prints( "建立日期: %s" ANSIRESET "\n", Btime(&(feed->chrono)));
    prints( "最新文章: %s" ANSIRESET "\n",
    (brdlist.update <= 0 ? "尚未開始" : Btime(&(brdlist.update))));
    prints( "最近抓取: %s" ANSIRESET "\n",
    (update <= 0 ? "尚未開始" : Btime(&(update))));
    prints( "站際存檔: %s" ANSIRESET "\n", (feed->attr & RSS_OUTGO ? "○" : "╳"));
    prints( "抓取更新: %s" ANSIRESET "\n", (feed->attr & RSS_GETUPDATE ? "○" : "╳"));
    prints( "使用標籤名作為文章類別: %s" ANSIRESET "\n", (feed->attr & RSS_USELABEL ? "○" : "╳"));
    prints( "敘述說明: %s" ANSIRESET "\n\n", feed->note);

    if (*(brdlist.white))
prints( "過濾白名單:\n \033[0;30;47m %-*s" ANSIRESET "\n", sizeof(brdlist.white), brdlist.white);

    if (*(brdlist.black))
prints( "過濾黑名單:\n \033[0;30;47m %-*s" ANSIRESET "\n", sizeof(brdlist.black), brdlist.black);

    vmsg(NULL);
    return XO_HEAD;
}

/* ----------------------------------------------------- */
/* 將更新時間歸零                                        */
/* ----------------------------------------------------- */

static int rss_reset(XO *xo)
{
    rssbrdlist_t brdlist;
    rssfeed_t *feed;
    register int pos, cur, size, i;
    char fpath[512];

    if (vans("確定要將更新時間歸零，重抓ＲＳＳ嗎 [y/N]？ ") != 'y')
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
    vmsg("歸零成功\!");
    return XO_FOOT;
}

/* ----------------------------------------------------- */
/* 暫停抓取                                              */
/* ----------------------------------------------------- */

static int rss_stop(XO *xo)
{
    rssfeed_t *feed, mfeed;
    register int pos, cur;

    pos = xo->pos;
    cur = pos - xo->top;
    feed = (rssfeed_t *) xo_pool + cur;

    memcpy(&mfeed, feed, sizeof(mfeed));

    if (mfeed.attr & RSS_STOP && vans("現在是暫停抓取狀態，要恢復抓取嗎 [y/N] ") == 'y')
mfeed.attr &= ~RSS_STOP;
    else if (!(mfeed.attr & RSS_STOP) && vans("要暫停抓取這個 feed 嗎 [y/N]？ ") == 'y')
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
/*轉信設定                                               */
/*------------------------------------------------------ */

static int rss_outgo(XO *xo)
{
    rssfeed_t *feed, mfeed;
    register int pos, cur;

    pos = xo->pos;
    cur = pos - xo->top;
    feed = (rssfeed_t *) xo_pool + cur;

    memcpy(&mfeed, feed, sizeof(mfeed));

    if (mfeed.attr & RSS_OUTGO && vans("要關閉站際存檔嗎 [y/N] ") == 'y')
mfeed.attr &= ~RSS_OUTGO;
    else if (!(mfeed.attr & RSS_OUTGO) && vans("要開啟站際存檔嗎 [y/N]？ ") == 'y')
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
/* 移動項目                                              */
/* ----------------------------------------------------- */

static int rss_move(XO *xo)
{
  rssfeed_t *feed;
  char buf[40];
  int pos, newOrder;

  pos = xo->pos;
  sprintf(buf, "請輸入第 %d 選項的新位置：", pos + 1);
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
/* 新增Tag                                               */
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
/* 看板功能表 */
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

if (vans("要新增ＲＳＳ資料嗎(Y/N)？[N] ") == 'y')
    return rss_add(xo);


    blist_del();  // 沒資料時刪掉brdlist.list裡的東西

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

    return XO_FOOT;/* itoc.010403: 把 b_lines 填上 feeter */
}


static int
rss_head(xo)
    XO *xo;
{
    vs_head("ＲＳＳ訂閱\器", str_site);
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
/* ------------------------------------*/
/* 更新全站訂閱列表 */
/* ------------------------------------*/

static void
blist_add()
{
 char *fpath = "etc/rss/brdlist.list";
 int (*sync_func)();

 sync_func=str_cmp;


 rec_add(fpath,currboard, sizeof(currboard));
 rec_sync(fpath, sizeof(currboard), sync_func, NULL);

}

static void
blist_del()
{
  char *fpath = "etc/rss/brdlist.list";
  char brd_t[BNLEN + 1];
  register int size ,i ;


  size = rec_num(fpath, sizeof(brd_t));

      for (i = 0; i < size; i++)
if (rec_get(fpath,brd_t, sizeof(brd_t), i) >= 0 &&
 !strcmp(brd_t,currboard))
{

rec_del(fpath, sizeof(brd_t), i,NULL);
break;

}

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
