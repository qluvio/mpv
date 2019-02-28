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
static long long get_usec_now(void)
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

static const char *urlbase(const char *f)
{
    char *p = strrchr(f, '/');
    if (p) {
      return p + 1;
    }
    else
        return f;
}

void stats_init(struct stats *st, struct MPContext *mpctx)
{
    // Using a global stats singleton
    gst = &_gst;
    st = gst;

    st->log = mpctx->log;
    st->start_usec = get_usec_now();
    st->seg_duration = 2002000; // MM Todo - hard coded for now
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
    mp_msg(st->log, MSGL_INFO, "ELVTRC URL_SEG_INFO suffix=%s\n", suffix);
    if (strcmp(suffix,"mpd") == 0 || strcmp(suffix,"m3u8") == 0) {
        si->type=dash_manifest;
        mp_msg(st->log, MSGL_INFO, "ELVTRC URL_SEG_INFO dash_manifest\n");
    }
    else {
        char *seg_start = strrchr(url, '/') + 1;
        char *seg_end = strchr(seg_start, '.');
        char seg[1000];
        int length = seg_end - seg_start;

	memcpy(seg, seg_start, length);
        seg[length] = '\0';
        mp_msg(st->log, MSGL_INFO, "ELVTRC URL_SEG_INFO dash_segment: seg=%s\n", seg);
        if (strstr(url,"video")) {
	  mp_msg(st->log, MSGL_INFO, "ELVTRC URL_SEG_INFO dash_video: seg=%s\n", seg);
            // init segment or regular?
            if (strcmp(seg, "init") == 0) {
                si->type = dash_init_video;
            } else {
                si->type = dash_video;
                si->index=atoi(seg);
                st->curseg_a = si->index; // set current seg counter in global
            }
        }
        if (strstr(url,"audio")) {
            // init segment or regular?
	  mp_msg(st->log, MSGL_INFO, "ELVTRC URL_SEG_INFO dash_audio: seg=%s\n", seg);
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
    mp_msg(st->log, MSGL_INFO, "ELVTRC NET OPEN url=%s\n", url);

    /* get the segment info - manifest, init, audio/video and update current seg index in global struct */
    struct seginfo si_tmp;
    seginfo_init(&si_tmp);
    url_seg_info(st, &si_tmp, url);
    mp_msg(st->log, MSGL_INFO, "ELVTRC NET OPEN seginfo type=%d, index=%d\n", si_tmp.type, si_tmp.index);

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
        /* Calculate player overhead when opening init segment */
        st->player_overhead = open_usec - st->segstats_manifest.seg_close_usec;
        mp_msg(st->log, MSGL_INFO, "ELVTRC PLAYER OVERHEAD %lld\n", st->player_overhead);
    }  else if ( si_tmp.type == dash_init_audio ) {
        st->segstats_audio_init = ss_tmp;
        st->init_seg_audio_open_usec = open_usec;
    }

}

void stats_update_seg_read(struct stats *st, const char* url, int off, int rsz)
{
    if (rsz > 0)
      mp_msg(st->log, MSGL_INFO, "ELVTRC NET READ off=%d size=%d url=%s\n", off, rsz, url);

    /* get segment info - manifest, init, audio/video and update current seg index in global struct */
    struct seginfo si_tmp;
    seginfo_init(&si_tmp);
    url_seg_info(st, &si_tmp, url);

    if ( off == 0 )  { // First read of the segment

        long long ttfb = get_usec(st);
        mp_msg(st->log, MSGL_INFO, "ELVTRC segment ttfb=%lld url=%s\n", ttfb, urlbase(url));

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
            st->segstats_audio_init.seg_first_buf_read_usec = ttfb;
        }

        if ( si_tmp.index - 1 == 0 ) { // first segment, first buf read
            st->first_buf_read_usec = ttfb;
        }
    }

    if (rsz <= 0) // EOF
        mp_msg(st->log, MSGL_INFO, "ELVTRC segment tt=%lld url=%s\n", get_usec(st), urlbase(url));
}

