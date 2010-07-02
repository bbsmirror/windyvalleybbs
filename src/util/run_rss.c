/*-------------------------------------------------------*/
/* run_rss.c                                             */
/*-------------------------------------------------------*/
/* target: to call rss.php                               */
/* create: 2009/04/08                                    */
/* update: 2009/6/24                                     */
/* author: hrs113355.bbs@xdbbs.twbbs.org                 */
/* modify: cache.bbs@bbs.ee.ncku.edu.tw                  */
/*-------------------------------------------------------*/


#include "bbs.h"
#define ESCAPE_SLASH "#%slash%#"

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

int main()
{
    chdir(BBSHOME);

    FILE *fp = fopen(FN_RSS_LIST, "rb");
    rssfeedlist_t feedlist;
    char cmd[768]; 
    int i=0;

    while (fread(&feedlist, sizeof(feedlist), 1, fp))
	if (*(feedlist.feedurl) && !strchr(feedlist.feedurl, '\"'))
	{
	    sprintf(cmd, "src/util/rss.php \"%s\" > /dev/null"
		, escape_slash(feedlist.feedurl));
	    system(cmd);
        i++;
	}
    fclose(fp);
    if(i==1)
      printf("There is 1 RSS feed has been updated.\n");    
    else
      printf("There are %d RSS feeds have been updated.\n", i);
    return 0;
}
