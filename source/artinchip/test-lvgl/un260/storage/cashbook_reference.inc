/* Storage worker only. Check every archive, not just the bounded archive list. */
static bool book_uses_operator(const cashbook_t *book,uint32_t id)
{
    for(unsigned i=0;i<book->run_count;++i)if(book->runs[i].result.operator_id==id)return true;
    for(unsigned i=0;i<book->close_count;++i)if(book->closes[i].operator_id==id)return true;
    return false;
}
static bool check_operator_reference(void)
{
    reference_result=CASHBOOK_REF_CLEAR;
    if(book_uses_operator(published,reference_id)){reference_result=CASHBOOK_REF_USED;return true;}
    DIR *dir=opendir(CASHBOOK_DIRECTORY);if(!dir)return errno==ENOENT;
    bool ok=true;struct dirent *entry;
    while(ok){
        errno=0;entry=readdir(dir);if(!entry){if(errno)ok=false;break;}
        unsigned id;int end=0;
        if(sscanf(entry->d_name,"cashbook-archive-%10u.bin%n",&id,&end)!=1||!end||entry->d_name[end])continue;
        int fd=openat(dirfd(dir),entry->d_name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
        ok=fd>=0&&checkpoint_read(fd,draft);if(fd>=0&&close(fd))ok=false;
        if(ok&&book_uses_operator(draft,reference_id)){reference_result=CASHBOOK_REF_USED;break;}
    }
    if(closedir(dir))ok=false;
    return ok;
}
