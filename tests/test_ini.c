#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ini.h"
char *Platform_LoadAssetText(const char *name, size_t *len_out){
  FILE*f=fopen(name,"rb"); if(!f) return NULL;
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  char*b=malloc(n+1); size_t g=fread(b,1,n,f); fclose(f); b[g]='\0';
  if(len_out) *len_out=g;
  return b;
}
int main(int argc,char**argv){
  struct INI*ini=ini_open(argv[1]);
  if(!ini){printf("open failed\n");return 1;}
  const char*s; size_t ls; int nsec=0,npair=0;
  while(ini_next_section(ini,&s,&ls)){
    nsec++; printf("[%.*s]\n",(int)ls,s);
    const char*k,*v; size_t lk,lv;
    while(ini_read_pair(ini,&k,&lk,&v,&lv)){
      npair++;
      printf("  key='%.*s' (%zu)  val='%.*s' (%zu)  atoi=%d\n",(int)lk,k,lk,(int)lv,v,lv,atoi(v));
    }
  }
  printf("sections=%d pairs=%d\n",nsec,npair);
  ini_close(ini); return 0;
}
