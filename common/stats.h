#pragma once

/*
 * Player stats
 *
 */

struct stats {

    int curseg_v;
    int curseg_a;

    long long start_usec;
};

extern struct stats *gst;

void stats_init(struct stats *st);

int stats_curseg(struct stats *st, int type);

void stats_curseg_set(struct stats *st, int type, int index);

void stats_update_seg_open(struct stats *st, const char* url);
void stats_update_seg_read(struct stats *st, const char* url, int off, int rsz);
void stats_update_seg_close(struct stats *st, const char* url);

void stats_update_demux(struct stats *st, int type, int sos, int eos, float pts,
    int seg_index, double seg_start, double seg_end, double seg_dstart);
