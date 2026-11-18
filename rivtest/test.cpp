#include "byml_riv.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc,char**argv){
    FILE*f=fopen(argv[1],"rb");fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);
    uint8_t* buf=(uint8_t*)aligned_alloc(16,n+16);fread(buf,1,n,f);fclose(f);
    uint32_t outLen=0,an=0;int err=0;
    uint8_t* r=riv::AddRival(buf,atoi(argv[3]),malloc,free,&outLen,&an,&err);
    printf("err=%d actors=%u outLen=%u\n",err,an,outLen);
    if(r){FILE*o=fopen(argv[2],"wb");fwrite(r,1,outLen,o);fclose(o);}
    return r?0:1;
}
