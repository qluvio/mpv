#pragma once

/*
 * Player stats
 *
 */

/* stats for an individual segment */

typedef enum {

    dash_unset = -1,
    dash_manifest = 1,
    dash_init_video,
    dash_init_audio,
    dash_video,
    dash_audio

} dash_type_e;

struct seginfo {

    dash_type_e type;
    int index;
};


struct segstats {

    // open time, first packet read time, close time
    long long seg_open_usec;
    long long seg_first_packet_read_usec;
    long long seg_close_usec;
    struct seginfo seginfo;
};


struct stats {

    int curseg_v;
    int curseg_a;

    long long start_usec; // initialization time for structure

    struct segstats segstats_video[1000]; // array of stats for individual video segments
    struct segstats segstats_audio[1000]; // audio segments
    long long manifest_open_usec;
    long long init_seg_open_usec;
    // Todo: add init open usec
};

extern struct stats *gst;

void stats_init(struct stats *st);

void segstats_init(struct segstats *ss);

void seginfo_init(struct seginfo *si);

void url_seg_info(struct stats *st, struct seginfo *si, const char* url);

int stats_curseg(struct stats *st, int type);

void stats_curseg_set(struct stats *st, int type, int index);

void stats_update_seg_open(struct stats *st, const char* url);
void stats_update_seg_read(struct stats *st, const char* url, int off, int rsz);
void stats_update_seg_close(struct stats *st, const char* url);

void stats_update_demux(struct stats *st, int type, int sos, int eos, float pts,
    int seg_index, double seg_start, double seg_end, double seg_dstart);
