#ifndef BOOT_D_MOTION_H
#define BOOT_D_MOTION_H
/* HTML sin(pi * phase / 900)^2, sampled offline at 15ms; no device libm. */
static inline float boot_d_smooth(float p){if(p<=0)return 0;if(p>=1)return 1;return p*p*(3-2*p);}
static inline float boot_d_dot(unsigned elapsed,unsigned index){
    static const float wave[61]={0.0000000f,0.0027391f,0.0109262f,0.0244717f,0.0432273f,0.0669873f,0.0954915f,0.1284276f,0.1654347f,0.2061074f,0.2500000f,0.2966317f,0.3454915f,0.3960442f,0.4477358f,0.5000000f,0.5522642f,0.6039558f,0.6545085f,0.7033683f,0.7500000f,0.7938926f,0.8345653f,0.8715724f,0.9045085f,0.9330127f,0.9567727f,0.9755283f,0.9890738f,0.9972609f,1.0000000f,0.9972609f,0.9890738f,0.9755283f,0.9567727f,0.9330127f,0.9045085f,0.8715724f,0.8345653f,0.7938926f,0.7500000f,0.7033683f,0.6545085f,0.6039558f,0.5522642f,0.5000000f,0.4477358f,0.3960442f,0.3454915f,0.2966317f,0.2500000f,0.2061074f,0.1654347f,0.1284276f,0.0954915f,0.0669873f,0.0432273f,0.0244717f,0.0109262f,0.0027391f,0.0000000f};
    unsigned start=2500U+index*150U;
    if(elapsed<start||elapsed-start>=5400U)return 0;
    unsigned p=(elapsed-start)%1800U;
    if(p>=900U)return 0;
    unsigned k=p/15U;float f=(float)(p%15U)/15;
    return wave[k]+(wave[k+1]-wave[k])*f;
}
#endif
