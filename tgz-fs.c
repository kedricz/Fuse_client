#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdlib.h>


static char* root_d;
static char* file;
static char*** file_path;
static char** file_name;
static int* file_size;
static int file_num;
static int max_path;
static char* buff;



void list(char* f);
void num(char* f);
int path_index(char *path);




static int sludge_getattr(const char* path, struct stat* stbuf)
{
  int sz;

  //printf("*getattr*  path = %s\n", path);
  memset(stbuf, 0, sizeof(struct stat));
  
  if(strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0766;
    stbuf->st_nlink = 2;
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
    return 0;
  }
     
  
  sz = path_index(path);
  size_t size = 0;

  if(sz != -1)
    size = file_size[sz];

  //printf("size = %d\n", size);
  stbuf->st_mode = S_IFREG | 0644;
  stbuf->st_nlink = 1;
  stbuf->st_size = size;
  stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
  
  return 0;
}



typedef struct {
  char name[255];
  int size;
  char path[255];
}header;




static int sludge_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) 
{
  //printf("*readdir*  path = %s\n", path);
  off_t nextoff = 0;

 
  while(1) {
    header h;
    FILE* fp = fopen(file, "r");
    int sz;

    if(fp==NULL)
      return -1;

    fseek(fp, nextoff, SEEK_SET);
    fread(&h, sizeof(header), 1, fp);
       

    sz = h.size;
    if(h.size==0)
      break;
 
    /******************************************************************/
    if(strcmp(path, "/") == 0) {

      filler(buf, ".", NULL, 0);
      filler(buf, "..", NULL, 0);
      filler(buf, h.name, NULL, 0);	

    } // end if

    nextoff += (sizeof(header) + sz);
    memset(&h, 0, sizeof(h));
    fclose(fp);

  } // end while
    

  return 0;  
}





static int sludge_unlink(const char* path)
{
  //printf("*unlink*   path = %s\n", path);
  header h;
  char* buffer;
  off_t offset_r, offset_w;
  int cp;
  size_t tsize;   // final file size
  size_t fsize;   // size of file to be deleted

  struct stat st;
  stat(file, &st);
  
  
  FILE* fp = fopen(file, "r");

  /* find the file to be deleted */
  while(!feof(fp)) {
    fread(&h, sizeof(header), 1, fp);

    /* if the file does not existed */
    if(h.size==0) 
      return -ENOENT;
    
    /* if the file is found, set offset to the beginning of the file */
    if(strcmp(h.name, path+1)==0) {
      cp = ftell(fp);
      offset_w = cp - sizeof(header);
      offset_r = cp + h.size;
      fsize = h.size;
      break;
    }
    
    fseek(fp, h.size, SEEK_CUR);
    memset(&h, 0, sizeof(header));
  }
  fclose(fp);
  

  /* overwrite the contents after the file beginning on offset */
  FILE* r = fopen(file, "r");
  FILE* w = fopen(file, "r+");
  header hr;

  fseek(r, offset_r, SEEK_SET);
  fseek(w, offset_w, SEEK_SET);

  while(!feof(r)) {
    memset(&hr, 0, sizeof(header));
 
    fread(&hr, sizeof(header), 1, r);
//printf("file name = %s\n", hr.name);
//   printf("file size = %d\n", hr.size);

    if(hr.size==0)
      break;

    fwrite(&hr, sizeof(header), 1, w);
    
    buffer = malloc(hr.size+1);
    fgets(buffer, hr.size+1, r);
    fputs(buffer, w);
    
    free(buffer);
  }
    
  fclose(r);
  fclose(w);

  
  /* truncate to the total size - file */
  tsize = (st.st_size - (sizeof(header)+fsize));
  FILE* fp2 = fopen(file, "r+");
  if(ftruncate(fileno(fp2), tsize) != 0)
    return -errno;

  fclose(fp2);

  num(file);
  list(file);

  return 0;
}



static int sludge_open(const char* path, struct fuse_file_info* fi)
{
  //printf("*open*  path = %s\n", path);

  int idx = path_index(path);
  
  
  int res = open(file, O_RDONLY);
  if(res == -1) {
    return -errno;
  } 

  fi->fh = idx;
  fi->direct_io = 1;
  fi->nonseekable = 1;

//printf("@@@ end @@@ *open*\n");
  return 0;
}



static int sludge_find(char* sludge, char* file)
{
  header h;
  FILE* fp;
  
  fp = fopen(sludge, "r");
  if(fp==NULL)
    return -1;


  while(!feof(fp)) {
    fread(&h, sizeof(header), 1, fp);
    
    if(h.size==0)
      break;

    if(strcmp(h.name, file)==0) {
      buff = malloc(h.size+1);
      fgets(buff, h.size+1, fp);
      return 0;
    }
    fseek(fp, h.size, SEEK_CUR);
  }
  
  return -1;
}



