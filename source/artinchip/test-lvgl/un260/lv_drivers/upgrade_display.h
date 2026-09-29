/* Maintenance-only display. No LVGL, protocol, motor or touch dependency.
 * Uses boot_light.c's existing framebuffer validation and display lease. */
#ifndef UN260_UPGRADE_DISPLAY_H
#define UN260_UPGRADE_DISPLAY_H
#ifndef UPGRADE_STATUS_PATH
#define UPGRADE_STATUS_PATH "/tmp/ui_update.status"
#endif
static const unsigned char upgrade_glyphs[36][5] = {
 {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
 {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
 {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
 {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
 {62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
 {3,4,120,4,3},{97,81,73,69,67},{62,81,73,69,62},{0,66,127,64,0},
 {98,81,73,73,70},{34,65,73,73,54},{24,20,18,127,16},{39,69,69,69,57},
 {60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30}
};
static void upgrade_rect(uint8_t *pixels,unsigned stride,int x,int y,int w,int h,uint32_t color)
{
    for(int row=y;row<y+h&&row<400;row++) for(int col=x;col<x+w&&col<1280;col++)
        if(row>=0&&col>=0)memcpy(pixels+(size_t)row*stride+(size_t)col*4,&color,4);
}
static void upgrade_text(uint8_t *pixels,unsigned stride,int x,int y,const char *text,int scale,uint32_t color)
{
    for(;*text&&x<1220;text++,x+=6*scale) {
        unsigned char c=(unsigned char)*text;
        if(c>='a'&&c<='z')c-=32;
        int index=c>='A'&&c<='Z'?c-'A':c>='0'&&c<='9'?26+c-'0':-1;
        if(index>=0)for(int col=0;col<5;col++)for(int row=0;row<7;row++)
            if(upgrade_glyphs[index][col]&(1U<<row))
                upgrade_rect(pixels,stride,x+col*scale,y+row*scale,scale,scale,color);
    }
}
static void upgrade_render(uint8_t *pixels,unsigned stride)
{
    int progress=0,failed=0,success=0;
    char line[256],message[160]="CHECKING THE SAME USB PACKAGE";
    FILE *status=fopen(UPGRADE_STATUS_PATH,"r");
    if(status) {
        while(fgets(line,sizeof(line),status)) {
            if(!strncmp(line,"progress=",9))progress=atoi(line+9);
            if(!strncmp(line,"stage=fail",10))failed=1;
            if(!strncmp(line,"success=1",9))success=1;
            if(!strncmp(line,"message=",8)&&line[8]!='\n') {
                snprintf(message,sizeof(message),"%.150s",line+8);
                message[strcspn(message,"\r\n")]=0;
            }
        }
        fclose(status);
    }
    if(progress<0)progress=0;
    if(progress>100)progress=100;
    uint32_t accent=failed?0xffb76313:0xff195bbb;
    upgrade_rect(pixels,stride,0,0,1280,400,0xfff3f5f6);
    upgrade_text(pixels,stride,64,42,"UN260 SYSTEM UPDATE",3,0xff576c79);
    upgrade_text(pixels,stride,64,104,failed?"UPDATE PAUSED":success?"UPDATE COMPLETE":"UPDATING YOUR DEVICE",5,accent);
    upgrade_text(pixels,stride,64,179,failed?"KEEP BACKUPS AND CHECK THE ERROR":"KEEP USB AND POWER CONNECTED",3,0xff20313b);
    upgrade_rect(pixels,stride,64,238,1152,12,0xffdce3e8);
    upgrade_rect(pixels,stride,64,238,1152*progress/100,12,accent);
    /* Wrap the diagnostic instead of cropping away the actionable suffix. */
    char first[91];snprintf(first,sizeof(first),"%.90s",message);
    upgrade_text(pixels,stride,64,283,first,2,0xff576c79);
    if(strlen(message)>90)upgrade_text(pixels,stride,64,307,message+90,2,0xff576c79);
    upgrade_text(pixels,stride,64,353,failed?"RAM LOG UI UPDATE LOG  USB COPY MAY BE UNAVAILABLE":"BACKUP   STORAGE   APPLICATION   VERIFY",2,0xff576c79);
}
#endif