void stats_update_seg_close(struct stats *st, const char* url)
{
    mp_msg(st->log, MSGL_INFO, "ELVTRC NET CLOSE url=%s\n", url);

    /* get segment info - manifest, init, audio/video and update current seg index in global struct */
    struct seginfo si_tmp;
    seginfo_init(&si_tmp);
    url_seg_info(st, &si_tmp, url);

    long long ttclose = get_usec(st);
    mp_msg(st->log, MSGL_INFO, "ELVTRC segment close=%lld url=%s\n", ttclose, urlbase(url));

    if (si_tmp.type == dash_video)  {
        st->segstats_video[si_tmp.index - 1].seg_close_usec = ttclose;
    } else if (si_tmp.type == dash_manifest) {
        st->segstats_manifest.seg_close_usec = ttclose ;
    } else if ( si_tmp.type == dash_init_video )  {
        st->segstats_video_init.seg_close_usec = ttclose;
    } else if ( si_tmp.type == dash_init_audio )  {
        st->segstats_audio_init.seg_close_usec = ttclose;
    }

    mp_msg(st->log, MSGL_INFO, "ELVTRC NET CLOSE exit.\n");

}

void stats_update_demux(struct stats *st, int type, int sos, int eos, float pts,
    int seg_index, double seg_start, double seg_end, double seg_dstart)
{
    if  ( pts < 0 ) {
        return;
    }

    struct packetinfo pi_tmp;
    packetinfo_init(&pi_tmp);
    long long dmux_usec = get_usec(st);
    if ( type == STREAM_VIDEO ) {
      pi_tmp.type = packet_video;
    } else if ( type == STREAM_AUDIO ) {
      pi_tmp.type = packet_audio;
    }
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
	  mp_msg(st->log, MSGL_INFO, "ELVTRC DEMUX PKT: first packet of first segment! %lld\n", dmux_usec);
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

    mp_msg(st->log, MSGL_INFO, "ELVTRC DEMUX PKT type=%d sos=%d eos=%d packet_index=%d pts=%f seg_index=%d seg_start=%f seg_end=%f seg_dstart=%f dmux_usec=%lld first=%lld\n",
        type, sos, eos, ps->packetinfo.packet_index, pts, seg_index, seg_start, seg_end, seg_dstart, dmux_usec, st->first_packet_decode_usec);

}

long long to_usec( double d ) {

    long long l = d * 1000000;
    return l;

}

static void legend(struct stats *st) {
    mp_msg(st->log, MSGL_INFO, "ELVSTATS LEGEND\n");

    mp_msg(st->log, MSGL_INFO, "- manifest ttfb and tt:     relative to manifest request\n");
    mp_msg(st->log, MSGL_INFO, "- init segment ttfb and tt: relative to own request\n");
    mp_msg(st->log, MSGL_INFO, "- segment ttfb and tt rel:  relative to own segment request time\n");
    mp_msg(st->log, MSGL_INFO, "- segment ttfb and tt:      relative to first segment request time\n");
    mp_msg(st->log, MSGL_INFO, "- exp_play_time:     relative to program start\n");
    mp_msg(st->log, MSGL_INFO, "- exp_play_time_adj: adjusted for player overhead\n");
    mp_msg(st->log, MSGL_INFO, "- ahead_time:        time ready before expected play time (or after if negative)\n");
    mp_msg(st->log, MSGL_INFO, "- latency:           arrival time since beginning of program adjusted for overhead\n");
}

