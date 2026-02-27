#include "config.h"
#if defined(HAVE_FFMPEG) && defined(HAVE_CAIRO)

#include "output/out_mp4.h"
#include "output/layout.h"
#include "log.h"

#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define VID_W 1920
#define VID_H 1080
#define FPS   30
#define DURATION_S 10
#define TOTAL_FRAMES (FPS * DURATION_S)

static void color_for_type(ri_host_type_t t, double *r, double *g, double *b)
{
    switch (t) {
    case RI_HOST_LOCAL:   *r=0.2; *g=0.8; *b=0.2; break;
    case RI_HOST_GATEWAY: *r=1.0; *g=0.8; *b=0.0; break;
    case RI_HOST_LAN:     *r=0.3; *g=0.7; *b=1.0; break;
    case RI_HOST_REMOTE:  *r=0.8; *g=0.3; *b=0.8; break;
    case RI_HOST_TARGET:  *r=1.0; *g=0.2; *b=0.2; break;
    }
}

/* Project 3D point to 2D with rotation */
static void project(double x, double y, double z,
                    double angle, double cx, double cy,
                    double scale, double *px, double *py)
{
    double rx = x * cos(angle) - z * sin(angle);
    double rz = x * sin(angle) + z * cos(angle);
    double perspective = 1.0 / (1.0 + rz * 0.001);
    *px = cx + rx * scale * perspective;
    *py = cy + y * scale * perspective;
}

static void render_frame(cairo_t *cr, const ri_graph_t *g,
                         double angle, int frame)
{
    /* Dark background */
    cairo_set_source_rgb(cr, 0.05, 0.05, 0.1);
    cairo_paint(cr);

    double cx = VID_W / 2.0;
    double cy = VID_H / 2.0;
    double scale = 2.0;

    /* Draw edges */
    for (int i = 0; i < g->edge_count; i++) {
        ri_edge_t *e = &g->edges[i];
        ri_host_t *s = &g->hosts[e->src_id];
        ri_host_t *d = &g->hosts[e->dst_id];

        double sx, sy, ex, ey;
        project(s->x, s->y, s->z, angle, cx, cy, scale, &sx, &sy);
        project(d->x, d->y, d->z, angle, cx, cy, scale, &ex, &ey);

        if (e->in_mst)
            cairo_set_source_rgba(cr, 1, 1, 1, 0.5);
        else
            cairo_set_source_rgba(cr, 0.4, 0.4, 0.4, 0.2);
        cairo_set_line_width(cr, e->in_mst ? 1.5 : 0.5);
        cairo_move_to(cr, sx, sy);
        cairo_line_to(cr, ex, ey);
        cairo_stroke(cr);
    }

    /* Draw nodes */
    for (int i = 0; i < g->host_count; i++) {
        ri_host_t *h = &g->hosts[i];
        double px, py;
        project(h->x, h->y, h->z, angle, cx, cy, scale, &px, &py);

        double r, gr, b;
        color_for_type(h->type, &r, &gr, &b);

        cairo_set_source_rgb(cr, r, gr, b);
        cairo_arc(cr, px, py, 6, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    /* Frame counter */
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_font_size(cr, 14);
    char buf[64];
    snprintf(buf, sizeof(buf), "Frame %d/%d", frame + 1, TOTAL_FRAMES);
    cairo_move_to(cr, 20, VID_H - 20);
    cairo_show_text(cr, buf);
}

int ri_out_mp4(const ri_graph_t *g, const char *filename)
{
    /* Set up FFmpeg output */
    AVFormatContext *fmt_ctx = NULL;
    avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, filename);
    if (!fmt_ctx) {
        LOG_ERROR("Could not create output context");
        return -1;
    }

    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        LOG_ERROR("H.264 codec not found");
        avformat_free_context(fmt_ctx);
        return -1;
    }

    AVStream *stream = avformat_new_stream(fmt_ctx, NULL);
    AVCodecContext *enc = avcodec_alloc_context3(codec);

    enc->codec_id = AV_CODEC_ID_H264;
    enc->width = VID_W;
    enc->height = VID_H;
    enc->time_base = (AVRational){1, FPS};
    enc->framerate = (AVRational){FPS, 1};
    enc->pix_fmt = AV_PIX_FMT_YUV420P;
    enc->gop_size = 12;
    enc->max_b_frames = 2;
    av_opt_set(enc->priv_data, "preset", "medium", 0);

    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(enc, codec, NULL) < 0) {
        LOG_ERROR("Could not open codec");
        avcodec_free_context(&enc);
        avformat_free_context(fmt_ctx);
        return -1;
    }

    avcodec_parameters_from_context(stream->codecpar, enc);
    stream->time_base = enc->time_base;

    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) {
            LOG_ERROR("Could not open %s", filename);
            avcodec_free_context(&enc);
            avformat_free_context(fmt_ctx);
            return -1;
        }
    }

    if (avformat_write_header(fmt_ctx, NULL) < 0) {
        LOG_ERROR("Could not write header");
        avcodec_free_context(&enc);
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
        return -1;
    }

    /* Allocate frames */
    AVFrame *frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = VID_W;
    frame->height = VID_H;
    av_frame_get_buffer(frame, 0);

    /* swscale for ARGB -> YUV420P */
    struct SwsContext *sws = sws_getContext(
        VID_W, VID_H, AV_PIX_FMT_ARGB,
        VID_W, VID_H, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, NULL, NULL, NULL);

    AVPacket *pkt = av_packet_alloc();

    /* Cairo surface for rendering */
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, VID_W, VID_H);
    cairo_t *cr = cairo_create(surface);

    LOG_INFO("Encoding %d frames...", TOTAL_FRAMES);

    for (int f = 0; f < TOTAL_FRAMES; f++) {
        double angle = 2.0 * M_PI * f / TOTAL_FRAMES;
        render_frame(cr, g, angle, f);
        cairo_surface_flush(surface);

        /* Convert Cairo ARGB to YUV420P */
        const uint8_t *src_data[1] = {
            cairo_image_surface_get_data(surface)
        };
        int src_stride[1] = {
            cairo_image_surface_get_stride(surface)
        };

        av_frame_make_writable(frame);
        sws_scale(sws, src_data, src_stride, 0, VID_H,
                  frame->data, frame->linesize);
        frame->pts = f;

        /* Encode frame */
        avcodec_send_frame(enc, frame);
        while (avcodec_receive_packet(enc, pkt) == 0) {
            av_packet_rescale_ts(pkt, enc->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_unref(pkt);
        }
    }

    /* Flush encoder */
    avcodec_send_frame(enc, NULL);
    while (avcodec_receive_packet(enc, pkt) == 0) {
        av_packet_rescale_ts(pkt, enc->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        av_interleaved_write_frame(fmt_ctx, pkt);
        av_packet_unref(pkt);
    }

    av_write_trailer(fmt_ctx);

    /* Cleanup */
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    sws_freeContext(sws);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&enc);
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&fmt_ctx->pb);
    avformat_free_context(fmt_ctx);

    LOG_INFO("Wrote MP4 to %s", filename);
    return 0;
}

#endif /* HAVE_FFMPEG && HAVE_CAIRO */
