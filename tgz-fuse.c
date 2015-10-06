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




struct posix_header
{                       /* byte offset */
  char name[100];       /*   0 */
  char mode[8];         /* 100 */
  char uid[8];          /* 108 */
  char gid[8];          /* 116 */
  char size[12];        /* 124 */
  char mtime[12];       /* 136 */
  char chksum[8];       /* 148 */
  char typeflag;        /* 156 */
  char linkname[100];   /* 157 */
  char magic[6];        /* 257 */
  char version[2];      /* 263 */
  char uname[32];       /* 265 */
  char gname[32];       /* 297 */
  char devmajor[8];     /* 329 */
  char devminor[8];     /* 337 */
  char prefix[155];     /* 345 */
                        /* 500 */
};




typedef struct node {
  char path[100];
  char name[100];
  int size;
  struct node *child;
  struct node *sibling;

} node;




static char* file;
static char** file_path;
static char*** file_path_token;
static char** file_name;
static int* file_size;
static int file_num;
static int max_path;
static char* buff;

static node* tree;


void list(char* f);
void num(char* f);
int path_index(char *path);

/* tree functions */
void insert(node **root, node* child, int idx, int tk_idx);
node* create(char *path, int size);
node* node_find(node* root, char *path);
void print_tree(node* root);
void build_tree(node** tree);




static int sludge_getattr(const char* path, struct stat* stbuf)
{
  int sz;
  int len = strlen(path+1);
  char* tmp_path = malloc(strlen(path));
  strcpy(tmp_path, path+1);

  //printf("*getattr*  path = %s\n", path);
  //printf("path len = %d\n", len);                                                                      
  //printf("last letter of path = %c\n\n", path[len]);


  memset(stbuf, 0, sizeof(struct stat));

  /*
  
  if(strcmp(path, "/") != 0) {
    node* nd = node_find(tree, path+1);
    if(nd == NULL)
      return -ENOENT;
  }
  else {
    node* nd = node_find(tree, "/");
    if(nd == NULL)
      return -ENOENT;
  }
  */  

  //printf("path = %s\n", path);
  //printf("tmp_path = %s\n", tmp_path);
  node* n = (node*)malloc(sizeof(node));
  if(strcmp(path, "/") == 0)
    n = node_find(tree, path);
  else
    n = node_find(tree, tmp_path);
  

  //printf("n->path = %s\n", n->path);
  //printf("path = %s\n", path+1);
  sz = path_index(path+1);
  size_t size = 0;


  if(n == NULL) {
    stbuf->st_mode = S_IFREG | 0644;
    stbuf->st_nlink = 1;
    stbuf->st_size = size;
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
    return 0;
  }


  if(strcmp(path, "/") == 0 || n->child != NULL || path[len]== '/') {
    stbuf->st_mode = S_IFDIR | 0766;
    stbuf->st_nlink = 2;
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
    return 0;
  }


  //printf("sz = %d\n", sz);
  if(sz != -1)
    size = file_size[sz];
  //printf("size = %d\n", size);

  stbuf->st_mode = S_IFREG | 0644;
  stbuf->st_nlink = 1;
  stbuf->st_size = size;
  stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

  return 0;
}







static int sludge_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) 
{

  //printf("*readdir*  path = %s\n", path);
  node* nd = (node*)malloc(sizeof(node));
  if(strcmp(path, "/") == 0)
    nd = node_find(tree, path); 
  else 
    nd = node_find(tree, path+1);

  //printf("nd->path = %s\n", nd->path);

  node* cur = nd->child;

  //printf("cur->name = %s\n", cur->path);

  if(cur != NULL) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, cur->path, NULL, 0);

    while(cur->sibling != NULL) {
      cur = cur->sibling;
      filler(buf, ".", NULL, 0);
      filler(buf, "..", NULL, 0);
      filler(buf, cur->path, NULL, 0);
    }
  }
 
  return 0;
}





