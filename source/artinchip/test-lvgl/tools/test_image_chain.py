#!/usr/bin/env python3
"""Real-file decoder-chain regression (not a hardware decode/visual test).

Compile the production PNG/JPEG/header/file-size/attempt/close functions together.
LVGL status enums are extracted verbatim from this SDK, never redefined by guess.
The host file adapter performs real fopen/read/seek/tell; only hardware ownership,
MPP packet/frame transport and unrelated profiling/cache admission are simulated.
Every PNG/JPEG resource traverses the attempt through frame acquisition, comparing
its entire compressed payload against the original. Generated boundary fixtures
exercise packet padding; explicit mutations prove both reported regressions fail.
"""
from pathlib import Path
from io import BytesIO
import os
import re
import struct
import subprocess
import tempfile
import zlib
from PIL import Image

root = Path(__file__).resolve().parents[1]
sdk = root.parents[2]
work = Path(tempfile.mkdtemp(prefix="un260-image-chain-"))
src = (root / "aic_ui/aic_dec.c").read_text()


def function(text, name):
    match = re.search(r"^static[^\n]*\b" + name + r"\s*\(", text, re.M)
    if not match:
        raise ValueError("Missing production function " + name)
    start = match.start()
    opening = text.index("{", match.end())
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end] + "\n"


def enum_for(path, symbol):
    text = path.read_text()
    for match in re.finditer(r"enum\s*\{[^}]*\};", text, re.S):
        if re.search(r"\b" + symbol + r"\b", match.group()):
            return match.group() + "\n"
    raise ValueError("Missing actual enum " + symbol)


lv_misc = sdk / "source/third-party/lvgl-8.3.2/src/misc"
enums = (enum_for(lv_misc / "lv_types.h", "LV_RES_INV") +
         enum_for(lv_misc / "lv_fs.h", "LV_FS_RES_OK") +
         enum_for(lv_misc / "lv_fs.h", "LV_FS_MODE_RD"))
names = ("stream_to_u64", "stream_to_u32", "stream_to_u16",
         "get_jpeg_format", "jpeg_get_img_size", "png_get_img_size",
         "get_file_size", "frame_dma_bytes", "aic_decoder_attempt",
         "aic_decoder_close")
actual = "\n".join(function(src, name) for name in names)
for helper in re.findall(r"^static lv_fs_res_t\s+(\w+)\s*\(", src, re.M):
    assert not re.search(r"return\s+LV_RES_(?:OK|INV)\s*;", function(src, helper)), \
        "Mixed FS/draw return code in " + helper

stub = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
''' + enums + r'''
typedef uint8_t lv_res_t;
typedef uint8_t lv_fs_res_t;
typedef struct { FILE *stream; unsigned seeks_end, seeks_zero, tells; } lv_fs_file_t;
typedef int lv_img_decoder_t;
typedef struct { const char *src; const unsigned char *img_data;
    struct { int cf; } header; } lv_img_decoder_dsc_t;
enum mpp_codec_type { MPP_CODEC_VIDEO_DECODER_PNG, MPP_CODEC_VIDEO_DECODER_MJPEG };
enum mpp_pixel_format { MPP_FMT_ARGB_8888, MPP_FMT_RGB_888, MPP_FMT_YUV420P,
    MPP_FMT_YUV422P, MPP_FMT_YUV444P, MPP_FMT_YUV400 };
enum { MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR, PACKET_FLAG_EOS,
    LV_IMG_CF_TRUE_COLOR_ALPHA, LV_IMG_CF_TRUE_COLOR };
struct mpp_buffer { struct {int width,height;} size; int stride[3],format; };
struct mpp_frame { struct mpp_buffer buf; };
struct frame_allocator { struct mpp_frame *frame; };
struct mpp_packet { void *data; uint32_t size,flag; };
struct decode_config { enum mpp_pixel_format pix_fmt;
    int bitstream_buffer_size,extra_frame_num,packet_count; };
