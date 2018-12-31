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

    st->curseg_a = st->curseg_v = -1;

    st->manifest_open_usec = 0;
    st->init_seg_open_usec = 0;
}

// MM Todo - Why is this never called?
void segstats_init(struct segstats *ss)
{
    ss->seg_open_usec = 0;
    ss->seg_first_packet_read_usec = 0;
    ss->seg_close_usec = 0;
    seginfo_init(&ss->seginfo);
}

void seginfo_init(struct seginfo *sinf)
{
    sinf->type = dash_unset; // manifest, init-video, init-audio, video, audio
    sinf->index = -1; // if vidio or audio, the segment number
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
            if (strcmp(seg,"init") == 0) {
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

    // TODO: Need to add audio segstats array and init seg open time
    long long open_usec = get_usec(st);
    if ( si_tmp.type == dash_video ) {
            struct segstats* ss;
            ss = &(st->segstats_video[si_tmp.index-1]);
            segstats_init(ss);
            ss->seg_open_usec = open_usec;
            ss->seginfo = si_tmp;
    } else if ( si_tmp.type == dash_manifest ) {
        st->manifest_open_usec = open_usec;
    } else if ( si_tmp.type == dash_init_video ) {
        st->init_seg_open_usec = open_usec;
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

    // Todo - Add audio
    if ( (off == 0 ) && ( si_tmp.type == dash_video ) ) { // First read of the segment
        long long ttfb = get_usec(st);
        printf("ELVTRC segment ttfb=%lld url=%s\n", ttfb, basename(url));
        // get the segstat from the segsarray
        struct segstats* ss;
        ss = &(st->segstats_video[si_tmp.index - 1]);
        assert ((ss->seginfo.type == dash_video));
        // set the time of the first packet
        ss->seg_first_packet_read_usec = ttfb;
        ss->seginfo = si_tmp;
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

    // Todo - Add audio
    if (si_tmp.type == dash_video)  { 
        struct segstats* ss;
        ss = &(st->segstats_video[si_tmp.index - 1]);
        // set the segment close time
        ss->seg_close_usec = ttclose;
        ss->seginfo = si_tmp;
    }
    printf("ELVTRC NET CLOSE exit.\n");

}

void stats_update_demux(struct stats *st, int type, int sos, int eos, float pts,
    int seg_index, double seg_start, double seg_end, double seg_dstart)
{
    printf("ELVTRACE DMUX PKT - enter\n");
    printf("ELVTRC DEMUX PKT type=%d sos=%d eos=%d pts=%f seg_index=%d seg_start=%f seg_end=%f seg_dstart=%f\n",
        type, sos, eos, pts,
        seg_index, seg_start, seg_end, seg_dstart);
}
