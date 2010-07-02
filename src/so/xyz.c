/*-------------------------------------------------------*/
/* xyz.c	( NTHU CS MapleBBS Ver 3.10 )		 */
/*-------------------------------------------------------*/
/* target : 雜七雜八的外掛				 */
/* create : 01/03/01					 */
/* update :   /  /  					 */
/*-------------------------------------------------------*/


#include "bbs.h"

#include <netinet/tcp.h>
#include <sys/wait.h>
#include <sys/resource.h>

extern UCACHE *ushm;
time_t diff,diff2;

/* cache.081017:系統資訊 */

int
x_siteinfo()
{
    double load[3];
    getloadavg(load, 3);
    diff = (time(0) - ushm->bbs_start) / 3600;
    diff2 = (time(0) - ushm->bbs_start) % 3600;
    clear();

    move(1, 0);
    prints("站    名： %s - %s (%s)\n\n", BBSNAME, MYHOSTNAME, MYIPADDR);
    prints("程式版本：%s\033[m\n", CURRENTVER);
    prints("開機時間： %d Day, %d Hour, %d Min, %d Sec\n", diff / 24, diff % 24, diff2 / 60, diff2 % 60);    
    prints("站上人數： %d/%d\n", ushm->count, MAXACTIVE);
    prints("系統負載： %.2f %.2f %.2f [%s]\n"
	    , load[0], load[1], load[2], load[0] > 3 ? "\033[1;41;37m過高\033[m" : load[0] > 1 ? 
        "\033[1;42;37m稍高\033[m" : "\033[1;44;37m正常\033[m");
    prints("索引資料： BRD %d KB, ACCT %d KB, HDR %d KB\n", sizeof(BRD), sizeof(ACCT), sizeof(HDR));
         struct rusage ru; 
         getrusage(RUSAGE_SELF, &ru); 
         prints("Mem 用量： sbrk %d KB, idrss %d KB, isrss %d KB\n", 
                ((int)sbrk(0) - 0x8048000) / 1024, 
                (int)ru.ru_idrss, (int)ru.ru_isrss); 
         prints("CPU 用量： %ld.%06ldu %ld.%06lds\n", 
                ru.ru_utime.tv_sec, ru.ru_utime.tv_usec, 
                ru.ru_stime.tv_sec, ru.ru_stime.tv_usec); 
    prints("\n");
    prints("\033[1mNCKU.EEBBS refresh Project: Maple-itoc cache-modified.\033[m\n");
    prints("\n");
#ifdef Modules    
    prints("\033[1;30mModules & Plug-in: \033[m\n\n");

//模組化的放在這邊    
#ifdef M3_USE_PMORE
    prints("\033[1;32m  online \033[1;30m  pmore (piaip's more) 2007 w/Movie\033[m\n");
#else
    prints("\033[1;31m  offline\033[1;30m  pmore (piaip's more) 2007 w/Movie\033[m\n");    
#endif
#ifdef GRAYOUT
    prints("\033[1;32m  online \033[1;30m  Grayout Advanced Control 淡入淡出特效系統\033[m\n");
#else
    prints("\033[1;31m  offline\033[1;30m  Grayout Advanced Control 淡入淡出特效系統\033[m\n");
#endif
#ifdef SMerge
    prints("\033[1;32m  online \033[1;30m  Smart Merge 修文自動合併\033[m\n");
#else
    prints("\033[1;31m  offline\033[1;30m  Smart Merge 修文自動合併\033[m\n");
#endif
#ifdef BBSRuby
    prints("\033[1;32m  online \033[1;30m  (EXP) BBSRuby v0.3\033[m\n");
#else
    prints("\033[1;31m  offline\033[1;30m  (EXP) BBSRuby v0.3\033[m\n");
#endif
#ifdef RSSreader
    prints("\033[1;32m  online \033[1;30m  (EXP) RSS 訂閱\器 v0.1\033[m\n");
#else
    prints("\033[1;31m  offline\033[1;30m  (EXP) RSS 訂閱\器 v0.1\033[m\n");
#endif
#ifdef M3_USE_PFTERM
    prints("\033[1;32m  online \033[1;30m  (EXP) pfterm (piaip's flat terminal, Perfect Term)\033[m\n");
#else
    prints("\033[1;31m  offline\033[1;30m  (EXP) pfterm (piaip's flat terminal, Perfect Term)\033[m\n");
#endif

#else
    prints("\033[1;30mModules & Plug-in: None\033[m\n");
#endif
    vmsg(NULL);
    return 0;
}


#ifdef HAVE_TIP

/* ----------------------------------------------------- */
/* 每日小秘訣						 */
/* ----------------------------------------------------- */

