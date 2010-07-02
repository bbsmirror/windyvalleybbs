/*-------------------------------------------------------*/
/* battr.h	( NTHU CS MapleBBS Ver 2.36 )		 */
/*-------------------------------------------------------*/
/* target : Board Attribution				 */
/* create : 95/03/29				 	 */
/* update : 95/12/15				 	 */
/*-------------------------------------------------------*/


#ifndef	_BATTR_H_
#define	_BATTR_H_


/* ----------------------------------------------------- */
/* Board Attribution : flags in BRD.battr		 */
/* ----------------------------------------------------- */


#define BRD_NOZAP	    0x0001	/* 不可 zap */
#define BRD_NOTRAN	    0x0002	/* 不轉信 */
#define BRD_NOCOUNT	    0x0004	/* 不計文章發表篇數 */
#define BRD_NOSTAT	    0x0008	/* 不納入熱門話題統計 */
#define BRD_NOVOTE	    0x0010	/* 不公佈投票結果於 [record] 板 */
#define BRD_ANONYMOUS	0x0020	/* 匿名看板 */
#define BRD_SCORE	    0x0040	/* 推文看板 */
#define BRD_PUSH        0x0080  /* 連推看板 */
#define BRD_PREFIX      0x0100  /* 使用文章類別 */
#define BRD_USIES       0x0200  /* 觀看閱\讀名單 */
#define BRD_EVALUATE    0x0400  /* 優劣文*/

/* ----------------------------------------------------- */
/* 各種旗標的中文意義					 */
/* ----------------------------------------------------- */


#define NUMBATTRS	11

#define STR_BATTR	"zTcsvA%fpuE"			/* itoc: 新增旗標的時候別忘了改這裡啊 */


#ifdef _ADMIN_C_
static char *battr_tbl[NUMBATTRS] =
{
  "不可 Zap",			/* BRD_NOZAP */
  "不轉信出去",			/* BRD_NOTRAN */
  "不記錄篇數",			/* BRD_NOCOUNT */
  "不做熱門話題統計",	/* BRD_NOSTAT */
  "不公開投票結果",		/* BRD_NOVOTE */
  "匿名看板",			/* BRD_ANONYMOUS */
  "推文看板",			/* BRD_SCORE */
  "連推看板",			/* BRD_PUSH */
  "使用文章類別",       /* BRD_PREFIX */
  "觀看進板記錄",       /* BRD_USIES*/
  "優劣文看板"          /* BRD_EVALUATE*/

};

#endif

#endif				/* _BATTR_H_ */
