#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main() {

 
 char str[] = "tmp111/tmp222/file1";
 const char s[2] = "/";
 const char ss = '/';
 //char* s = "/";
 //char* token;



 FILE* fp = fopen("tmp/tmp2/file1", "w");
 char* buff = "xxx";
 fputs(buff, fp);
 fclose(fp);



 /*

 printf("str = %s\n", str);

 //token = strtok(str, s);
 
 //printf("*%s\n", token);

 //printf("dir = %s\n", token);

 char *s1;
 char *s2;
 char *str2 = malloc(1+strlen(str));

 strcpy(str2, str);
 //printf("str2 = %s\n", str2);





 s1 = strchr(str2, '/');
 while(s1 != NULL) {
   //printf("s1 = %s\n", s1);
   s2 = strchr(s1+1, '/');
   if(s2==NULL)
     break;
   strcpy(s1, s2+1);
 }


 //printf("s1 = %s\n", s1);

 
 char* path = malloc(strlen(str));


 token = strtok(str, s);
 printf("dir = %s\n", token);
 
 strcat(path, token);
 while(token != NULL) {
 
   strcat(path, "/");
   //printf("dir = %s\n", path);

   token = strtok(NULL, s);
   strcat(path, token);
  

   if(strcmp(token, s1)==0)
     break;

   printf("dir = %s\n", path);
 }
 
 */


 return 0;
}