static int sludge_unlink(const char* path)
{
  //printf("*unlink*   path = %s\n", path);
  struct posix_header h;
  char* buffer;
  off_t offset_r, offset_w;
  int cp;
  size_t tsize;   // final file size
  size_t fsize;   // size of file to be deleted
  size_t s;

  struct stat st;
  stat(file, &st);
  
  
  FILE* fp = fopen(file, "r");

  /* find the file to be deleted */
  while(!feof(fp)) {
    fread(&h, 512, 1, fp);
    s = atoi(h.size);

    /* if the file does not existed */
    if(s==0) 
      return -ENOENT;
    
    /* if the file is found, set offset to the beginning of the file */
    if(strcmp(h.name, path+1)==0) {
      cp = ftell(fp);
      offset_w = cp - 512;
      offset_r = cp + s;
      fsize = s;
      break;
    }
    
    fseek(fp, s, SEEK_CUR);
    memset(&h, 0, sizeof(h));
  }
  fclose(fp);
  

  /* overwrite the contents after the file beginning on offset */
  FILE* r = fopen(file, "r");
  FILE* w = fopen(file, "r+");
  struct posix_header hr;

  fseek(r, offset_r, SEEK_SET);
  fseek(w, offset_w, SEEK_SET);

  while(!feof(r)) {
    memset(&hr, 0, sizeof(hr));
 
    fread(&hr, 512, 1, r);
//printf("file name = %s\n", hr.name);
//   printf("file size = %d\n", hr.size);

    if(atoi(hr.size)==0)
      break;

    fwrite(&hr, 512, 1, w);
    
    buffer = malloc(512);
    fgets(buffer, 512, r);
    fputs(buffer, w);
    
    free(buffer);
  }
    
  fclose(r);
  fclose(w);

  
  /* truncate to the total size - file */
  tsize = (st.st_size - (512+512));
  FILE* fp2 = fopen(file, "r+");
  if(ftruncate(fileno(fp2), tsize) != 0)
    return -errno;

  fclose(fp2);

  num(file);
  list(file);
  free(tree);
  tree = NULL;
  build_tree(&tree);

//printf("@@@ end @@@ *unlink*\n");
  return 0;
}



static int sludge_open(const char* path, struct fuse_file_info* fi)
{
  //printf("*open*  path = %s\n", path);

  int idx = path_index(path+1);
    
  int res = open(file, O_RDONLY);
  if(res == -1) {
    return -errno;
  } 

  fi->fh = idx;
  fi->direct_io = 1;
  fi->nonseekable = 1;

  return 0;
}




static int sludge_find(char* sludge, char* file)
{
  struct posix_header h;
  FILE* fp;
  char bf[512];

  fp = fopen(sludge, "r");
  if(fp==NULL)
    return -1;

  //printf("sludge = %s       file = %s\n", sludge, file);

  buff = malloc(512);

  int i=1;
  int size;
  while(1) {
    fread(&h, 512, 1, fp);
   
    if(atoi(h.size)==0) {
      fclose(fp);
      return -1;
    }

    printf("h.path = %s\n", h.name);
    size = atoi(h.size);

    //int size = atoi(h.size);
    if(strcmp(h.name, file)==0) {
      fclose(fp);
      break;
    }
 
  fseek(fp, 512, SEEK_CUR);
  memset(&h, 0, sizeof(h));
  i = i+2;
  } // end while loop


  FILE* fp2 = fopen(sludge, "r");
  fseek(fp2, 512*i, SEEK_SET);
  fgets(bf, 512, fp);                                                                                   
  printf("contents = %s\n", bf); 
  memcpy(buff, bf, size+1);

  return 0;
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

  struct posix_header h;
  strcpy(h.name, path+1);
  //strcpy(h.path, path+1);
  snprintf(h.size, sizeof(h.size), "%d", size);
  //strcpy(h.size, itoa(size));

  FILE* fp = fopen(file, "a");

  fwrite(&h, 512, 1, fp);

  char* buffer = malloc(512);
  memcpy(buffer, buf, 512);
 
  fputs(buffer, fp);
  fclose(fp);

  num(file);
  list(file);
  free(tree);
  tree = NULL;
  build_tree(&tree);

  //printf("@@@ end @@@ *write*\n");

  return size;
}






static struct fuse_operations sludge_oper = {
  .getattr  = sludge_getattr,
  .readdir  = sludge_readdir,
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



  /**********************************************/
  // build a file directory tree

  tree = NULL;
  build_tree(&tree);
  //print_tree(tree);

  //printf("\n------------------------\n");

  umask(0);
  return fuse_main(argc2, argv2, &sludge_oper, NULL);
}







