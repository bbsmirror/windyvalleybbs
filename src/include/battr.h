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


#define BRD_NOZAP	    0x0001	/* ���i zap */
#define BRD_NOTRAN	    0x0002	/* ����H */
#define BRD_NOCOUNT	    0x0004	/* ���p�峹�o��g�� */
#define BRD_NOSTAT	    0x0008	/* ���ǤJ�������D�έp */
#define BRD_NOVOTE	    0x0010	/* �����G�벼���G�� [record] �O */
#define BRD_ANONYMOUS	0x0020	/* �ΦW�ݪO */
#define BRD_SCORE	    0x0040	/* ����ݪO */
#define BRD_PUSH        0x0080  /* �s���ݪO */
#define BRD_PREFIX      0x0100  /* �ϥΤ峹���O */
#define BRD_USIES       0x0200  /* �[�ݾ\\Ū�W�� */
#define BRD_EVALUATE    0x0400  /* �u�H��*/

/* ----------------------------------------------------- */
/* �U�غX�Ъ�����N�q					 */
/* ----------------------------------------------------- */


#define NUMBATTRS	11

#define STR_BATTR	"zTcsvA%fpuE"			/* itoc: �s�W�X�Ъ��ɭԧO�ѤF��o�̰� */


#ifdef _ADMIN_C_
static char *battr_tbl[NUMBATTRS] =
{
  "���i Zap",			/* BRD_NOZAP */
  "����H�X�h",			/* BRD_NOTRAN */
  "���O���g��",			/* BRD_NOCOUNT */
  "�����������D�έp",	/* BRD_NOSTAT */
  "�����}�벼���G",		/* BRD_NOVOTE */
  "�ΦW�ݪO",			/* BRD_ANONYMOUS */
  "����ݪO",			/* BRD_SCORE */
  "�s���ݪO",			/* BRD_PUSH */
  "�ϥΤ峹���O",       /* BRD_PREFIX */
  "�[�ݶi�O�O��",       /* BRD_USIES*/
  "�u�H��ݪO"          /* BRD_EVALUATE*/

};

#endif

#endif				/* _BATTR_H_ */