struct mpp_decoder { void *pm; unsigned capacity; unsigned char *packet;
    struct frame_allocator *allocator; struct mpp_packet submitted; };
#define AIC_IMAGE_CACHE_BUDGET (8U*1024U*1024U)
#define PNG_HEADER_SIZE (8+12+13)
#define PNGSIG 0x89504e470d0a1a0aull
#define MNGSIG 0x8a4d4e470d0a1a0aull
#define JPEG_SOI 0xFFD8
#define JPEG_SOF 0xFFC0
static const char *inject;
static unsigned files, opens, closes, heaps, dma, decs, packets, decoded;
static unsigned payload_reads, good_rewinds, frame_gets, frame_puts;
static unsigned char *source_bytes;
static unsigned source_size;
static bool match(const char *s) { return inject && strcmp(inject,s)==0; }
static uint64_t app_clock_monotonic_us(void) {return 1;}
static unsigned app_clock_elapsed_us32(uint64_t a,uint64_t b) {return b-a;}
static bool perf_profile_is_enabled(void) {return false;}
static void perf_profile_report_image_decode(const char*s,unsigned t,unsigned b)
{(void)s;(void)t;(void)b;}
static lv_fs_res_t lv_fs_open(lv_fs_file_t*f,const char *s,int mode) {
    assert(mode==LV_FS_MODE_RD);
    if(match("file-open")) return LV_FS_RES_DENIED;
    memset(f,0,sizeof(*f)); f->stream=fopen(s,"rb");
    if(!f->stream) return LV_FS_RES_NOT_EX;
    files++; opens++; return LV_FS_RES_OK;
}
static lv_fs_res_t lv_fs_close(lv_fs_file_t*f) {
    assert(files==1 && f->stream); assert(fclose(f->stream)==0);
    files--; closes++; f->stream=NULL; return LV_FS_RES_OK;
}
static lv_fs_res_t lv_fs_seek(lv_fs_file_t*f,uint32_t n,int whence) {
    if((whence==SEEK_END && match("seek-end")) ||
       (whence==SEEK_SET && match("seek-reset")) ||
       (whence==SEEK_CUR && match("seek-jpeg"))) return LV_FS_RES_FS_ERR;
    if(fseek(f->stream,n,whence)) return LV_FS_RES_FS_ERR;
    if(whence==SEEK_END) f->seeks_end++;
    if(whence==SEEK_SET && n==0) f->seeks_zero++;
    return LV_FS_RES_OK;
}
static lv_fs_res_t lv_fs_tell(lv_fs_file_t*f,uint32_t*n) {
    if(match("tell")) return LV_FS_RES_FS_ERR;
    long pos=ftell(f->stream); if(pos<0) return LV_FS_RES_FS_ERR;
    *n=(uint32_t)pos; f->tells++; return LV_FS_RES_OK;
}
static lv_fs_res_t lv_fs_read(lv_fs_file_t*f,void*out,uint32_t n,uint32_t*read_count) {
    *read_count=0;
    bool payload=f->seeks_zero!=0;
    if((!payload && match("header-read")) || (payload && match("file-read")))
        return LV_FS_RES_HW_ERR;
    if(payload) {
        assert(ftell(f->stream)==0 && f->seeks_end==1 && f->seeks_zero==1 && f->tells==1);
        good_rewinds++; payload_reads++;
    }
    uint32_t limit=n;
    if((payload && match("short-read")) || (!payload && match("short-header"))) limit=n-1;
    *read_count=(uint32_t)fread(out,1,limit,f->stream);
    return ferror(f->stream) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK;
}
static bool lv_img_cache_reserve_bytes(unsigned b,unsigned max) {
    assert(b>0 && max==AIC_IMAGE_CACHE_BUDGET);
    return !match("cache-budget");
}
static int dmabuf_device_open(void) {
    if(match("heap-open")){errno=ENOMEM;return -1;} heaps++;return 9;
}
static void dmabuf_device_close(int f) {assert(f==9 && heaps==1);heaps--;}
static int mpp_buf_alloc(int f,struct mpp_buffer*b) {
    assert(f==9 && b->size.width>0 && b->size.height>0);
    if(match("output-dma")){errno=ENOMEM;return -1;}dma++;return 0;
}
static void mpp_buf_free(struct mpp_buffer*b) {(void)b;assert(!decs && dma==1);dma--;}
static struct frame_allocator *open_allocator(struct mpp_frame*f) {
    if(match("allocator")) return NULL;
    struct frame_allocator*a=malloc(sizeof(*a));assert(a);a->frame=f;return a;
}
static struct mpp_decoder *mpp_decoder_create(enum mpp_codec_type t) {
    (void)t;if(match("decoder-create"))return NULL;
    struct mpp_decoder*d=calloc(1,sizeof(*d));assert(d);decs++;return d;
}
static int mpp_decoder_control(struct mpp_decoder*d,int cmd,void*a) {
    assert(cmd==MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR);d->allocator=a;
    return match("decoder-control")?1:0;
}
static int mpp_decoder_init(struct mpp_decoder*d,struct decode_config*c) {
    assert(c->packet_count==1 && c->extra_frame_num==0);
    d->capacity=c->bitstream_buffer_size;d->pm=match("pm-null")?NULL:d;
    return match("decoder-init")?1:0;
}
static int mpp_decoder_get_packet(struct mpp_decoder*d,struct mpp_packet*p,unsigned n) {
    assert(d->pm);
    if(match("packet"))return 1;
    if(d->capacity<n+8U) return -ENOMEM; /* SDK packet manager's reserved gap. */
    if(match("packet-null"))return 0;
    d->packet=malloc(d->capacity);assert(d->packet);packets++;
    memset(d->packet,0xA5,d->capacity);p->data=d->packet;return 0;
}
static int mpp_decoder_put_packet(struct mpp_decoder*d,struct mpp_packet*p) {
    assert(p->data==d->packet && p->size==source_size && p->flag==PACKET_FLAG_EOS);
    assert(memcmp(p->data,source_bytes,source_size)==0);
    for(unsigned i=0;i<8;i++) assert(d->packet[source_size+i]==0);
    d->submitted=*p; return match("put-packet")?1:0;
}
static int mpp_decoder_decode(struct mpp_decoder*d) {
    assert(d->submitted.size==source_size && d->allocator && d->allocator->frame);
    if(match("decode"))return 1;decoded++;return 0;
}
static int mpp_decoder_get_frame(struct mpp_decoder*d,struct mpp_frame*f) {
    if(match("get-frame"))return 1;*f=*d->allocator->frame;frame_gets++;return 0;
}
static int mpp_decoder_put_frame(struct mpp_decoder*d,struct mpp_frame*f) {
    assert(f->buf.size.width==d->allocator->frame->buf.size.width);
    if(match("put-frame"))return 1;frame_puts++;return 0;
}
static void mpp_decoder_destory(struct mpp_decoder*d) {
    assert(decs==1 && dma==1);
    if(d->packet){assert(packets==1);packets--;free(d->packet);}decs--;free(d);
}
static void lv_ge2d_scaled_cache_drop_source(const struct mpp_frame*f) {(void)f;}
'''

tests = r'''
static void load_source(const char*path) {
    FILE*f=fopen(path,"rb");assert(f);assert(fseek(f,0,SEEK_END)==0);
    long n=ftell(f);assert(n>0 && n<16*1024*1024);source_size=(unsigned)n;
    rewind(f);source_bytes=malloc(source_size);assert(source_bytes);
    assert(fread(source_bytes,1,source_size,f)==source_size);assert(fclose(f)==0);
}
static void run_attempt(const char*path,const char*failure,const char*expected_stage) {
    assert(!files && !heaps && !dma && !decs && !packets);
    load_source(path);inject=failure;
    lv_img_decoder_dsc_t d={.src=path};const char*stage=NULL;int error=-123;unsigned bytes=0;
    unsigned begin_opens=opens,begin_closes=closes,begin_frames=frame_gets,begin_rewinds=good_rewinds;
    lv_res_t result=aic_decoder_attempt(&d,&stage,&error,&bytes);
    if(expected_stage) {
        if(result!=LV_RES_INV || strcmp(stage,expected_stage)) {
            fprintf(stderr,"Unexpected failure result path=%s inject=%s result=%d stage=%s ret=%d expected=%s\n",
                path,failure?failure:"none",result,stage,error,expected_stage);abort();
        }
        assert(!d.img_data);
    } else {
        if(result!=LV_RES_OK) {
            fprintf(stderr,"CHAIN_FAIL src=%s stage=%s ret=%d bytes=%u\n",path,stage,error,bytes);exit(23);
        }
        assert(d.img_data && bytes>0 && frame_gets==begin_frames+1 && good_rewinds==begin_rewinds+1);
        aic_decoder_close(NULL,&d);assert(!d.img_data);
        aic_decoder_close(NULL,&d); /* Closing twice must not double-release. */
    }
    assert(opens-begin_opens==closes-begin_closes);
    assert(!files && !heaps && !dma && !decs && !packets);
    free(source_bytes);source_bytes=NULL;inject=NULL;
    assert(image_mem_used()==0);
}
int main(int argc,char**argv) {
    assert(LV_RES_OK==1 && LV_RES_INV==0 && LV_FS_RES_OK==0 && LV_FS_MODE_RD==2);
    assert(argc>=5);
    const char*manifest=argv[1],*boundaries=argv[2],*jpeg=argv[3],*truncated=argv[4];
    FILE*list=fopen(manifest,"r");assert(list);char line[2048],first[2048]={0};unsigned assets=0;
    while(fgets(line,sizeof(line),list)) {
        line[strcspn(line,"\r\n")]=0;if(!*line)continue;
        if(!*first)strcpy(first,line);run_attempt(line,NULL,NULL);assets++;
    }
    assert(fclose(list)==0 && assets>0);
    printf("CHAIN PASS real-assets=%u payload_reads=%u frame_gets=%u frame_puts=%u\n",assets,payload_reads,frame_gets,frame_puts);
    list=fopen(boundaries,"r");assert(list);unsigned fixtures=0;
    while(fgets(line,sizeof(line),list)) {
        line[strcspn(line,"\r\n")]=0;if(!*line)continue;
        run_attempt(line,NULL,NULL);fixtures++;
    }
    assert(fclose(list)==0 && fixtures==10);
    printf("BOUNDARY PASS 10 valid PNG/JPEG fixtures at 1016/1017/1023/1024/16384 bytes, payload+8 padding and seek reset\n");
    struct failure {const char*inject;const char*stage;} failures[]={
        {"file-open","file-open"},{"header-read","header"},{"short-header","header"},
        {"seek-end","file-size"},{"tell","file-size"},{"seek-reset","file-size"},
        {"cache-budget","cache-budget"},{"heap-open","heap-open"},{"output-dma","output-dma"},
        {"decoder-create","decoder-create"},{"allocator","allocator"},{"decoder-control","decoder-control"},
        {"decoder-init","decoder-init"},{"pm-null","decoder-init"},{"packet","packet"},
        {"packet-null","packet"},{"file-read","file-read"},{"short-read","file-read"},
        {"put-packet","put-packet"},{"decode","decode"},{"get-frame","get-frame"},{"put-frame","put-frame"}
    };
    for(unsigned i=0;i<sizeof(failures)/sizeof(*failures);i++)
        run_attempt(first,failures[i].inject,failures[i].stage);
    run_attempt(jpeg,"seek-jpeg","header");run_attempt(truncated,NULL,"header");
    printf("FAILURE PASS %zu injections + real truncated file; every file/heap/packet/decoder/frame released; open=%u close=%u\n",
        sizeof(failures)/sizeof(*failures)+1,opens,closes);
}
'''

assets = sorted(p for p in (root / "aic_ui/lvgl_data").rglob("*")
                if p.suffix.lower() in (".png", ".jpg", ".jpeg"))
if not assets:
    raise SystemExit("No actual resources found")
manifest = work / "assets.txt"
manifest.write_text("\n".join(str(p) for p in assets) + "\n")
png = next(p for p in assets if p.suffix.lower() == ".png")
truncated = work / "truncated.png"
truncated.write_bytes(png.read_bytes()[:20])
fixtures = []
for length in (1016, 1017, 1023, 1024, 16384):
    for fmt, suffix in (("PNG", ".png"), ("JPEG", ".jpg")):
        encoded = BytesIO()
        Image.new("RGB", (16, 16), (41, 83, 127)).save(encoded, fmt)
        data = encoded.getvalue()
        if fmt == "PNG":
            # Valid text chunk before IEND, rather than a corrupt fake payload.
            text = b"padding\0" + b"x" * (length - len(data) - 12 - 8)
            chunk = struct.pack(">I", len(text)) + b"tEXt" + text
            chunk += struct.pack(">I", zlib.crc32(b"tEXt" + text) & 0xffffffff)
            data = data[:-12] + chunk + data[-12:]
        else:
            # Legal JPEG COM marker after SOI; keeps a complete baseline JPEG.
            padding = b"x" * (length - len(data) - 4)
            data = data[:2] + b"\xff\xfe" + struct.pack(">H", len(padding) + 2) + padding + data[2:]
        assert len(data) == length
        Image.open(BytesIO(data)).load()  # Independently verify valid image data.
        path = work / ("boundary_%d" % length + suffix)
        path.write_bytes(data)
        fixtures.append(path)
boundary_list = work / "boundaries.txt"
boundary_list.write_text("\n".join(str(p) for p in fixtures) + "\n")
jpeg = work / "boundary_1016.jpg"
args = [str(manifest), str(boundary_list), str(jpeg), str(truncated)]


stub = '#include "' + str(root / 'aic_ui/image_memory.c') + '"\n' + stub

def build_run(name, production, expected_error=None):
    cfile = work / (name + ".c")
    cfile.write_text(stub + production + tests)
    exe = work / name
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-function", "-Wno-unused-parameter", "-Wno-misleading-indentation",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer", str(cfile), "-o", str(exe)], check=True)
    result = subprocess.run([str(exe)] + args, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            env=dict(os.environ, ASAN_OPTIONS="detect_leaks=1"))
    print(result.stdout, end="")
    if expected_error:
        # The old function must fail an actual successful file operation at the
        # exact requested stage, not through a sanitizer/compiler failure.
        assert result.returncode == 23 and expected_error in result.stderr, result.stderr
        print("MUTATION CAUGHT", name, result.stderr.strip().splitlines()[-1])
    else:
        print(result.stderr, end="")
        result.check_returncode()


old_size = function(src, "get_file_size")
mutant_size, count = re.subn(r"return\s+LV_FS_RES_OK\s*;", "return LV_RES_OK;", old_size)
assert count == 1, "Current helper must have exactly one FS-success return"
build_run("old_file_size", actual.replace(old_size, mutant_size), "stage=file-size ret=1")
old_padding, count = re.subn(r"\(len\s*\+\s*8U?\s*\+\s*1023U?\)", "(len + 1023U)", actual)
assert count == 2, "Both reservation and SDK bitstream capacity must include len+8"
build_run("old_packet_padding", old_padding, "stage=packet ret=-12")
build_run("real_chain", actual)
print("Artifacts:", work)