void list(char* f) {
  FILE *fp = fopen(f, "r");
  struct posix_header h;
  int i=0;

  /* if the file does not exist */
  if(fp==NULL) {
    printf("Error opening file!\n");
    exit(1);
  }

  /* initialize file_path array */
  file_path_token = (char***)malloc(sizeof(char**)*file_num);
  file_path = (char**)malloc(sizeof(char*)*file_num);
  //file_name = (char**)malloc(sizeof(char*)*file_num);

  //printf("file_num = %d\n", file_num);

  path_num(f);
  //printf("max_path = %d\n", max_path);
  

  int j;
  for(j=0; j<file_num; j++) {
    //file_name[j] = (char*)malloc(255*sizeof(char));
    file_path_token[j] = (char**)malloc(max_path*sizeof(char*));
    file_path[j] = (char*)malloc(512*sizeof(char));
  }


  int a,b;
  for(a=0; a<file_num; a++) {
    for(b=0; b<max_path; b++) {
      file_path_token[a][b] = '\0';
    }
  }


  /* parse the file path & build path array */
  const char s[2] = "/";
 
  int c=0;
  while(!feof(fp)) {
    char* tmp_path_token = malloc(sizeof(char)*255);
    char* token;
    fread(&h, 512, 1, fp);

    if(atoi(h.size) == 0)
      break;

    
    strcpy(tmp_path_token, h.name);

    int r=0;
    token = strtok(tmp_path_token, s);
    while(token != NULL) {
      file_path_token[c][r] = malloc(1+strlen(token));
      strcpy(file_path_token[c][r++], token); 
      token = strtok(NULL, s);
      if(token == NULL) {
        break;
      }
    } // end while



    fseek(fp, 512, SEEK_CUR);
    memset(&h, 0, sizeof(h));
    //free(tmp_file);
    //free(tmp_path);
    free(tmp_path_token);
    c++;
  }

  fclose(fp);


  FILE *fp2 = fopen(f, "r");
  struct posix_header h2;

  /* initialize file_size array */
  if(sizeof(file_size) != 0)
    free(file_size);
  file_size = (int*)malloc(sizeof(int)*file_num);

  int k=0;
  /* read the headers of the files in the archive */
  while(!feof(fp)) {
    fread(&h2, 512, 1, fp2);

    if(atoi(h2.size) == 0)
      break;
    
    strcpy(file_path[k], h2.name);
    size_t sz = atoi(h2.size);
    //printf("sz = %d\n", sz);
    file_size[k++] = sz;
    fseek(fp2, 512, SEEK_CUR);
    memset(&h2, 0, sizeof(h2));
  }

  fclose(fp2);

} // end list;





void path_num(char* f) {
  int num=0;
  FILE *fp = fopen(f, "r");
  struct posix_header h;
  const char s[2] = "/";
  while(!feof(fp)) {
    char* token;
    //char* fpath = (char*)malloc(sizeof(char)*100);
    char fpath[100];
    int i=0;
    fread(&h, 512, 1, fp);

    if(atoi(h.size) == 0)
      break;
  
    //printf("h.name = %s\n", h.name);
    strcpy(fpath, h.name);
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
 
    fseek(fp, 512, SEEK_CUR);
    memset(&h, 0, sizeof(h));
  } // end while(feof)
  
  fclose(fp);
  max_path = num;

} // end path_num




void num(char* f) {
  FILE *fp = fopen(f, "r");
  struct posix_header h;
  int count=0;

  /* if the file does not exist */
  if(fp==NULL) {
    printf("Error opening file!\n");
    exit(1);
  }

  /* read the headers of the files in the archive */
  while(!feof(fp)) {
    fread(&h, 512, 1, fp);

    if(atoi(h.size) == 0)
      break;

    count++;
    fseek(fp, 512, SEEK_CUR);
    memset(&h, 0, sizeof(h));
  }

  fclose(fp);
  file_num = count;
}




int path_index(char *path) {
  int i;
  //char* fpath = malloc(sizeof(char)*100);
  //char* temp;

  /*
  if((temp = strchr(path, '/')) != NULL) {
    char* token;
    while((token = strchr(temp, '/')) != NULL) {
      strcpy(temp, token+1);
    }
    strcpy(fpath, temp);
  }

  else {
    strcpy(fpath, path);
  }
  */

  //printf("**path index \npath = %s\n", path);

  if(strcmp(path, "/") == 0)
    return -1;
  
  else {
    for(i=0; i<file_num; i++) {
      if(strcmp(path, file_path[i]) == 0)
	return i;
    }
  }
 
  return -1;
}




