#pragma once

/*
 * FFmpeg API compatibility for Generals Linux media code.
 * Supports FFmpeg 4.4+ (libavcodec 58) through current 6.x/7.x.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/version.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 37, 100)
#include <libavutil/channel_layout.h>
#endif

#ifdef __cplusplus
}
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
#define FFMPEG_USE_CH_LAYOUT 1
#else
#define FFMPEG_USE_CH_LAYOUT 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline int ffmpeg_codec_channels(const AVCodecContext *codec)
{
	if (codec == NULL) {
		return 0;
	}
#if FFMPEG_USE_CH_LAYOUT
	return codec->ch_layout.nb_channels;
#else
	return codec->channels;
#endif
}

static inline int ffmpeg_swr_alloc_for_codec(
	SwrContext **pswr,
	const AVCodecContext *codec,
	int out_channels,
	int out_sample_rate)
{
	SwrContext *swr = NULL;
	int ret;

	if (pswr == NULL || codec == NULL || out_channels <= 0 || out_sample_rate <= 0) {
		return AVERROR(EINVAL);
	}

	*pswr = NULL;

#if FFMPEG_USE_CH_LAYOUT
	AVChannelLayout out_layout;

	swr = swr_alloc();
	if (swr == NULL) {
		return AVERROR(ENOMEM);
	}

	av_channel_layout_default(&out_layout, out_channels);
	av_opt_set_chlayout(swr, "in_chlayout", &codec->ch_layout, 0);
	av_opt_set_chlayout(swr, "out_chlayout", &out_layout, 0);
	av_opt_set_int(swr, "in_sample_rate", codec->sample_rate, 0);
	av_opt_set_int(swr, "out_sample_rate", out_sample_rate, 0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt", codec->sample_fmt, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
#else
	{
		int64_t in_layout = codec->channel_layout;
		int64_t out_layout;

		if (in_layout == 0 && codec->channels > 0) {
			in_layout = av_get_default_channel_layout(codec->channels);
		}
		out_layout = av_get_default_channel_layout(out_channels);
		swr = swr_alloc_set_opts(
			NULL,
			out_layout,
			AV_SAMPLE_FMT_S16,
			out_sample_rate,
			in_layout,
			codec->sample_fmt,
			codec->sample_rate,
			0,
			NULL);
	}
	if (swr == NULL) {
		return AVERROR(ENOMEM);
	}
#endif

	ret = swr_init(swr);
	if (ret < 0) {
		swr_free(&swr);
		return ret;
	}

	*pswr = swr;
	return 0;
}

static inline int ffmpeg_swr_alloc_for_codec_mono_stereo(
	SwrContext **pswr,
	const AVCodecContext *codec,
	int out_sample_rate)
{
	const int in_ch = ffmpeg_codec_channels(codec);
	const int out_ch = (in_ch == 1) ? 1 : 2;
	return ffmpeg_swr_alloc_for_codec(pswr, codec, out_ch, out_sample_rate);
}

#ifdef __cplusplus
}
#endif