static int sludge_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{

  //printf("*read*      path = %s\n", path);
  
  size_t len;
  int idx = fi->fh;
  
  //printf("idx = %d\n", idx);

  if(sludge_find(file, path+1) != 0)                     
    return -ENOENT;

  len = file_size[idx];


  if(offset > len)
    size = 0;

  else {
    if(offset+size > len)
      size = len-offset;


    FILE* fp = fopen(file, "r");
    fseek(fp, offset, SEEK_SET);
    int f = ftell(fp);

    memcpy(buf, buff, size);
    memset(buff, 0, sizeof(buff));
    fclose(fp);
  }
  //printf("@@@ end @@@ *read*\n");
  
  return size;
}



static int sludge_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
  //printf("*write*  path = %s\n",path);
  //printf("size = %d      offset = %d\n", size, offset);

  header h;
  strcpy(h.name, path+1);
  strcpy(h.path, path+1);
  h.size = size;

  FILE* fp = fopen(file, "a");

  fwrite(&h, sizeof(header), 1, fp);

  char* buffer = malloc(sizeof(buf));
  memcpy(buffer, buf, size+1);
 
  fputs(buffer, fp);
  fclose(fp);

  num(file);
  list(file);

  //printf("@@@ end @@@ *write*\n");

  return size;
}








static struct fuse_operations sludge_oper = {
  .getattr  = sludge_getattr,
  .readdir  = sludge_readdir,
  //.mknod    = sludge_mknod,
  .unlink   = sludge_unlink,
  .open     = sludge_open,
  .read     = sludge_read,
  .write    = sludge_write,
};




int main(int argc, char *argv[])
{
  int argc2;
  char *argv2[argc-1];

  /* parse the arguments */
  if(argc < 3)
    fprintf(stderr, "No sludge file or mount directory");

  else {
    argc2 = argc-1;
    argv2[0] = (char*)malloc(sizeof(argv[0]));
    argv2[0] = argv[0];

    int i=1;
    while(i < argc-2) {
      argv2[i] = (char*)malloc(sizeof(argv[i]));
      argv2[i] = argv[i];
      i++;
    }
    argv2[i] = (char*)malloc(sizeof(argv[argc-1]));
    argv2[i] = argv[argc-1];
  }


  file = argv[argc-2];

  num(file);
  list(file);

  umask(0);
  return fuse_main(argc2, argv2, &sludge_oper, NULL);
}



void list(char* f) {
  FILE *fp = fopen(f, "r");
  header h;
  int i=0;

  /* if the file does not exist */
  if(fp==NULL) {
    printf("Error opening file!\n");
    exit(1);
  }

  /* initialize file_path array */
  file_path = (char***)malloc(sizeof(char**)*file_num);
  file_name = (char**)malloc(sizeof(char*)*file_num);


  path_num(f);
  //printf("max_path = %d\n", max_path);


  int j;
  for(j=0; j<file_num; j++) {
    file_name[j] = (char*)malloc(255*sizeof(char));
    file_path[j] = (char**)malloc(max_path*sizeof(char*));
  }


  int a,b;
  for(a=0; a<file_num; a++) {
    for(b=0; b<max_path; b++) {
      file_path[a][b] = '\0';
    }
  }



  /* initialize file_size array */
  if(sizeof(file_size) != 0)
    free(file_size);
  file_size = (int*)malloc(sizeof(int)*file_num);


  /* read the headers of the files in the archive */
  while(!feof(fp)) {
    fread(&h, sizeof(header), 1, fp);

    if(h.size == 0)
      break;
    
    strcpy(file_name[i], h.name);
    //printf("file size = %d\n", h.size);
    file_size[i++] = h.size;
    fseek(fp, h.size, SEEK_CUR);
    memset(&h, 0, sizeof(h));
  }

  fclose(fp);
} // end list;



void path_num(char* f) {
  int num=0;
  FILE *fp = fopen(f, "r");
  header h;
  const char s[2] = "/";

  while(!feof(fp)) {
    char* token;
    char* fpath = malloc(sizeof(char)*255);
    int i=0;
    fread(&h, sizeof(header), 1, fp);

    if(h.size == 0)
      break;

    //strcpy(tmp_file, h.name);
    strcpy(fpath, h.path);


    token = strtok(fpath, s);
    while(token != NULL) {
      i++;
      //printf("token = %s\n", token);
      token = strtok(NULL, s);

      if(token == NULL) {
	//printf("break\n");
        break;
      }
    } // end while(token)
    
    
    if(i > num)
      num = i;
 

    fseek(fp, h.size, SEEK_CUR);
    memset(&h, 0, sizeof(h));
  } // end while(feof)
  
  fclose(fp);
  max_path = num;

} // end path_num




void num(char* f) {
  FILE *fp = fopen(f, "r");
  header h;
  int count=0;

  /* if the file does not exist */
  if(fp==NULL) {
    printf("Error opening file!\n");
    exit(1);
  }

  /* read the headers of the files in the archive */
  while(!feof(fp)) {
    fread(&h, sizeof(header), 1, fp);

    if(h.size == 0)
      break;

    count++;
    fseek(fp, h.size, SEEK_CUR);
    memset(&h, 0, sizeof(h));
  }

  fclose(fp);
  file_num = count;
}




int path_index(char *path) {
  int i;
  char *temp = path+1;

  for(i=0; i<file_num; i++) {
    if(strcmp(temp, file_name[i]) == 0)
      return i;
  }
 
  return -1;
}
