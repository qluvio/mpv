/*
 *
 */

#include "common/common.h"
#include "common/stats.h"

#include <sys/time.h>
#include <stdio.h>

// Global stats structure
struct stats _gst;
struct stats *gst;

/*
 * If the segment is manifest or init segment, record the open time
 * Else record the open time, first packet read time, & close time
 * Add each of the regular segments to an array of structs
 * Store all of the above in a global segment struct
 */

/*
 * Returns usec since epoch
 */
static long long get_usec_now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

/*
 * Returns usec since beginnning of program
 */
static long long get_usec(struct stats *st)
{
    return get_usec_now() - st->start_usec;
}

static const char *basename(const char *f)
{
    char *p = strrchr(f, '/');
    if (p) {
        printf("ELVTRC: basename=%s\n", p + 1);
        return p + 1;
    }
    else
        return f;
}

void stats_init(struct stats *st)
{
    // Using a global stats singleton
    gst = &_gst;
    st = gst;

    st->start_usec = get_usec_now();
    st->seg_duration = 6000000; // MM Todo - hard coded for now
    st->curseg_a = st->curseg_v = -1;
    st->curpacket_a = st->curpacket_v = 0;

    st->manifest_open_usec = 0;
    st->init_seg_video_open_usec = 0;
    st->init_seg_audio_open_usec = 0;
    st->first_seg_open_usec = 0;
    st->first_buf_read_usec = 0;
    st->first_packet_decode_usec = 0;
}

void segstats_init(struct segstats *ss)
{
    ss->seg_open_usec = 0;
    ss->seg_first_buf_read_usec = 0;
    ss->seg_close_usec = 0;
    seginfo_init(&ss->seginfo);
}

void seginfo_init(struct seginfo *sinf)
{
    sinf->type = dash_unset; // manifest, init-video, init-audio, video, audio
    sinf->index = -1; // if vidio or audio, the segment number
}

void packetinfo_init(struct packetinfo *pinf)
{
    pinf->type = packet_unset;
    pinf->seg_index = -1;
    pinf->start_of_segment = 0;
    pinf->end_of_segment = 0;
    pinf->pts = 0;
    pinf->seg_start = 0;
    pinf->seg_end = 0;
}

int stats_curseg(struct stats *st, int type) {
    switch (type) {
    case STREAM_AUDIO:
        return st->curseg_a;
    case STREAM_VIDEO:
        return st->curseg_v;
    default:
        return -1;
    }
}

void stats_curseg_set(struct stats *st, int type, int index)
{
    switch (type) {
    case STREAM_AUDIO:
        st->curseg_a = index;
        break;
    case STREAM_VIDEO:
        st->curseg_v = index;
        break;
    }
}

/*
   Function to translate segment URL to audio/video segment or manifest
   returns struct seg_info consisting of seg_type and seg_index
   init segment should be a very large number
*/
void url_seg_info(struct stats *st, struct seginfo *si, const char* url)
{
    // Example urls: en.mpd, en-1080p@5120000-init.m4v, en-1080p@5120000-2.m4v
    // Parse on last "." and "-"
    char *suffix = strrchr(url, '.') + 1;
    printf("ELVTRC URL_SEG_INFO suffix=%s\n", suffix);
    if (strcmp(suffix,"mpd") == 0) {
        si->type=dash_manifest;
        printf("ELVTRC URL_SEG_INFO dash_manifest\n");
    }
    else {
        char *seg_start = strrchr(url, '-') + 1;
        char *seg_end = strchr(seg_start, '.');
        char seg[1000];
        int length = seg_end - seg_start;
        int offset = seg_start - url;
        printf("ELVRTC URL_SEG_INFO dash_segment: offset=%d length=%d\n", offset, length);
        int position = offset + 1;
        int c = 0;
        while ( c < length ) {
            seg[c] = url[position+c-1];
            c++;
        }
        seg[c] = '\0';
        printf("ELVTRC URL_SEG_INFO dash_segment: seg=%s\n", seg);
        if (strcmp(suffix,"m4v") == 0) {
            printf("ELVTRC URL_SEG_INFO dash_video: seg=%s\n", seg);
            // init segment or regular?
            if (strcmp(seg, "init") == 0) {
                si->type = dash_init_video;
            } else {
                si->type = dash_video;
                si->index=atoi(seg);
                st->curseg_a = si->index; // set current seg counter in global
            }
        }
        if (strcmp(suffix,"m4a") == 0) {
            // init segment or regular?
            printf("ELVTRC URL_SEG_INFO dash_audio: seg=%s\n", seg);
            if (strcmp(seg, "init") == 0) {
                si->type = dash_init_audio;
            } else {
                si->type = dash_audio;
                si->index=atoi(seg);
                st->curseg_a = si->index;
            }
        }
    }
    return;
}

