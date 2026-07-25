#pragma once

/*
 * FFmpeg API compatibility for Generals Linux media code.
 * Uses deprecated-but-stable fields/APIs (channels, swr_alloc_set_opts) so a
 * binary built on FFmpeg 4.4+ runs against newer distro libavcodec.so.N.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline int ffmpeg_codec_channels(const AVCodecContext *codec)
{
	if (codec == NULL) {
		return 0;
	}
	return codec->channels;
}

static inline int ffmpeg_swr_alloc_for_codec(
	SwrContext **pswr,
	const AVCodecContext *codec,
	int out_channels,
	int out_sample_rate)
{
	SwrContext *swr = NULL;
	int64_t in_layout;
	int64_t out_layout;
	int ret;

	if (pswr == NULL || codec == NULL || out_channels <= 0 || out_sample_rate <= 0) {
		return AVERROR(EINVAL);
	}

	*pswr = NULL;
	in_layout = codec->channel_layout;
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
	if (swr == NULL) {
		return AVERROR(ENOMEM);
	}

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