int
x_tip()
{
  int i, j;
  char msg[128];
  FILE *fp;

  if (!(fp = fopen(FN_ETC_TIP, "r")))
    return XEASY;

  fgets(msg, 128, fp);
  j = atoi(msg);		/* 第一行記錄總篇數 */
  i = time(0) % j + 1;
  j = 0;

  while (j < i)			/* 取第 i 個 tip */
  {
    fgets(msg, 128, fp);
    if (msg[0] == '#')
      j++;
  }

  move(12, 0);
  clrtobot();
  fgets(msg, 128, fp);
  prints("\033[1;36m每日小祕訣：\033[m\n");
  prints("            %s", msg);
  fgets(msg, 128, fp);
  prints("            %s", msg);
  vmsg(NULL);
  fclose(fp);
  return 0;
}
#endif	/* HAVE_TIP */


#ifdef HAVE_LOVELETTER 

/* ----------------------------------------------------- */
/* 情書產生器						 */
/* ----------------------------------------------------- */

int
x_loveletter()
{
  FILE *fp;
  int start_show;	/* 1:開始秀 */
  int style;		/* 0:開頭 1:正文 2:結尾 */
  int line;
  char buf[128];
  char header[3][5] = {"head", "body", "foot"};	/* 開頭、正文、結尾 */
  int num[3];

  /* etc/loveletter 前段是#head 中段是#body 後段是#foot */
  /* 行數上限：#head五行  #body八行  #foot五行 */

  if (!(fp = fopen(FN_ETC_LOVELETTER, "r")))
    return XEASY;

  /* 前三行記錄篇數 */
  fgets(buf, 128, fp);
  num[0] = atoi(buf + 5);
  num[1] = atoi(buf + 5);
  num[2] = atoi(buf + 5);

  /* 決定要選第幾篇 */
  line = time(0);
  num[0] = line % num[0];
  num[1] = (line >> 1) % num[1];
  num[2] = (line >> 2) % num[2];

  vs_bar("情書產生器");

  start_show = style = line = 0;

  while (fgets(buf, 128, fp))
  {
    if (*buf == '#')
    {
      if (!strncmp(buf + 1, header[style], 4))  /* header[] 長度都是 5 bytes */
	num[style]--;

      if (num[style] < 0)	/* 已經 fget 到要選的這篇了 */
      {
	outc('\n');
	start_show = 1;
	style++;
      }
      else
      {
	start_show = 0;
      }
      continue;
    }

    if (start_show)
    {
      if (line >= (b_lines - 5))	/* 超過螢幕大小了 */
	break;

      outs(buf);
      line++;
    }
  }

  fclose(fp);
  vmsg(NULL);

  return 0;
}
#endif	/* HAVE_LOVELETTER */


/* ----------------------------------------------------- */
/* 密碼忘記，重設密碼					 */
/* ----------------------------------------------------- */


int
x_password()
{
  int i;
  ACCT acct;
  FILE *fp;
  char fpath[80], email[60], passwd[PSWDLEN + 1];
  time_t now;

  vmsg("當其他使用者忘記密碼時，重送新密碼至該使用者的信箱");

  if (acct_get(msg_uid, &acct) > 0)
  {
    time(&now);

    if (acct.lastlogin > now - 86400 * 10)
    {
      vmsg("該使用者必須十天以上未上站方可重送密碼");
      return 0;
    }

    vget(b_lines - 2, 0, "請輸入認證時的 Email：", email, 40, DOECHO);

    if (str_cmp(acct.email, email))
    {
      vmsg("這不是該使用者認證時用的 Email");
      return 0;
    }

    if (not_addr(email) || !mail_external(email))
    {
      vmsg(err_email);
      return 0;
    }

    vget(b_lines - 1, 0, "請輸入真實姓名：", fpath, RNLEN + 1, DOECHO);
    if (strcmp(acct.realname, fpath))
    {
      vmsg("這不是該使用者的真實姓名");
      return 0;
    }

    if (vans("資料正確，請確認是否產生新密碼(Y/N)？[N] ") != 'y')
      return 0;

    sprintf(fpath, "%s 改了 %s 的密碼", cuser.userid, acct.userid);
    blog("PASSWD", fpath);

    /* 亂數產生 A~Z 組合的密碼八碼 */
    for (i = 0; i < PSWDLEN; i++)
      passwd[i] = rnd(26) + 'A';
    passwd[PSWDLEN] = '\0';

    /* 重新 acct_load 載入一次，避免對方在 vans() 時登入會有洗錢的效果 */
    if (acct_load(&acct, acct.userid) >= 0)
    {
      str_ncpy(acct.passwd, genpasswd(passwd), PASSLEN + 1);
      acct_save(&acct);
    }

    sprintf(fpath, "tmp/sendpass.%s", cuser.userid);
    if (fp = fopen(fpath, "w"))
    {
      fprintf(fp, "%s 為您申請了新密碼\n\n", cuser.userid);
      fprintf(fp, BBSNAME "ID : %s\n\n", acct.userid);
      fprintf(fp, BBSNAME "新密碼 : %s\n", passwd);
      fclose(fp);

      bsmtp(fpath, BBSNAME "新密碼", email, 0);
      unlink(fpath);

      vmsg("新密碼已寄到該認證信箱");
    }
  }

  return 0;
}