void stats_update_seg_open(struct stats *st, const char* url)
{
    printf("ELVTRC NET OPEN url=%s\n", url);

    /* get the segment info - manifest, init, audio/video and update current seg index in global struct */
    const char* baseurl = basename(url);

    struct seginfo si_tmp;
    seginfo_init(&si_tmp);
    url_seg_info(st, &si_tmp, baseurl);
    printf("ELVTRC NET OPEN seginfo type=%d, index=%d", si_tmp.type, si_tmp.index);

    // TODO: Need to add audio segstats array
    long long open_usec = get_usec(st);

    struct segstats ss_tmp;
    segstats_init(&ss_tmp);
    ss_tmp.seg_open_usec = open_usec;
    ss_tmp.seginfo = si_tmp;

    if ( si_tmp.index - 1 == 0 ) {// first segment
        st->first_seg_open_usec = open_usec;
    }

    if ( si_tmp.type == dash_video ) {
        st->segstats_video[si_tmp.index-1] = ss_tmp;
    }  else if ( si_tmp.type == dash_manifest ) {
        st->segstats_manifest = ss_tmp;
        st->manifest_open_usec = open_usec;
    }  else if ( si_tmp.type == dash_init_video ) {
        st->segstats_video_init = ss_tmp;
        st->init_seg_video_open_usec = open_usec;
    }  else if ( si_tmp.type == dash_init_audio ) {
        st->segstats_audio_init = ss_tmp;
        st->init_seg_audio_open_usec = open_usec;
    }

}

void stats_update_seg_read(struct stats *st, const char* url, int off, int rsz)
{
    if (rsz > 0)
        printf("ELVTRC NET READ off=%d size=%d url=%s\n", off, rsz, url);

    /* get segment info - manifest, init, audio/video and update current seg index in global struct */
    const char* baseurl = basename(url);
    struct seginfo si_tmp;
    seginfo_init(&si_tmp);
    url_seg_info(st, &si_tmp, baseurl);

    if ( off == 0 )  { // First read of the segment

        long long ttfb = get_usec(st);
        printf("ELVTRC segment ttfb=%lld url=%s\n", ttfb, basename(url));

        if ( si_tmp.type == dash_video ) {
            st->segstats_video[si_tmp.index - 1].seg_first_buf_read_usec = ttfb;
        }
        else if ( si_tmp.type == dash_manifest )  {
            st->segstats_manifest.seg_first_buf_read_usec = ttfb;
        }
        else if ( si_tmp.type == dash_init_video )  {
            st->segstats_video_init.seg_first_buf_read_usec = ttfb;
        }
        else if ( si_tmp.type == dash_init_audio )  {
            st->segstats_video_init.seg_first_buf_read_usec = ttfb;
        }

        if ( si_tmp.index - 1 == 0 ) { // first segment, first buf read
            st->first_buf_read_usec = ttfb;
        }
    }

    if (rsz <= 0) // EOF
        printf("ELVTRC segment tt=%lld url=%s\n", get_usec(st), basename(url));
}

void stats_update_seg_close(struct stats *st, const char* url)
{
    printf("ELVTRC NET CLOSE url=%s\n", url);

    /* get segment info - manifest, init, audio/video and update current seg index in global struct */
    const char* baseurl = basename(url);
    struct seginfo si_tmp;
    seginfo_init(&si_tmp);
    url_seg_info(st, &si_tmp, baseurl);

    long long ttclose = get_usec(st);
    printf("ELVTRC segment close=%lld url=%s\n", ttclose, basename(url));


    if (si_tmp.type == dash_video)  {
        st->segstats_video[si_tmp.index - 1].seg_close_usec = ttclose;
    } else if (si_tmp.type == dash_manifest) {
        st->segstats_manifest.seg_close_usec = ttclose ;
    } else if ( si_tmp.type == dash_init_video )  {
        st->segstats_video_init.seg_close_usec = ttclose;
    } else if ( si_tmp.type == dash_init_audio )  {
        st->segstats_audio_init.seg_close_usec = ttclose;
    }

    printf("ELVTRC NET CLOSE exit.\n");

}

void stats_update_demux(struct stats *st, int type, int sos, int eos, float pts,
    int seg_index, double seg_start, double seg_end, double seg_dstart)
{
    printf("ELVTRACE DMUX PKT - enter\n");
    if  ( pts < 0 ) {
        return;
    }

    struct packetinfo pi_tmp;
    packetinfo_init(&pi_tmp);
    long long dmux_usec = get_usec(st);
    pi_tmp.type = type;
    pi_tmp.seg_index = seg_index;
    pi_tmp.start_of_segment = sos;
    pi_tmp.end_of_segment = eos;
    pi_tmp.pts = pts;
    pi_tmp.seg_start = seg_start;
    pi_tmp.seg_end = seg_end;

    struct packetstats* ps;
    if (pi_tmp.type == packet_video) {
        pi_tmp.packet_index = st->curpacket_v;
        if (st->curpacket_v == 0 && seg_index == 0) {
            printf("ELVTRC DEMUX PKT: first packet of first segment! %lld\n", dmux_usec);
            st->first_packet_decode_usec = dmux_usec;
        }
        st->curpacket_v += 1;
        ps = &(st->packetstats_video[pi_tmp.packet_index]);
    } else {
        pi_tmp.packet_index = st->curpacket_a;
        st->curpacket_a += 1;
        ps = &(st->packetstats_audio[pi_tmp.packet_index]);
    }
    ps->packet_dmux_usec = dmux_usec;
    ps->packetinfo = pi_tmp;

    printf("ELVTRC DEMUX PKT type=%d sos=%d eos=%d packet_index=%d pts=%f seg_index=%d seg_start=%f seg_end=%f seg_dstart=%f dmux_usec=%lld first=%lld\n",
        type, sos, eos, ps->packetinfo.packet_index, pts, seg_index, seg_start, seg_end, seg_dstart, dmux_usec, st->first_packet_decode_usec);

}


