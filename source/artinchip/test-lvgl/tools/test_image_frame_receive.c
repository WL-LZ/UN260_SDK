#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "un260/lv_system/app_clock.h"
#include "un260/lv_drivers/uart_io.h"
#include "un260/protocol/protocol_frame_parser.h"
#include "un260/protocol/protocol_frame_queue.h"
#include "un260/protocol/protocol_request.h"
#include "un260/protocol/protocol_rx_service.h"
#include "un260/protocol/stream_data_reply.h"
#include "un260/counting/counting_session_state.h"

/* Currency-operation presentation is outside the stream/RX boundary fixture. */
static counting_session_state_t g_counting_session;
static bool app_fault_recovery_observe_setting_detail(
    uint8_t cmd,const uint8_t *buf,uint16_t len,const counting_session_state_t *session)
{(void)cmd;(void)buf;(void)len;(void)session;return false;}

static unsigned images, waves, wave_ends;
static uint16_t routed_wave_mask;
static uint16_t last_sample_bytes, last_wave_bytes;
static uint8_t last_image_source;
static void route_image_payload(const uint8_t *payload, uint16_t len)
{
    image_data_reply_view_t image = image_data_reply_parse(payload, len);
    assert(image.kind != IMAGE_DATA_REPLY_INVALID);
    images++;
    if (image.kind == IMAGE_DATA_REPLY_SCANLINE) {
        last_sample_bytes = image.sample_count;
        last_image_source = image.image_id;
    }
}
static void route_wave_payload(const uint8_t *payload, uint16_t len)
{
    wave_data_reply_view_t wave = wave_data_reply_parse(payload, len);
    assert(wave.kind != WAVE_DATA_REPLY_INVALID);
    if (len==1 && payload[0]==0xFF) { wave_ends++; return; }
    assert(wave.kind == WAVE_DATA_REPLY_CHANNEL);
    last_wave_bytes = wave.value_count;
    routed_wave_mask |= (uint16_t)(1U << (wave.channel - 1U));
    waves++;
}
static void ui_page_dynamic_capture_on_frame(uint8_t command,const uint8_t *payload,uint16_t len)
{
    if(command==0x47) route_image_payload(payload,len);
    else { assert(command==0x48); route_wave_payload(payload,len); }
}
#include "image_runtime_route.inc"

static const uint8_t ack[] = {0xFD,0xDF,0x06,0x38,0x00,0x83};
static const uint8_t end_image[] = {0xFD,0xDF,0x06,0x47,0xFF,0x63};