void stats_finalize(struct stats *st)
{
    legend(st);

    long long t0 = st->manifest_open_usec;
    long long t1 = st->first_seg_open_usec;
    long long t2 = st->first_packet_decode_usec;

    long long t0adj = t0 + st->player_overhead; // t0 adjusted for player overhead

    int i = 0;

    long long manifest_ttfb = 0;
    long long video_init_ttfb = 0;
    long long audio_init_ttfb = 0;
    long long segment_ttfb = 0; // time of first read relative to first segment's request time
    long long segment_ttfb_rel = 0; // relative to this segment's request time
    long long manifest_tt = 0;
    long long video_init_tt = 0;
    long long audio_init_tt = 0;
    long long segment_tt = 0; // time of last read relative to this segment's request time
    long long segment_tt_rel = 0; // relative to this segment's request time

    manifest_ttfb = st->segstats_manifest.seg_first_buf_read_usec - st->segstats_manifest.seg_open_usec;
    video_init_ttfb =  st->segstats_video_init.seg_first_buf_read_usec - st->segstats_video_init.seg_open_usec;
    audio_init_ttfb = st->segstats_audio_init.seg_first_buf_read_usec - st->segstats_audio_init.seg_open_usec;

    manifest_tt = st->segstats_manifest.seg_close_usec - st->segstats_manifest.seg_open_usec;
    video_init_tt =  st->segstats_video_init.seg_close_usec - st->segstats_video_init.seg_open_usec;
    audio_init_tt = st->segstats_audio_init.seg_close_usec - st->segstats_audio_init.seg_open_usec;

    mp_msg(st->log, MSGL_INFO,
        "ELVSTATS MANIFEST ttfb=%lld tt=%lld video_init ttfb=%lld tt=%lld audio_init_ttfb=%lld audio_init_tt=%lld\n",
        manifest_ttfb, manifest_tt, video_init_ttfb, video_init_tt, audio_init_ttfb, audio_init_tt);

    long long play_time = 0; // time when the frame is supposed to be played
                             // for segment is the time the segment is supposed to be received
    long long play_time_adj = 0; // adjusted for player overhead
    long long ahead_of_play_time = 0; // can be negative if not keeping up
    long long latency = 0;

    // SEGMENTS
    for ( i = 0; i < st->curseg_v; i++ ) {

        segment_ttfb = st->segstats_video[i].seg_first_buf_read_usec - st->segstats_video[0].seg_open_usec;
        segment_tt =  st->segstats_video[i].seg_close_usec - st->segstats_video[0].seg_open_usec;
        segment_ttfb_rel = st->segstats_video[i].seg_first_buf_read_usec - st->segstats_video[i].seg_open_usec;
        segment_tt_rel =  st->segstats_video[i].seg_close_usec - st->segstats_video[i].seg_open_usec;
        play_time = t1 + st->seg_duration * i;
        play_time_adj = play_time - st->player_overhead; // adjusted for player overhead
        ahead_of_play_time = play_time - st->segstats_video[i].seg_close_usec;
        latency = st->segstats_video[i].seg_close_usec - t0adj;
        mp_msg(st->log, MSGL_INFO,
            "ELVSTATS SEG %d ttfb=%lld tt=%lld ttfb_rel=%lld tt_rel=%lld "
            "exp_play_time=%lld exp_play_time_adj=%lld ahead_time=%lld latency=%lld\n",
            i, segment_ttfb, segment_tt, segment_ttfb_rel, segment_tt_rel,
            play_time, play_time_adj, ahead_of_play_time, latency);
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

        // If pts starts at 0 every segment we need to add the segment start pts.
        // Initial tests had the pts starting at 0 but testing with avpipe has pts continuing from prev segment
        // this_pts = st->packetstats_video[j].packetinfo.seg_start + st->packetstats_video[j].packetinfo.pts;
        this_pts = st->packetstats_video[j].packetinfo.pts;

        // printf("MMSEG this_pts %f %lld\n", this_pts, to_usec(this_pts));

        play_time = t2 + to_usec(this_pts);

        // printf("MMSEG play_time %lld = %lld + %lld\n", play_time, t2, to_usec(this_pts));

        ahead_of_play_time = play_time - st->packetstats_video[j].packet_dmux_usec;

        if ( j > 0 ) {
            decode_time = st->packetstats_video[j].packet_dmux_usec - st->packetstats_video[j-1].packet_dmux_usec;
        } else {
            decode_time =  st->packetstats_video[0].packet_dmux_usec - t2;
        }

        latency = st->packetstats_video[j].packet_dmux_usec - t0adj;

        mp_msg(st->log, MSGL_INFO,
            "ELVSTATS FRAME %d seg=%d play_time=%lld ahead_time=%lld decode_time=%lld latency=%lld\n",
            j, this_segment_index, play_time, ahead_of_play_time, decode_time, latency);
    }
}
