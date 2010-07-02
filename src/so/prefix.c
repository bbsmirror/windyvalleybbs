/*-------------------------------------------------------*/
/* prefix.c         ( NTHU CS MapleBBS Ver 3.10 )        */
/*-------------------------------------------------------*/
/* target : Post Refix �޲z�����{��                      */
/* author : kyo.bbs@cszone.org                           */
/* create : 05/01/17                                     */
/* update :   /  /                                       */
/*-------------------------------------------------------*/
                                                                                
                                                                                
#include "bbs.h"
                                                                                
                                                                                
extern XZ xz[];
extern char xo_pool[];
                                                                                
#define PREFIX_MANAGER  0x01
                                                                                
#define C_PREFIX_MANAGER        "\033[1;31m"
#define C_PREFIX_USER           "\033[1;37m"
                                                                              
static int
prefix_edit(prefix, echo)
  PREFIX *prefix;
  int echo;
{
  vmsg("�Ъ`�N�A���e�e��n�[�W [ �M ] �~�|���G�Ч@�ΡI");
  char buf_command[3] = {0};
  char *menu[] = {buf_command,
      "1  �@��ϥΪ�",
      "2    �O�D�M��", NULL};
                                                                                
  strcpy(buf_command, "11");
                                                                                
  if (echo == GCARRY)
  {
    if (prefix->xmode & PREFIX_MANAGER)
      strcpy(buf_command, "22");
  }
                                                                           
  switch(pans(b_lines - 15, 0, "���O�ϥ��v��", menu))
  {
    case '1':
      prefix->xmode = 0;
      break;
    case '2':
      prefix->xmode = PREFIX_MANAGER;
      break;
  }
                                                                                
  if (!(vget(b_lines, 0, "�峹���O�G", prefix->title, 11, echo)))
    return 0;
                                                                                
  return 1;
}
                                                                                
static int
prefix_add(xo)
  XO *xo;
{
  PREFIX prefix;
                                                                                
  memset(&prefix, 0, sizeof(PREFIX));
                                                                                
  if (prefix_edit(&prefix, DOECHO))
  {
    char fpath[64];
                                                                                
    time(&prefix.stamp);
    brd_fpath(fpath, currboard, FN_PREFIX);
    rec_add(fpath, &prefix, sizeof(PREFIX));
  }
                                                                                
  return XO_INIT;
}
                                                                                
static int
prefix_change(xo)
  XO *xo;
{
  PREFIX *prefix, old_prefix;
  int pos, cur;
                                                                                
  pos = xo->pos;
  cur = pos - xo->top;
  prefix = (PREFIX *) xo_pool + cur;
                                                                                
  old_prefix = *prefix;

  if (prefix_edit(&old_prefix, GCARRY))
  {
    if (memcmp(prefix, &old_prefix, sizeof(PREFIX)))
    {
      if (vans("�T�w�n�ק�ܡH[N] ") == 'y')
      {
        char fpath[64];
                                                                                
        brd_fpath(fpath, currboard, FN_PREFIX);
        rec_put(fpath, &old_prefix, sizeof(PREFIX), pos, NULL);
        return XO_INIT;
      }
    }
  }
                                                                                
  return XO_HEAD;
}
                                                                                
static int
prefix_title(xo)
  XO *xo;
{
  PREFIX *prefix, old_prefix;
  int pos, cur;
                                                                                
  pos = xo->pos;
  cur = pos - xo->top;
  prefix = (PREFIX *) xo_pool + cur;
                                                                                
  old_prefix = *prefix;
                                                                                
  if (vget(b_lines, 0, "�峹���O�G", prefix->title, 11, GCARRY))
  {
    if (memcmp(prefix, &old_prefix, sizeof(PREFIX)))
    {
      char fpath[64];
                                                                                
      brd_fpath(fpath, currboard, FN_PREFIX);
      rec_put(fpath, prefix, sizeof(PREFIX), pos, NULL);
      return XO_LOAD;
    }
  }
                                                                                
  return XO_FOOT;
}

prefix_delete(xo)
  XO *xo;
{
  if (vans(msg_del_ny) == 'y')
  {
    if (!rec_del(xo->dir, sizeof(PREFIX), xo->pos, NULL))
    {
      PREFIX *prefix;
      prefix = (PREFIX *) xo_pool + (xo->pos - xo->top);
      return XO_LOAD;
    }
  }
  return XO_FOOT;
}
                                                                                
static int
prefix_rangedel(xo)
  XO *xo;
{
  return xo_rangedel(xo, sizeof(PREFIX), NULL, NULL);
}
                                                                                
static int
vfyprefix(prefix, pos)
  PREFIX *prefix;
  int pos;
{
  return Tagger(prefix->stamp, pos, TAG_NIN);
}
                                                                                
static int
prefix_prune(xo)
  XO *xo;
{
  return xo_prune(xo, sizeof(PREFIX), vfyprefix, NULL);
}
                                                                                
