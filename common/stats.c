/*
 *
 */

#include "common/common.h"
#include "common/stats.h"

#include <sys/time.h>

// Global stats structure
struct stats _gst;
struct stats *gst;

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
    if (p)
        return p;
    else
        return f;
}

void stats_init(struct stats *st)
{
    // Using a global stats sigleton
    gst = &_gst;
    st = gst;

    st->start_usec = get_usec_now();

    st->curseg_a = st->curseg_v = -1;
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

void stats_update_seg_open(struct stats *st, const char* url)
{
    printf("ELVTRC NET OPEN url=%s\n", url);
}

void stats_update_seg_read(struct stats *st, const char* url, int off, int rsz)
{
    if (rsz > 0)
        printf("ELVTRC NET READ off=%d size=%d url=%s\n", off, rsz, url);

    if (off == 0) // First read of the segment
        printf("ELVTRC segment ttfb=%lld url=%s\n", get_usec(st), basename(url));
    if (rsz <= 0) // EOF
        printf("ELVTRC segment tt=%lld url=%s\n", get_usec(st), basename(url));
}

void stats_update_seg_close(struct stats *st, const char* url)
{
    printf("ELVTRC NET CLOSE url=%s\n", url);
}

void stats_update_demux(struct stats *st, int type, int sos, int eos, float pts,
    int seg_index, double seg_start, double seg_end, double seg_dstart)
{
    printf("ELVTRC DEMUX PKT type=%d sos=%d eos=%d pts=%f seg_index=%d seg_start=%f seg_end=%f seg_dstart=%f\n",
        type, sos, eos, pts,
        seg_index, seg_start, seg_end, seg_dstart);
}
