// Physarum (slime mold) simulation -- Jeff Jones model.
// 300k agents sense a chemical trail, steer toward it, and deposit more.
// Trail diffuses + decays -> living vein-like networks emerge.
// Build: gcc -O3 -march=native -o physarum physarum.c -lm && ./physarum
// Output: physarum.png (dependency-free PNG writer included)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define W 1024
#define H 1024
#define N 300000
#define STEPS 600
#define SENSE_DIST 9.0f
#define SENSE_ANGLE 0.45f
#define TURN 0.3f
#define SPEED 1.1f
#define DEPOSIT 0.8f
#define DECAY 0.94f

static float trail[W*H], tmp[W*H];
static float ax[N], ay[N], aa[N];
static uint64_t s = 88172645463325252ULL;
static float rnd(void){ s^=s<<13; s^=s>>7; s^=s<<17; return (s>>40)/(float)(1<<24); }

static float sample(float x, float y){
    int ix=((int)floorf(x)%W+W)%W, iy=((int)floorf(y)%H+H)%H;
    return trail[iy*W+ix];
}

// ---- minimal PNG writer (stored deflate blocks) ----
static uint32_t crct[256];
static void mkcrc(void){ for(uint32_t n=0;n<256;n++){uint32_t c=n;for(int k=0;k<8;k++)c=c&1?0xedb88320u^(c>>1):c>>1;crct[n]=c;} }
static uint32_t crc(uint32_t c,const uint8_t*b,size_t n){c=~c;while(n--)c=crct[(c^*b++)&255]^(c>>8);return ~c;}
static void be32(uint8_t*p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static void chunk(FILE*f,const char*t,const uint8_t*d,uint32_t n){
    uint8_t h[8],cc[4]; be32(h,n); memcpy(h+4,t,4); fwrite(h,1,8,f);
    if(n) fwrite(d,1,n,f);
    uint8_t*buf=malloc(n+4); memcpy(buf,t,4); if(n) memcpy(buf+4,d,n);
    be32(cc,crc(0,buf,n+4)); free(buf); fwrite(cc,1,4,f);
}
static void write_png(const char*fn,const uint8_t*rgb){
    mkcrc(); FILE*f=fopen(fn,"wb");
    fwrite("\x89PNG\r\n\x1a\n",1,8,f);
    uint8_t ihdr[13]; be32(ihdr,W); be32(ihdr+4,H); ihdr[8]=8; ihdr[9]=2; ihdr[10]=ihdr[11]=ihdr[12]=0;
    chunk(f,"IHDR",ihdr,13);
    size_t rowb=W*3+1, raw=rowb*H;
    uint8_t*r=malloc(raw);
    for(int y=0;y<H;y++){ r[y*rowb]=0; memcpy(r+y*rowb+1,rgb+(size_t)y*W*3,W*3); }
    size_t nblk=(raw+65534)/65535, zn=2+raw+nblk*5+4;
    uint8_t*z=malloc(zn); size_t p=0; z[p++]=0x78; z[p++]=1;
    uint32_t a=1,b=0;
    for(size_t i=0;i<raw;i++){a=(a+r[i])%65521;b=(b+a)%65521;}
    for(size_t o=0;o<raw;o+=65535){
        size_t l=raw-o<65535?raw-o:65535;
        z[p++]=(o+l>=raw); z[p++]=l&255; z[p++]=l>>8; z[p++]=~l&255; z[p++]=(~l>>8)&255;
        memcpy(z+p,r+o,l); p+=l;
    }
    be32(z+p,(b<<16)|a); p+=4;
    chunk(f,"IDAT",z,p); chunk(f,"IEND",NULL,0);
    fclose(f); free(r); free(z);
}

static void colormap(float v,uint8_t*o){ // black -> deep blue -> magenta -> gold -> white
    v=v<0?0:v>1?1:v; v=powf(v,0.6f);
    static const float st[5][3]={{0,0,0},{20,10,90},{200,30,120},{255,190,40},{255,255,235}};
    float t=v*4; int i=(int)t; if(i>3)i=3; float f=t-i;
    for(int c=0;c<3;c++) o[c]=(uint8_t)(st[i][c]*(1-f)+st[i+1][c]*f);
}

int main(void){
    for(int i=0;i<N;i++){ // start in a disc, facing the center
        float r=sqrtf(rnd())*H*0.4f, t=rnd()*6.2831853f;
        ax[i]=W/2+cosf(t)*r; ay[i]=H/2+sinf(t)*r; aa[i]=t+3.14159f;
    }
    for(int step=0;step<STEPS;step++){
        for(int i=0;i<N;i++){
            float x=ax[i],y=ay[i],a=aa[i];
            float f=sample(x+cosf(a)*SENSE_DIST,y+sinf(a)*SENSE_DIST);
            float l=sample(x+cosf(a-SENSE_ANGLE)*SENSE_DIST,y+sinf(a-SENSE_ANGLE)*SENSE_DIST);
            float r=sample(x+cosf(a+SENSE_ANGLE)*SENSE_DIST,y+sinf(a+SENSE_ANGLE)*SENSE_DIST);
            if(f<l&&f<r) a+=(rnd()<0.5f?-1:1)*TURN;
            else if(l>r) a-=TURN; else if(r>l) a+=TURN;
            x+=cosf(a)*SPEED; y+=sinf(a)*SPEED;
            if(x<0)x+=W; if(x>=W)x-=W; if(y<0)y+=H; if(y>=H)y-=H;
            ax[i]=x; ay[i]=y; aa[i]=a;
            trail[(int)y*W+(int)x]+=DEPOSIT;
        }
        for(int y=0;y<H;y++){ // 3x3 blur + decay
            int y0=(y+H-1)%H,y1=(y+1)%H;
            for(int x=0;x<W;x++){
                int x0=(x+W-1)%W,x1=(x+1)%W; float sum=0;
                sum+=trail[y0*W+x0]+trail[y0*W+x]+trail[y0*W+x1];
                sum+=trail[y*W+x0]+trail[y*W+x]+trail[y*W+x1];
                sum+=trail[y1*W+x0]+trail[y1*W+x]+trail[y1*W+x1];
                tmp[y*W+x]=sum/9.0f*DECAY;
            }
        }
        memcpy(trail,tmp,sizeof trail);
        if(step%100==0) fprintf(stderr,"step %d/%d\n",step,STEPS);
    }
    uint8_t*img=malloc(W*H*3);
    for(int i=0;i<W*H;i++) colormap(trail[i]/6.0f,img+i*3);
    write_png("physarum.png",img);
    puts("saved physarum.png");
    return 0;
}