static void
prefix_item(num, prefix)
  int num;
  PREFIX *prefix;
{
  prints("%6d  %c%-10s  %s%s\033[m\n",
    num, tag_char(prefix->stamp),
    prefix->title,
    (prefix->xmode & PREFIX_MANAGER) ? C_PREFIX_MANAGER : C_PREFIX_USER,
    (prefix->xmode & PREFIX_MANAGER) ? "�O�D�M��" : "�@��ϥΪ�");
}
                                                                                
static int
prefix_query(xo)
  XO *xo;
{
  PREFIX *prefix;
  int pos, cur;
  char fpath[64];
                                                                                
  pos = xo->pos;
  cur = pos - xo->top;
  prefix = (PREFIX *) xo_pool + cur;
                                                                                
  prefix->xmode ^= PREFIX_MANAGER;
  brd_fpath(fpath, currboard, FN_PREFIX);
  rec_put(fpath, prefix, sizeof(PREFIX), pos, NULL);
                                                                                
  move(3 + cur, 0);
  prefix_item(++pos, prefix);
                                                                                
  return XO_NONE;
}
                                                                                
static int
prefix_body(xo)
  XO *xo;
{
  PREFIX *prefix;
  int max, num, tail;
                                                                                
  max = xo->max;
  if (max <= 0)
  {
    if (vans("�s�W�峹���O��ơH[N] ") == 'y')
      return prefix_add(xo);
                                                                                
    return XO_QUIT;
  }
                                                                                
  prefix = (PREFIX *) xo_pool;
  num = xo->top;
  tail = num + XO_TALL;
  if (max > tail)
    max = tail;
                                                                               
  move(3, 0);
  do
  {
    prefix_item(++num, prefix++);
  } while (num < max);
  clrtobot();
                                                                                
  return XO_FOOT;       /* itoc.010403: �� b_lines ��W feeter */
}
                                                                                
static int
prefix_head(xo)
  XO *xo;
{
  vs_head("���O�޲z", str_site);
  prints(NECKER_PREFIX, d_cols, "");
                                                                                
  return prefix_body(xo);
}
                                                                                
static int
prefix_load(xo)
  XO *xo;
{
  xo_load(xo, sizeof(PREFIX));
  return prefix_body(xo);
}
                                                                                
static int
prefix_init(xo)
  XO *xo;
{
  xo_load(xo, sizeof(PREFIX));
  return prefix_head(xo);
}
                                                                                
static int
prefix_tag(xo)
  XO *xo;
{
  PREFIX *prefix;
  int tag, pos, cur;
                                                                                
  pos = xo->pos;
  cur = pos - xo->top;
  prefix = (PREFIX *) xo_pool + cur;
                                                                                
  if (tag = Tagger(prefix->stamp, pos, TAG_TOGGLE))
  {
    move(3 + cur, 0);
    prefix_item(++pos, prefix);
  }
                                                                                
  return xo->pos + 1 + XO_MOVE; /* lkchu.981201: ���ܤU�@�� */
}
                                                                                
                                                                                
static int
prefix_move(xo)
  XO *xo;
{
  PREFIX *prefix;
  int pos, cur, i;
  char buf[40], ans[5];
                                                                                
  pos = xo->pos;
  cur = pos - xo->top;
  prefix = (PREFIX *) xo_pool + cur;
                                                                                
  sprintf(buf, "�п�J�� %d �ﶵ���s��m�G", pos + 1);
  if (vget(b_lines, 0, buf, ans, 5, DOECHO))
  {
    i = atoi(ans) - 1;
    if (i < 0)
      i = 0;
    else if (i >= xo->max)
      i = xo->max - 1;
                                                                                
    if (i != pos)
    {
      if (!rec_del(xo->dir, sizeof(PREFIX), pos, NULL))
      {
        rec_ins(xo->dir, prefix, sizeof(PREFIX), i, 1);
        xo->pos = i;
        return prefix_load(xo);
      }
    }
  }
  return XO_FOOT;
}
                                                                                
static int
prefix_help(xo)
  XO *xo;
{
  xo_help("prefix");
  return XO_HEAD;
}
                                                                                
                                                                                
static KeyFunc prefix_cb[] =
{
  XO_INIT, prefix_init,
  XO_LOAD, prefix_load,
  XO_HEAD, prefix_head,
  XO_BODY, prefix_body,
                                                                                
  'r', prefix_query,
  'a', prefix_add,
  'E', prefix_change,
  'T', prefix_title,
  Ctrl('D'), prefix_prune,
  'd', prefix_delete,
  'D', prefix_rangedel,
  't', prefix_tag,
  'm', prefix_move,
  'h', prefix_help
};
                                                                                
                                                                                
int
XoPrefix(xo)
  XO *xo;
{
                                                                                
  if (bbstate & STAT_BM)
  {
    char fpath[64];
                                                                                
    brd_fpath(fpath, currboard, FN_PREFIX);
    xz[XZ_PREFIX - XO_ZONE].xo = xo = xo_new(fpath);
    xz[XZ_PREFIX - XO_ZONE].cb = prefix_cb;
    xover(XZ_PREFIX);
    free(xo);
                                                                                
    return XO_INIT;
  }
                                                                                
  return XO_NONE;
}                                                                              
