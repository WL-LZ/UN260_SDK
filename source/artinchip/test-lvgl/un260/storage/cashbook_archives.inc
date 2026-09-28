/* Worker-only archive I/O. No UI object or live counting data is accessed. */
static void archive_name(char *name,size_t size,uint32_t id)
{snprintf(name,size,"cashbook-archive-%010u.bin",id);}
static bool write_checkpoint(int dir,const char *name,const cashbook_t *book,bool replace)
{
    int flags=O_WRONLY|O_CREAT|O_NOFOLLOW|O_NONBLOCK|(replace?0:O_EXCL);
    int fd=openat(dir,name,flags,0600);if(fd<0)return false;
    header_t h={.magic=CHECKPOINT,.version=1,.size=sizeof(*book),.crc=crc(book,sizeof(*book)),.time=(uint64_t)time(NULL)};
    struct stat st;bool regular=!fstat(fd,&st)&&S_ISREG(st.st_mode);
    bool ok=regular&&(!replace||!ftruncate(fd,0))&&io(fd,&h,sizeof(h),true)&&io(fd,(void *)book,sizeof(*book),true)&&!fsync(fd);
    if(close(fd))ok=false;
    if(!ok&&regular)unlinkat(dir,name,0);
    return ok;
}
static bool checkpoint_read(int fd,cashbook_t *book)
{
    header_t h;struct stat st;
    return !fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_size==(off_t)(sizeof(h)+sizeof(*book))&&
        io(fd,&h,sizeof(h),false)&&h.magic==CHECKPOINT&&h.version==1&&h.size==sizeof(*book)&&
        io(fd,book,sizeof(*book),false)&&h.crc==crc(book,sizeof(*book))&&cashbook_valid(book);
}
static bool archive_period(void)
{
    if(!published->run_count||published->sequence==UINT32_MAX){snprintf(result,sizeof(result),"No records to archive.");return false;}
    for(unsigned i=0;i<published->group_count;i++){
        const cashbook_group_t *g=&published->groups[i];
        if(!g->excluded&&(!cashbook_closed(published,g->day)||g->review||!g->confirmed)){
            snprintf(result,sizeof(result),"Confirm or exclude every group, then close all business days before archiving.");return false;
        }
    }
    snprintf(result,sizeof(result),"Archive storage is unavailable. Original records are retained.");
    if(mkdir(CASHBOOK_DIRECTORY,0755)&&errno!=EEXIST)return false;
    int dir=open(CASHBOOK_DIRECTORY,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(dir<0)return false;
    char name[64];archive_name(name,sizeof(name),published->sequence);
    bool ok=write_checkpoint(dir,name,published,false);
    if(!ok&&errno==EEXIST){int fd=openat(dir,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);ok=fd>=0&&checkpoint_read(fd,draft)&&!memcmp(draft,published,sizeof(*draft));if(fd>=0)close(fd);}
    if(ok)ok=!fsync(dir);
    if(ok){
        cashbook_defaults(draft);draft->sequence=published->sequence+1;draft->business_day=published->business_day;
        ok=write_checkpoint(dir,"cashbook.next",draft,true);
        if(ok){fatal=true;ok=!renameat(dir,"cashbook.next",dir,"cashbook.v1")&&!fsync(dir);if(ok)fatal=false;}
    }
    close(dir);
    snprintf(result,sizeof(result),ok?"Period archived. Older records remain available in Archives.":"Archive not confirmed. Original records are retained; restart to check storage.");return ok;
}
static bool scan_archives(void)
{
    archive_draft_count=0;DIR *dir=opendir(CASHBOOK_DIRECTORY);
    if(!dir){snprintf(result,sizeof(result),errno==ENOENT?"No archived periods yet.":"Archive storage could not be read.");return errno==ENOENT;}
    struct dirent *entry;
    while((entry=readdir(dir))){unsigned id;int end=0;
        if(sscanf(entry->d_name,"cashbook-archive-%10u.bin%n",&id,&end)!=1||!end||entry->d_name[end])continue;
        cashbook_archive_t item={.id=id};unsigned pos=0;
        while(pos<archive_draft_count&&archive_draft[pos].id>id)pos++;
        if(pos>=CASHBOOK_ARCHIVES)continue;
        if(archive_draft_count<CASHBOOK_ARCHIVES)archive_draft_count++;
        memmove(archive_draft+pos+1,archive_draft+pos,(archive_draft_count-pos-1)*sizeof(item));archive_draft[pos]=item;
    }
    closedir(dir);snprintf(result,sizeof(result),"%u archived periods available.",archive_draft_count);return true;
}
static bool read_archive(void)
{
    char path[256],name[64];archive_name(name,sizeof(name),archive_id);snprintf(path,sizeof(path),CASHBOOK_DIRECTORY "/%s",name);
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);bool ok=fd>=0&&checkpoint_read(fd,view_draft);if(fd>=0)close(fd);
    snprintf(result,sizeof(result),ok?"Archived period / read only. Export or return to current records.":"Archive could not be verified. Its file was not changed.");return ok;
}
static bool export_line(int fd,const char *format,...)
{
    char line[256];va_list args;va_start(args,format);int n=vsnprintf(line,sizeof(line),format,args);va_end(args);
    return n>=0&&(size_t)n<sizeof(line)&&io(fd,line,n,true);
}
static bool export_view(void)
{
    if(!usb_storage_prepare()){snprintf(result,sizeof(result),"Insert a USB drive and try again.");return false;}
    const cashbook_t *book=archive_view?archive_view:published;
    int dir=open(CASHBOOK_USB_DIRECTORY,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
    if(dir<0){snprintf(result,sizeof(result),"USB storage could not be opened. Reinsert the drive and retry.");return false;}
    char name[96];int fd=-1;
    for(unsigned i=0;i<1000;i++){snprintf(name,sizeof(name),"UN260_records_%010u_%03u.csv",book->sequence,i);fd=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd>=0||errno!=EEXIST)break;}
    bool ok=fd>=0&&export_line(fd,"TYPE,DAY,VERSION,GROUP,ATTEMPT,SOURCE,STATUS,CURRENCY,NOTES,AMOUNT\r\n");
    for(unsigned i=0;ok&&i<book->run_count;i++){
        const cashbook_run_t *r=&book->runs[i];const cashbook_group_t *g=cashbook_group(book,r->group);
        const char *status=g->excluded?"EXCLUDED":g->confirmed&&g->selected==r->id?"COUNTED":"NOT_COUNTED";
        for(unsigned j=0;ok&&j<r->result.currencies;j++)ok=export_line(fd,"ATTEMPT,%u,0,%u,%u,%u,%s,%s,%u,%llu\r\n",g->day,g->id,r->id,r->result.source,status,r->result.money[j].code,r->result.money[j].pcs,(unsigned long long)r->result.money[j].amount);
    }
    for(unsigned i=0;ok&&i<book->close_count;i++){
        const cashbook_close_t *c=&book->closes[i];
        for(unsigned j=0;ok&&j<c->currencies;j++)ok=export_line(fd,"DAY_CLOSE,%u,%u,0,0,0,%s,%s,%u,%llu\r\n",c->day,c->revision,c->current?"CLOSED":"REOPENED",c->money[j].code,c->money[j].pcs,(unsigned long long)c->money[j].amount);
    }
    if(ok)ok=!fsync(fd);
    if(fd>=0&&close(fd))ok=false;
    if(ok)ok=!fsync(dir);
    if(!ok&&fd>=0)unlinkat(dir,name,0);
    close(dir);
    snprintf(result,sizeof(result),ok?"Saved to USB: %s":"Export failed. Check the USB drive and retry.",name);return ok;
}