long long to_usec( double d ) {

    long long l = d * 1000000;
    return l;

}

void stats_finalize(struct stats *st)
{

    long long t0 = st->manifest_open_usec;
    long long t1 = st->first_seg_open_usec;
    long long t2 = st->first_packet_decode_usec;

    long long setup_time =
        st->init_seg_video_open_usec > st->init_seg_audio_open_usec ?  st->init_seg_video_open_usec : st->init_seg_audio_open_usec;
    setup_time = setup_time > st->manifest_open_usec ? setup_time : st->manifest_open_usec;

    int i = 0;

    long long manifest_ttfb = 0;
    long long video_init_ttfb = 0;
    long long audio_init_ttfb = 0;
    long long segment_ttfb = 0; // time of first read relative to this segment's request time
    long long manifest_tt = 0;
    long long video_init_tt = 0;
    long long audio_init_tt = 0;
    long long segment_tt = 0; // time of last read relative to this segment's request time

    manifest_ttfb = st->segstats_manifest.seg_first_buf_read_usec - st->segstats_manifest.seg_open_usec;
    video_init_ttfb =  st->segstats_video_init.seg_first_buf_read_usec - st->segstats_video_init.seg_open_usec;
    audio_init_ttfb = st->segstats_audio_init.seg_first_buf_read_usec - st->segstats_audio_init.seg_open_usec;

    manifest_tt = st->segstats_manifest.seg_close_usec - st->segstats_manifest.seg_open_usec;
    video_init_tt =  st->segstats_video_init.seg_close_usec - st->segstats_video_init.seg_open_usec;
    audio_init_tt = st->segstats_audio_init.seg_close_usec - st->segstats_audio_init.seg_open_usec;

    printf("manifest ttfb=%lld tt=%lld video_init ttfb=%lld tt=%lld audio_init_ttfb=%lld audio_init_tt=%lld\n", manifest_ttfb, manifest_tt, video_init_ttfb, video_init_tt, audio_init_ttfb, audio_init_tt);

    // SEGMENTS
    long long play_time = 0;
    long long ahead_of_play_time = 0;
    long long latency = 0;

    for ( i = 0; i < st->curseg_v; i++ ) {

        segment_ttfb = st->segstats_video[i].seg_first_buf_read_usec - st->segstats_video[0].seg_open_usec;
        segment_tt =  st->segstats_video[i].seg_close_usec - st->segstats_video[0].seg_open_usec;
        play_time = t1 + st->seg_duration * i;
        ahead_of_play_time = play_time - st->segstats_video[i].seg_close_usec;
        latency = st->segstats_video[i].seg_close_usec - t0; 
        printf("ELVTRC SEG %d time_to_first_buf=%lld total_time=%lld expected_play_time=%lld ahead_time=%lld latency=%lld\n", i, segment_ttfb, segment_tt, play_time, ahead_of_play_time, latency);

    }

    // FRAMES ("PACKETS")
    int j = 0;

    play_time = 0;
    ahead_of_play_time = 0;
    int this_segment_index = 0;

    long long decode_time = 0;
    double this_pts = 0;
    latency = 0;


    for ( j = 0; j < st->curpacket_v; j++ ) {

        if ( st->packetstats_video[j].packetinfo.end_of_segment == 1 ) {
            continue;
        }

        this_segment_index = st->packetstats_video[j].packetinfo.seg_index;

        this_pts = st->packetstats_video[j].packetinfo.seg_start + st->packetstats_video[j].packetinfo.pts;

        // printf("MMSEG this_pts %f %lld\n", this_pts, to_usec(this_pts));

        play_time =  t2 + to_usec(this_pts);

        // printf("MMSEG play_time %lld = %lld + %lld\n", play_time, t2, to_usec(this_pts));

        ahead_of_play_time = play_time - st->packetstats_video[j].packet_dmux_usec;

        if ( j > 0 ) {
            decode_time = st->packetstats_video[j].packet_dmux_usec - st->packetstats_video[j-1].packet_dmux_usec;
        } else {
            decode_time =  st->packetstats_video[0].packet_dmux_usec - t2;
        }

        latency = st->packetstats_video[j].packet_dmux_usec - t0;

        printf("ELVTRC FR %d seg=%d play_time=%lld ahead_time=%lld decode_time=%lld latency=%lld\n", j, this_segment_index, play_time, ahead_of_play_time, decode_time, latency);

    }

}