static void make_header(uint8_t *data, uint8_t source, uint16_t width, uint16_t height)
{
    const uint8_t header[] = {0xFD,0xDF,11,0x47,0,source,
                              width>>8,width,height>>8,height,0x31};
    memcpy(data, header, sizeof(header));
}
static uint16_t make_line(uint8_t *data, uint8_t source, uint16_t width,
                          uint16_t height, uint16_t sequence)
{
    uint16_t samples = image_data_reply_scanline_bytes(source,width,height);
    assert(samples);
    uint16_t size = samples + 8U;
    data[0]=0xFD; data[1]=0xDF; data[2]=(uint8_t)size;
    data[3]=0x47; data[4]=source; data[5]=sequence>>8; data[6]=sequence;
    for (uint16_t i=0;i<samples;i++) data[i+7]=(uint8_t)(i*17U+9U);
    /* Pixel bytes may contain a complete ordinary ACK or a false FD/DF frame. */
    if (samples>=179) {
        memcpy(data+160,ack,sizeof(ack));
        const uint8_t false_frame[]={0xFD,0xDF,28,0xDF,0x1B,0xD7};
        memcpy(data+180,false_frame,sizeof(false_frame));
    }
    data[size-1]=0xA9; /* RX trailer/CRC is not assumed to equal TX 0x0A. */
    return size;
}
static unsigned feed(protocol_frame_parser_t *parser, const uint8_t *data,
                      uint16_t size, uint16_t expected_len)
{
    unsigned ready=0;
    protocol_frame_view_t frame;
    for (uint16_t i=0;i<size;i++) {
        protocol_frame_parse_result_t result=protocol_frame_parser_feed(parser,data[i],&frame);
        if (result!=PROTOCOL_FRAME_PARSE_INCOMPLETE&&result!=PROTOCOL_FRAME_PARSE_READY)
            fprintf(stderr,"parse error=%u i=%u size=%u source=%u lenbyte=%u\n",
                    result,i,size,data[4],data[2]);
        assert(result==PROTOCOL_FRAME_PARSE_INCOMPLETE||result==PROTOCOL_FRAME_PARSE_READY);
        if (result==PROTOCOL_FRAME_PARSE_READY) {
            assert(i==size-1 && frame.len==expected_len);
            assert(memcmp(frame.data,data,size)==0);
            ready++;
        }
    }
    return ready;
}
static void parser_cases(void)
{
    protocol_frame_parser_t parser;
    uint8_t header[11],data[PROTOCOL_FRAME_RX_MAX_SIZE];
    for (uint8_t source=1;source<=6;source++) {
        protocol_frame_parser_init(&parser);
        make_header(header,source,400,200);
        assert(feed(&parser,header,sizeof(header),sizeof(header))==1);
        uint16_t count=image_data_reply_scanline_count(source,400,200);
        assert(count==(source<=2?400:200));
        for (uint16_t sequence=1;sequence<=count;sequence++) {
            assert(make_line(data,source,400,200,sequence)==408);
            assert(feed(&parser,data,408,408)==1);
        }
        assert(feed(&parser,ack,sizeof(ack),sizeof(ack))==1);
        assert(feed(&parser,end_image,sizeof(end_image),sizeof(end_image))==1);
        /* The terminator clears geometry; a standalone short image stays short. */
        uint8_t short_data[]={0xFD,0xDF,9,0x47,source,0,1,0x55,0x14};
        assert(feed(&parser,short_data,sizeof(short_data),sizeof(short_data))==1);
    }
    for (uint16_t low=0;low<=4;low++) {
        protocol_frame_parser_init(&parser);
        make_header(header,3,248+low,200);
        assert(feed(&parser,header,11,11)==1);
        uint16_t size=make_line(data,3,248+low,200,1);
        assert(size==256+low && data[2]==low);
        assert(feed(&parser,data,size,size)==1);
    }
    const uint16_t widths[]={248,249,250,251,252,504,512};
    for (unsigned i=0;i<sizeof(widths)/sizeof(widths[0]);i++) {
        protocol_frame_parser_init(&parser);
        make_header(header,1,widths[i],384);
        assert(feed(&parser,header,11,11)==1);
        assert(image_data_reply_scanline_count(1,widths[i],384)==768);
        uint16_t size=make_line(data,1,widths[i],384,768);
        assert(size==widths[i]+8U);
        if (widths[i]<=252) assert(data[2]==widths[i]-248U);
        assert(feed(&parser,data,size,size)==1);
    }
    /* White packet bytes follow width, even when width differs from 2*height. */
    protocol_frame_parser_init(&parser);
    make_header(header,1,400,3);
    assert(feed(&parser,header,11,11)==1);
    uint16_t size=make_line(data,1,400,3,6);
    assert(size==408 && feed(&parser,data,size,size)==1);
    protocol_frame_parser_init(&parser);
    make_header(header,2,3,2);
    assert(feed(&parser,header,11,11)==1);
    for (uint16_t sequence=1;sequence<=4;sequence++) {
        size=make_line(data,2,3,2,sequence);
        assert(size==11 && feed(&parser,data,size,size)==1);
    }
    protocol_frame_parser_init(&parser);
    make_header(header,3,512,384);
    assert(feed(&parser,header,11,11)==1);
    size=make_line(data,3,512,384,1);
    assert(size==520 && feed(&parser,data,size,size)==1);

    /* Another source's complete block is consumed without forwarding it. */
    protocol_frame_parser_init(&parser);
    make_header(header,3,512,384);
    assert(feed(&parser,header,11,11)==1);
    size=make_line(data,1,512,384,1);
    assert(size==520 && feed(&parser,data,size,0)==0);
    size=make_line(data,3,512,384,1);
    assert(feed(&parser,data,size,size)==1);
    assert(feed(&parser,ack,sizeof(ack),sizeof(ack))==1);

    /* A known source with inconsistent LEN must not use the short-frame path. */
    protocol_frame_parser_init(&parser);
    make_header(header,1,400,200);
    assert(feed(&parser,header,11,11)==1);
    size=make_line(data,1,400,200,1);
    data[2]++;
    protocol_frame_view_t invalid;
    for (unsigned i=0;i<4;i++)
        assert(protocol_frame_parser_feed(&parser,data[i],&invalid)==PROTOCOL_FRAME_PARSE_INCOMPLETE);
    assert(protocol_frame_parser_feed(&parser,data[4],&invalid)==PROTOCOL_FRAME_PARSE_INVALID_LENGTH);
    assert(parser.image_source==0 && parser.image_width==0 && parser.image_height==0);
    assert(feed(&parser,ack,6,6)==1);
    assert(feed(&parser,header,11,11)==1);
    size=make_line(data,1,400,200,1);
    assert(feed(&parser,data,size,size)==1);

    protocol_frame_parser_init(&parser);
    make_header(header,1,400,200);
    assert(feed(&parser,header,11,11)==1);
    size=make_line(data,1,400,200,1);
    assert(feed(&parser,data,120,0)==0);
    assert(protocol_frame_parser_reset_partial_image(&parser));
    assert(!protocol_frame_parser_reset_partial_image(&parser));
    assert(feed(&parser,ack,sizeof(ack),sizeof(ack))==1);
    /* init and partial reset both clear old geometry. */
    uint8_t short_data[]={0xFD,0xDF,9,0x47,1,0,1,0x55,0x14};
    assert(feed(&parser,short_data,9,9)==1);
    assert(feed(&parser,ack,4,0)==0);
    assert(!protocol_frame_parser_reset_partial_image(&parser));
    protocol_frame_view_t partial;
    assert(protocol_frame_parser_feed(&parser,ack[4],&partial)==PROTOCOL_FRAME_PARSE_INCOMPLETE);
    assert(protocol_frame_parser_feed(&parser,ack[5],&partial)==PROTOCOL_FRAME_PARSE_READY);
    assert(partial.len==6 && memcmp(partial.data,ack,6)==0);
}
static bool pending_probe(uint8_t command,uint64_t start,uint64_t deadline)
{
    return protocol_rx_service_has_command_between(command,start,deadline)||
           protocol_frame_queue_has_command_between(command,start,deadline);
}
static protocol_frame_t pop_frame(uint8_t command,uint16_t size)
{
    protocol_frame_t frame;
    uint64_t limit=app_clock_monotonic_ms()+2000;
    while (!protocol_frame_queue_pop(&frame)) {
        assert(app_clock_monotonic_ms()<limit);
        usleep(1000);
    }
    assert(frame.len==size && frame.data[3]==command);
    assert(frame.received_ms>0);
    return frame;
}
static void send_bytes(int fd,const uint8_t *data,size_t size,size_t chunk)
{
    for (size_t offset=0;offset<size;) {
        size_t part=size-offset<chunk?size-offset:chunk;
        assert(write(fd,data+offset,part)==(ssize_t)part);
        offset+=part;
        if (offset<size) usleep(2000);
    }
}
static void route(protocol_frame_t *frame)
{
    assert(app_command_runtime_dispatch(frame->data[3],frame->data,frame->len));
}
static void actual_rx_cases(void)
{
    int pipes[2];
    assert(pipe(pipes)==0);
    int log_fd=open("/dev/null",O_WRONLY);
    assert(log_fd>=0);
    protocol_frame_queue_clear();
    assert(protocol_rx_service_start(pipes[0],log_fd));
    assert(!protocol_rx_service_start(pipes[0],log_fd));
    protocol_request_set_pending_reply_probe(pending_probe);
    protocol_request_t request=PROTOCOL_REQUEST_INITIALIZER(100);
    assert(protocol_request_begin_for_command(&request,0x38));
    uint8_t header[11],data[PROTOCOL_FRAME_RX_MAX_SIZE],wave[250];
    make_header(header,1,400,200);
    send_bytes(pipes[1],header,11,11);
    assert(make_line(data,1,400,200,1)==408);
    send_bytes(pipes[1],data,408,37);
    protocol_frame_t frame=pop_frame(0x47,11); route(&frame);
    frame=pop_frame(0x47,408); route(&frame);
    assert(last_sample_bytes==400 && last_image_source==1);
    assert(!pending_probe(0x38,request.started_ms,request.started_ms+request.timeout_ms));
    send_bytes(pipes[1],ack,sizeof(ack),sizeof(ack));
    uint64_t limit=app_clock_monotonic_ms()+70;
    while (!pending_probe(0x38,request.started_ms,request.started_ms+request.timeout_ms)) {
        assert(app_clock_monotonic_ms()<limit); usleep(1000);
    }
    /* Consumer delay after RX must not cause a false timeout. */
    while(app_clock_monotonic_ms()<request.started_ms+request.timeout_ms+10) usleep(1000);
    assert(!protocol_request_take_timeout(&request));
    frame=pop_frame(0x38,6);
    protocol_request_result_context_begin(frame.received_ms);
    assert(protocol_request_take_result(&request));
    protocol_request_result_context_end();
    memset(wave,0x55,sizeof(wave));
    wave[0]=0xFD;wave[1]=0xDF;wave[2]=250;wave[3]=0x48;wave[4]=1;
    send_bytes(pipes[1],wave,250,61);
    frame=pop_frame(0x48,250);route(&frame);
    assert(last_wave_bytes==244 && waves==1);
    /* Real UART worker/FIFO/production route must retain all ten selected types,
     * including types 8..10 after the first seven, with embedded FDDF bytes. */
    unsigned before_waves = waves;
    uint8_t burst[10U * sizeof(wave)];
    for (unsigned channel=1;channel<=10;channel++) {
        wave[4]=(uint8_t)channel;
        memcpy(&wave[40],ack,sizeof(ack));
        memcpy(&burst[(channel-1U)*sizeof(wave)],wave,sizeof(wave));
    }
    send_bytes(pipes[1],burst,sizeof(burst),53);
    for (unsigned channel=1;channel<=10;channel++) {
        frame=pop_frame(0x48,250);assert(frame.data[4]==channel);route(&frame);
    }
    assert(waves==before_waves+10 && routed_wave_mask==0x3ffU && last_wave_bytes==244);
    const uint8_t magnetic_end[]={0xFD,0xDF,6,0x48,0xFF,0x83};
    unsigned before_ends=wave_ends;
    send_bytes(pipes[1],magnetic_end,sizeof(magnetic_end),2);
    frame=pop_frame(0x48,6);route(&frame);assert(wave_ends==before_ends+1);
    assert(!protocol_frame_queue_has_pending());
    send_bytes(pipes[1],end_image,6,6);
    frame=pop_frame(0x47,6);route(&frame);

    /* Wrong RGB source is dropped without producing its embedded ordinary ACK. */
    make_header(header,3,512,384);
    send_bytes(pipes[1],header,11,11);
    frame=pop_frame(0x47,11);route(&frame);
    uint16_t size=make_line(data,1,512,384,1);
    send_bytes(pipes[1],data,size,89);
    size=make_line(data,3,512,384,1);
    send_bytes(pipes[1],data,size,71);
    frame=pop_frame(0x47,520);route(&frame);
    assert(last_image_source==3 && last_sample_bytes==512);
    assert(!protocol_frame_queue_has_pending());

    /* RX idle recovery is limited to an extended image partial. */
    make_header(header,1,400,200);
    send_bytes(pipes[1],header,11,11);
    frame=pop_frame(0x47,11);route(&frame);
    size=make_line(data,1,400,200,1);
    send_bytes(pipes[1],data,100,100);
    usleep(1100000);
    send_bytes(pipes[1],ack,6,6);
    frame=pop_frame(0x38,6);
    send_bytes(pipes[1],header,11,11);
    send_bytes(pipes[1],data,size,83);
    frame=pop_frame(0x47,11);route(&frame);
    frame=pop_frame(0x47,408);route(&frame);
    assert(last_sample_bytes==400);
    assert(!protocol_frame_queue_has_pending());
    protocol_rx_service_stop();
    protocol_rx_service_stop();
    assert(!protocol_rx_service_has_command_between(0x38,0,UINT64_MAX));
    assert(protocol_rx_service_start(pipes[0],log_fd));
    uint8_t short_data[]={0xFD,0xDF,9,0x47,1,0,1,0x55,0x14};
    send_bytes(pipes[1],short_data,9,9);
    frame=pop_frame(0x47,9);route(&frame);
    assert(last_sample_bytes==1);
    protocol_rx_service_stop();
    protocol_request_set_pending_reply_probe(NULL);
    close(pipes[0]);close(pipes[1]);close(log_fd);
}
static void framing_limits(void)
{
    uint8_t data[PROTOCOL_FRAME_RX_MAX_SIZE+1],payload[251],out[256];
    memset(payload,0,sizeof(payload));
    assert(protocol_frame_build(out,sizeof(out),0x38,payload,250)==255);
    assert(protocol_frame_build(out,sizeof(out),0x38,payload,251)<0);
    uint16_t size=make_line(data,1,512,384,1);
    assert(size==520 && protocol_frame_is_valid(data,size));
    data[3]=0x48;assert(!protocol_frame_is_valid(data,size));
    data[3]=0x47;data[4]=0;assert(!protocol_frame_is_valid(data,size));
    data[4]=7;assert(!protocol_frame_is_valid(data,size));
    data[4]=1;data[2]++;assert(!protocol_frame_is_valid(data,size));
    data[2]=(uint8_t)(PROTOCOL_FRAME_RX_MAX_SIZE+1);
    assert(!protocol_frame_is_valid(data,PROTOCOL_FRAME_RX_MAX_SIZE+1));
    protocol_frame_queue_clear();
    size=make_line(data,1,512,384,1);
    assert(protocol_frame_queue_push_wait_at(data,size,0,123456));
    protocol_frame_t frame;
    assert(protocol_frame_queue_pop(&frame) && frame.len==520 && frame.received_ms==123456);
    assert(memcmp(data,frame.data,size)==0);
    /* Keep the generic extended RX capacity/FIFO boundary covered separately
     * from the controller's current maximum width-byte block. */
    memset(data,0x55,sizeof(data));
    size=PROTOCOL_FRAME_RX_MAX_SIZE;
    data[0]=0xFD;data[1]=0xDF;data[2]=(uint8_t)size;data[3]=0x47;
    data[4]=1;data[5]=0;data[6]=1;
    assert(protocol_frame_is_valid(data,size));
    assert(protocol_frame_queue_push_wait_at(data,size,0,654321));
    assert(protocol_frame_queue_pop(&frame) && frame.len==size && frame.received_ms==654321);
    assert(memcmp(data,frame.data,size)==0);
    data[2]=(uint8_t)(size+1);
    assert(!protocol_frame_is_valid(data,size+1));
}
int main(void)
{
    framing_limits();
    parser_cases();
    actual_rx_cases();
    puts("image plus ten-type magnetic RX parser/real pipe/queue/dynamic runtime route/deadline/idle tests passed");
    return 0;
}
