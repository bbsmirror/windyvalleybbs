/*-------------------------------------------------------*/
/* rssadmin.c	( NTHU CS MapleBBS Ver 3.10 )		 */
/*-------------------------------------------------------*/
/* target : rss全站控管					 */
/* create :   /  /				    	 */
/* update :   /  /  					 */
/* author :   	                         */
/*-------------------------------------------------------*/
/* note   :
/*-------------------------------------------------------*/

#include "bbs.h"


extern BCACHE *bshm;
extern XZ xz[];


/* ----------------------------------------------------- */
/* 表單中的功能   */
/* ----------------------------------------------------- */

static void
blist_item(int num , char *brd)
{
     prints("%6d %-13s\n", num, brd);
}


static void
blist_query(char *brd)
{
  char fpath[256];
  XO *xt;
    strcpy(currboard , brd);
    brd_fpath(fpath,brd,FN_RSSCONF);

    xz[XZ_RSS - XO_ZONE].xo = xt = xo_new(fpath);
    strcpy(xt->dir, fpath);
    xover(XZ_RSS);
    free(xt);
}



static int
blist_search(char *brd, char *key)
{
    return (int) str_str(brd,key);
}


static void
blist_add(char *brd)
{
 char *fpath = "etc/rss/brdlist.list";
 int (*sync_func)();
 int resiz = BNLEN+1;
 sync_func=str_cmp;


 rec_add(fpath,brd,resiz);
 rec_sync(fpath, resiz, sync_func, NULL);

}


static int
blist_update()/*更新rssadmin列表*/
{
   char *fpath = FN_RSS_LIST;
   char *brdlist_fpath = "etc/rss/brdlist.list";
   char rssbrd_fpath[512];

   rssfeedlist_t feedlist;
   rssbrdlist_t brdlist;
   char brd_t[BNLEN+1];

   int i,j,k=0;
   int size_rf,size_rb,size_bl;
   int add;
   int resiz = BNLEN+1;


   size_rf = rec_num(fpath, sizeof(rssfeedlist_t));

    for (i = 0; i < size_rf; i++)
        if (rec_get(fpath, &feedlist, sizeof(feedlist), i) >= 0)
        {
            sprintf(rssbrd_fpath, FN_RSS_FOLDER "%s", escape_slash(feedlist.feedurl));

            size_rb = rec_num(rssbrd_fpath, sizeof(rssbrdlist_t));

            for(j=1; j<size_rb ;j++)
                if(rec_get(rssbrd_fpath, &brdlist ,sizeof(brdlist),j) >=0)
                {
                    size_bl=rec_num(brdlist_fpath,resiz);
                    add=1;

                    for(k=0;k<size_bl ;k++)
                        if(rec_get(brdlist_fpath,&brd_t ,sizeof(brd_t),k)>=0 && !strcmp(brdlist.brdname,brd_t))
                            add=0;

                    if(add)
                        blist_add(brdlist.brdname);


                }

        }
    size_bl=rec_num(brdlist_fpath,resiz);
    if(size_rb>0)
        return 1;
    else
        return 0;

}






/* ----------------------------------------------------- */
/* 全站訂閱主函式					 */
/* ----------------------------------------------------- */


int
a_rssadmin()
{
  int num, pageno, pagemax, redraw, reload;
  int ch, cur, i, dirty;
  struct stat st;
  char *data;
  int recsiz;
  char *fpath="etc/rss/brdlist.list";
  char buf[40];

  dirty = 0;	/* 1:有新增/刪除資料 */
  reload = 1;
  pageno = 0;
  cur = 0;
  data = NULL;

  recsiz=BNLEN+1;
  do
  {
    if (reload)
    {
      if (stat(fpath, &st) == -1)
      {
    vmsg("目前站上沒有任何看板訂閱\ＲＳＳ");
    return 0;

      }

      i = st.st_size;
      num = (i / recsiz) - 1;
      if (num < 0)
      {
        if(blist_update())
            continue;

   vmsg("目前站上沒有任何看板訂閱\ＲＳＳ");
    return 0;
      }

      if ((ch = open(fpath, O_RDONLY)) >= 0)
      {
	data = data ? (char *) realloc(data, i) : (char *) malloc(i);
	read(ch, data, i);
	close(ch);
      }

      pagemax = num / XO_TALL;
      reload = 0;
      redraw = 1;
    }

    if (redraw)
    {
      vs_head("ＲＳＳ訂閱\看板", str_site);
      prints(NECKER_RSSADMIN, d_cols, "");  //NECKER

      i = pageno * XO_TALL;
      ch = BMIN(num, i + XO_TALL - 1);
      move(3, 0);
      do
      {
	blist_item(i + 1, data + i * recsiz);  //item_func
	i++;
      } while (i <= ch);

      outf(FEETER_RSSADMIN);  //  feeter
      move(3 + cur, 0);
      outc('>');
      redraw = 0;
    }

    ch = vkey();
    switch (ch)
    {
    case KEY_RIGHT:
    case '\n':
    case ' ':
    case 'r':
      i = cur + pageno * XO_TALL;
      blist_query(data + i * recsiz);  //query_func
      redraw = 1;
      break;
    case 'U':
       if(vans("要更新列表嗎 [y/N]？")== 'y')
       {   blist_update();
           reload = 1;
       }
       break;
    case '/':
      if (vget(b_lines, 0, "關鍵字：", buf, sizeof(buf), DOECHO))
      {
	str_lower(buf, buf);
	for (i = pageno * XO_TALL + cur + 1; i <= num; i++)
	{
	  if (blist_search(data + i * recsiz, buf))  // serch_func
	  {
	    pageno = i / XO_TALL;
	    cur = i % XO_TALL;
	    break;
	  }
	}
      }
      redraw = 1;
      break;

    default:
      ch = xo_cursor(ch, pagemax, num, &pageno, &cur, &redraw);
      break;
    }
  } while (ch != 'q');

  free(data);


  return 0;
}
