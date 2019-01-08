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

typedef enum {

    packet_unset = -1,
    packet_video = 1,
    packet_audio

} packet_type_e;

struct seginfo {

    dash_type_e type;
    int index;
};

struct packetinfo {

    packet_type_e type;
    int seg_index;
    int packet_index;
    int start_of_segment;
    int end_of_segment;
    float pts;
    double seg_start;
    double seg_end;

};

struct segstats {

    // open time, first packet read time, close time
    long long seg_open_usec; // MM: Todo - need to add audio; these are all for video
    long long seg_first_buf_read_usec;
    long long seg_close_usec;
    struct seginfo seginfo;
};

struct packetstats {

    long long packet_dmux_usec;
    // start of play is pts 0
    struct packetinfo packetinfo;

};

struct stats {

    int curseg_v;
    int curseg_a;
    int curpacket_v;
    int curpacket_a;

    long long start_usec; // initialization time for structure

    long long seg_duration; // Todo - need to extract
    struct segstats segstats_manifest;
    struct segstats segstats_video_init;
    struct segstats segstats_audio_init;
    struct segstats segstats_video[1000]; // array of stats for individual video segments
    struct segstats segstats_audio[1000]; // audio segments
    long long manifest_open_usec; // this is t0
    long long init_seg_video_open_usec;
    long long init_seg_audio_open_usec;
    long long first_seg_open_usec; // this becomes t1
    long long first_buf_read_usec;
    long long first_packet_decode_usec; // this becomes t2
    struct packetstats packetstats_video[1000 * 1000]; // packets are actually "frames" in video; nothing to do with network
    struct packetstats packetstats_audio[1000 * 1000];
};

extern struct stats *gst;

void stats_init(struct stats *st);

void segstats_init(struct segstats *ss);

void seginfo_init(struct seginfo *si);

void packetinfo_init(struct packetinfo *pi);

void url_seg_info(struct stats *st, struct seginfo *si, const char* url);

int stats_curseg(struct stats *st, int type);

void stats_curseg_set(struct stats *st, int type, int index);

void stats_update_seg_open(struct stats *st, const char* url);
void stats_update_seg_read(struct stats *st, const char* url, int off, int rsz);
void stats_update_seg_close(struct stats *st, const char* url);

void stats_update_demux(struct stats *st, int type, int sos, int eos, float pts,
    int seg_index, double seg_start, double seg_end, double seg_dstart);

void stats_finalize(struct stats *st);

long long to_usec(double d);