void build_tree(node** tree) 
{ 
  node *root = create("/", 0);
  insert(tree, root, 0, 0);


  const char s[2] = "/";
  int k;
  for(k=0; k<file_num; k++) {
    //printf("-----------------------------------------------\n");
    //printf("insert %s\n", file_path[k]);
    //printf("-----------------------------------------------\n");
    
    char* fpath = malloc(1+strlen(file_path[k]));
    //char* ppath = malloc(1+file_path[k]);
    int l=0;
    while(l < max_path) {
      node* pnode = (node*)malloc(sizeof(node));                                             
      
      //printf("file_path_token[%d][%d] = %s\n", k, l,file_path_token[k][l]);
      if(file_path_token[k][l] == NULL)
	break;
      
      
      
      /* copy file path */
      if(l == 0) {
	pnode = node_find(*tree, "/");
	//printf("file_path_token[k][l] = %s\n", file_path_token[k][l]);
	strcpy(fpath, file_path_token[k][l]);
	//printf("333333333\n");
	//printf(" file path to be added (fpath) = %s\n", fpath);                              
      }
      
      else {
	/* find parent node */
	pnode = node_find(*tree, fpath);
	//printf("fpath = %s\n", fpath);                                                       
	//printf("pnode path = %s   name = %s\n", pnode->path, pnode->name);                   
	
	
	strcat(fpath, s);
	strcat(fpath, file_path_token[k][l]);
	//printf(" file path to be added (fpath) = %s\n", fpath);
      }
      
      node* n;
      
      if(l == max_path-1 || file_path_token[k][l+1] == NULL) {
	//printf("@@ leaf!!!\n");
	n = create(file_path[k], file_size[k]);
      }
      else {
	n = create(fpath, 0);
      }
      
      //printf("+++++++++++++++++++++++++++++++++\n");
      //printf("pnode path = %s   name = %s\n", pnode->path, pnode->name);
      //printf("child node path = %s   name = %s\n\n", n->path, n->name);
      insert(&(pnode->child), n, k, l);
            
      l++;
      //free(fname);
      //free(pnode);
    } // end while
    //free(fpath);
  } // end for
} // end build_tree()		



node* create(char *path,  int size) {
  node *root = (node*)malloc(sizeof(node));
  strcpy(root->path, path);
  root->size = size;
  root->child = NULL;
  root->sibling = NULL;

  return root;
}




void insert(node **root, node *child, int idx, int tk_idx) {
  node *last_child = NULL;
  char *temp;
  const char s[2] = "/";
  char* token;
  char* fpath;
  char* name;


  //printf("***** insert *****\n");
  
  if(*root == NULL) {
    *root = child;
    //printf("@@Added!!   ==>     child->path = %s  name = %s\n", child->path, child->name);
    return;
  }

  int cur_idx = tk_idx;

  //printf(" root->path (from parameter)= %s\n", (*root)->path);
  //printf(" root->name (from parameter)= %s\n", (*root)->name);
  //printf(" child->path (from parameter)  = %s\n", child->path);
 
  
  if(file_path_token[idx][tk_idx] == NULL)
    return;

  if(strcmp((*root)->path, child->path) == 0)
    return;


  //printf(" root->path (from parameter)= %s\n", (*root)->path);
  //printf(" root->name (from parameter)= %s\n", (*root)->name);
  //printf(" child->path (from parameter)  = %s\n", child->path);


  last_child = *root;
  while(last_child->sibling != NULL) {
    last_child = last_child->sibling;
    if(strcmp(last_child->path, child->path) == 0)
      return;
  }
  insert(&(last_child->sibling), child, idx, cur_idx);

   
} // end insert()




node* node_find(node* root, char *path) {
  node* found = NULL;

  if(root == NULL)
    return NULL;
  
  if(strcmp(root->path, path) == 0)
    return root;

  if((found = node_find(root->child, path)) == NULL)
     found = node_find(root->sibling, path);
     
  return found;
}





void print_tree(node* root)
{

  if(root != NULL) {
    //printf("path = %s  |  name = %s\n", root->path, root->name);
    printf(" %s\t", root->path);
  
    node* sib = root->sibling;
    print_tree(sib);


    node* child = root->child;
    if(child == NULL)
      return;
    printf("\n");
    print_tree(child);
  }
}
