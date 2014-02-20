#include <time.h>
#include"jni_utils.h"
#include "video_recorder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
}

#define LOG_TAG "VideoRecorder"

// ffmpeg calls this back, used for log
static void ffmpeg_log_callback (void* ptr, int level, const char* fmt, va_list vl) {

	char s[1024];
	vsnprintf(s, 1024, fmt, vl);

	switch (level) {

		case AV_LOG_PANIC:
		case AV_LOG_FATAL:
		case AV_LOG_ERROR:
		case AV_LOG_WARNING:
		LOGE("%s \n", s);
		break;

		case AV_LOG_INFO:
		LOGI("%s \n", s);
		break;

		case AV_LOG_DEBUG:
		//LOGD("%s \n", s);
		break;
	}
}

VideoRecorder::VideoRecorder()
{
	// audio related vars
	audio_st = NULL;
	audio_frame = NULL;
	audio_pkt_buf = NULL;
	audio_pkt_buf_size = 0;

	samples = NULL;
	dst_samples = NULL;
	dst_samples_size = 0;
	max_dst_nb_samples = 0;
	samples_count = 0;
	audio_input_frame_size = 0;
	audio_input_leftover_samples = 0;

	swr_ctx = NULL;

	// video related vars
	video_st = NULL;
	video_frame = NULL;
	video_pkt_buf = NULL;
	video_pkt_buf_size = 0;
	timestamp_start = 0;

	// common
	oc = NULL;

	av_log_set_callback(ffmpeg_log_callback);
}

VideoRecorder::~VideoRecorder()
{

}

int VideoRecorder::open(const char *file, bool hasAudio)
{
	LOGI("opening recorder \n");

	av_register_all();
	avcodec_register_all();

	int ret = avformat_alloc_output_context2(&oc, NULL, NULL, file);
	if (!oc) {
		LOGE("alloc_output_context failed \n");
		return ret;
	}

	video_st = add_video_stream(AV_CODEC_ID_HEVC);
	if (hasAudio) {
		audio_st = add_audio_stream(AV_CODEC_ID_AAC);
	}

	// for debug
	av_dump_format(oc, 0, file, 1);

	ret = open_video();
	if (ret < 0) {
		LOGE("open video failed \n");
		return ret;
	}
	if (hasAudio) {
		ret = open_audio();
		if (ret < 0) {
			LOGE("open audio failed \n");
			return ret;
		}
	}

	ret = avio_open(&oc->pb, file, AVIO_FLAG_WRITE);
	if (ret < 0) {
		LOGE("open file failed: %s \n", file);
		return ret;
	}

	ret = avformat_write_header(oc, NULL);
	if (ret < 0) {
		LOGE("write format header failed \n");
		return ret;
	}

	pthread_mutex_init(&write_mutex, NULL);

	LOGI("recorder open success \n");
	return 0;
}

AVStream *VideoRecorder::add_audio_stream(enum AVCodecID codec_id)
{
    AVCodec *codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		LOGE("find audio encoder failed \n");
		return NULL;
	}
	AVStream *st = avformat_new_stream(oc, codec);
	if (!st) {
		LOGE("new audio stream failed \n");
		return NULL;
	}

	AVCodecContext *c = st->codec;
	c->sample_fmt = audio_sample_format;
	c->bit_rate = audio_bit_rate;
	c->sample_rate = audio_sample_rate;
	c->channels = audio_channels;
	if (c->channels == 2) {
		c->channel_layout = AV_CH_LAYOUT_STEREO;
	}
	if (codec_id == AV_CODEC_ID_AAC) {
		LOGI("prepare for AAC audio encoder \n");

		// AAC encoder is experimental, so we need to set this
		c->strict_std_compliance = -2;

		// AAC encoder only support float format
		c->sample_fmt = AV_SAMPLE_FMT_FLTP;
	}

	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	LOGI("audio stream added \n");
	return st;
}

int VideoRecorder::open_audio()
{
	if (!audio_st) {
		LOGE("no audio stream \n");
		return -1;
	}

	AVCodecContext *c = audio_st->codec;
	AVCodec *codec = avcodec_find_encoder(c->codec_id);
	if (!codec) {
		LOGE("find audio encoder failed \n");
		return -1;
	}

	int ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) {
		LOGE("open audio codec failed \n");
		return ret;
	}

	audio_pkt_buf_size = 100000;
	audio_pkt_buf = (uint8_t *)av_malloc(audio_pkt_buf_size);
	if (!audio_pkt_buf) {
		LOGE("allocate audio_pkt_buf failed \n");
		return -1;
	}

	audio_frame = av_frame_alloc();
	if (!audio_frame) {
		LOGE("avcodec_alloc_frame for audio failed \n");
		return -1;
	}

	audio_input_frame_size = c->codec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE ?
	        10000 : c->frame_size;
	samples = av_malloc(audio_input_frame_size * audio_sample_size * c->channels);
	audio_input_leftover_samples = 0;

	// may need re-sample
	if (c->sample_fmt != audio_sample_format) {
		swr_ctx = swr_alloc();
		if (!swr_ctx) {
			LOGE("allocate resampler context failed \n");
			return -1;
		}
		av_opt_set_int       (swr_ctx, "in_channel_count",   c->channels,       0);
		av_opt_set_int       (swr_ctx, "in_sample_rate",     c->sample_rate,    0);
		av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt",      audio_sample_format, 0);
		av_opt_set_int       (swr_ctx, "out_channel_count",  c->channels,       0);
		av_opt_set_int       (swr_ctx, "out_sample_rate",    c->sample_rate,    0);
		av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);
		int ret = swr_init(swr_ctx);
		if (ret < 0) {
			LOGE("initialize the resampling context failed \n");
			return ret;
		}

		max_dst_nb_samples = audio_input_frame_size;
		int line_size;
		// allocate dst_samples buffer: dst_samples and dst_samples[0] are allocated
		// dst_samples[1] = dst_samples[0] + line_size
		ret = av_samples_alloc_array_and_samples(&dst_samples, &line_size, c->channels,
													 max_dst_nb_samples, c->sample_fmt, 0);
		if (ret < 0) {
			LOGE("allocate destination samples failed \n");
			return ret;
		}
		dst_samples_size = av_samples_get_buffer_size(NULL, c->channels, max_dst_nb_samples,
																		  c->sample_fmt, 0);
	}

	LOGI("audio codec opened \n");
	return 0;
}

AVStream *VideoRecorder::add_video_stream(enum AVCodecID codec_id)
{
	AVCodec* codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		LOGE("find video encoder failed \n");
		return NULL;
	}

	AVStream *st = avformat_new_stream(oc, codec);
	if (!st) {
		LOGD("new video stream failed \n");
		return NULL;
	}

	AVCodecContext *c = st->codec;
	/* put sample parameters */
	c->bit_rate = video_bitrate;
	c->width = video_width;
	c->height = video_height;
	c->time_base.num = 1;
	c->time_base.den = 15;
	c->gop_size = 25;
	c->thread_count = 5;
	c->pix_fmt = video_pixfmt;
	if (codec_id == AV_CODEC_ID_HEVC) {
		av_opt_set(c->priv_data, "preset", "ultrafast", 0);
		av_opt_set(c->priv_data, "wpp", "4", 0);
		av_opt_set(c->priv_data, "disable_sei", "0", 0);
		av_opt_set(c->priv_data, "HM_compatibility", "12", 0);
		//av_opt_set(c->priv_data, "dump_bs", "/sdcard/dump.bs", 0);
	}

	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	LOGI("video stream added \n");
	return st;
}

int VideoRecorder::open_video()
{
	if (!video_st) {
		LOGE("no video stream \n");
		return -1;
	}

	AVCodecContext *c = video_st->codec;
	AVCodec *codec = avcodec_find_encoder(c->codec_id); //why not use c->codec ?
	int ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) {
		LOGE("avcodec_open2 failed \n");
		return ret;
	}

	// We assume the encoded frame will be smaller in size than an equivalent raw frame in RGBA8888 format ... a pretty safe assumption!
	video_pkt_buf_size = c->width * c->height * 4;
	video_pkt_buf = (uint8_t *)av_malloc(video_pkt_buf_size);
	if (!video_pkt_buf) {
		LOGE("could not allocate video_pkt_buf\n");
		return -1;
	}

	video_frame = av_frame_alloc();
	if (!video_frame) {
		LOGE("avcodec_alloc_frame for video failed \n");
		return -1;
	}

	timestamp_start = 0;

	LOGI("video codec opened \n");
	return 0;
}

int VideoRecorder::close()
{
	if (oc) {
		// flush out delayed frames
		LOGI("flush out delayed frames \n");
		int out_size;
		AVCodecContext *c = video_st->codec;
		int got_packet = 1, ret = -1;
		while (got_packet == 1) {
			av_init_packet(&video_pkt);
			video_pkt.data = video_pkt_buf;
			video_pkt.size = video_pkt_buf_size;
			ret = avcodec_encode_video2(c, &video_pkt, NULL, &got_packet);
			if (ret < 0) {
				LOGE("Error encoding video frame: %d\n", ret);
				return ret;
			}
			if (got_packet) {
				LOGD("got a video packet \n");
				int ret = write_frame(oc, &c->time_base, video_st, &video_pkt);
				if (ret < 0) {
					LOGE("Error while writing video packet: %d \n", ret);
					return ret;
				}
			}
		}

		// audio also needs flushing
		if (audio_st) {
			AVCodecContext *c = audio_st->codec;
			int got_packet = 1, ret = -1;
			while (got_packet == 1) {
				av_init_packet(&audio_pkt);
				audio_pkt.data = audio_pkt_buf;
				audio_pkt.size = audio_pkt_buf_size;
				ret = avcodec_encode_audio2(c, &audio_pkt, NULL, &got_packet);
				if (ret < 0) {
					LOGE("Error encoding audio frame: %d\n", ret);
					return ret;
				}
				if (got_packet) {
					LOGD("got an audio packet \n");
					int ret = write_frame(oc, &c->time_base, audio_st, &audio_pkt);
					if (ret < 0) {
						LOGE("Error while writing audio packet: %d \n", ret);
						return ret;
					}
				}
			}
		}

		LOGI("write trailer \n");
		av_write_trailer(oc);
	}

	if (video_st) {
		avcodec_close(video_st->codec);
	}

	if (video_pkt_buf) {
		av_free(video_pkt_buf);
	}

	if (video_frame) {
		avcodec_free_frame(&video_frame);
	}

	if (audio_st) {
		avcodec_close(audio_st->codec);
	}

	if (audio_pkt_buf) {
		av_free(audio_pkt_buf);
	}

	if (audio_frame) {
		avcodec_free_frame(&audio_frame);
	}

	if (samples) {
		av_free(samples);
	}

	if (dst_samples) {
		av_free(dst_samples[0]);
		av_free(dst_samples);
	}

	if (swr_ctx) {
		swr_free(&swr_ctx);
	}

	if (oc) {
		for (int i = 0; i < oc->nb_streams; i++) {
			av_freep(&oc->streams[i]->codec);
			av_freep(&oc->streams[i]);
		}
		avio_close(oc->pb);
		av_free(oc);
	}

	pthread_mutex_destroy(&write_mutex);

	LOGI("recorder closed \n");
	return 0;
}

int VideoRecorder::setVideoOptions(VideoFrameFormat fmt, int width, int height, unsigned long bitrate)
{
	switch (fmt) {
		case VideoFrameFormatYUV420P: video_pixfmt = PIX_FMT_YUV420P; break;
		case VideoFrameFormatNV12: video_pixfmt = PIX_FMT_NV12; break;
		case VideoFrameFormatNV21: video_pixfmt = PIX_FMT_NV21; break;
		case VideoFrameFormatRGB24: video_pixfmt = PIX_FMT_RGB24; break;
		case VideoFrameFormatBGR24: video_pixfmt = PIX_FMT_BGR24; break;
		case VideoFrameFormatARGB: video_pixfmt = PIX_FMT_ARGB; break;
		case VideoFrameFormatRGBA: video_pixfmt = PIX_FMT_RGBA; break;
		case VideoFrameFormatABGR: video_pixfmt = PIX_FMT_ABGR; break;
		case VideoFrameFormatBGRA: video_pixfmt = PIX_FMT_BGRA; break;
		case VideoFrameFormatRGB565LE: video_pixfmt = PIX_FMT_RGB565LE; break;
		case VideoFrameFormatRGB565BE: video_pixfmt = PIX_FMT_RGB565BE; break;
		case VideoFrameFormatBGR565LE: video_pixfmt = PIX_FMT_BGR565LE; break;
		case VideoFrameFormatBGR565BE: video_pixfmt = PIX_FMT_BGR565BE; break;
		default:
			LOGE("Unknown frame format passed to SetVideoOptions!\n");
			return -1;
	}
	video_width = width;
	video_height = height;
	video_bitrate = bitrate;
	return 0;
}

int VideoRecorder::setAudioOptions(AudioSampleFormat fmt, int channels, unsigned long samplerate, unsigned long bitrate)
{
	switch (fmt) {
		case AudioSampleFormatU8: audio_sample_format = AV_SAMPLE_FMT_U8; audio_sample_size = 1; break;
		case AudioSampleFormatS16: audio_sample_format = AV_SAMPLE_FMT_S16; audio_sample_size = 2; break;
		case AudioSampleFormatS32: audio_sample_format = AV_SAMPLE_FMT_S32; audio_sample_size = 4; break;
		case AudioSampleFormatFLT: audio_sample_format = AV_SAMPLE_FMT_FLT; audio_sample_size = 4; break;
		case AudioSampleFormatDBL: audio_sample_format = AV_SAMPLE_FMT_DBL; audio_sample_size = 8; break;
		default:
			LOGE("Unknown sample format passed to setAudioOptions!\n");
			return -1;
	}

	audio_channels = channels;
	audio_bit_rate = bitrate;
	audio_sample_rate = samplerate;
	return 0;
}

int VideoRecorder::write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
    pkt->pts = av_rescale_q_rnd(pkt->pts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    pkt->dts = av_rescale_q_rnd(pkt->dts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    pkt->duration = av_rescale_q(pkt->duration, *time_base, st->time_base);
    pkt->stream_index = st->index;

    /* Write the compressed frame to the media file. */
    int ret = -1;
	pthread_mutex_lock(&write_mutex);
    ret = av_interleaved_write_frame(fmt_ctx, pkt);
	pthread_mutex_unlock(&write_mutex);
	return ret;
}

int VideoRecorder::supplyAudioSamples(const void *sampleData, unsigned long numBytes)
{
	// check whether there is any audio stream
	if (audio_st == NULL) {
		LOGE("tried to supply an audio frame when no audio stream was present \n");
		return -1;
	}

	unsigned long numSamples = numBytes / audio_sample_size / audio_channels;
	LOGD("supply audio data: %lu bytes, %lu samples \n", numBytes, numSamples);

	AVCodecContext *c = audio_st->codec;
	uint8_t *data = (uint8_t *)sampleData;

	// if numSamples is too large, we will go through it audio_input_frame_size samples at a time
	while (numSamples > 0) {
		// if we have enough samples for a frame, we write out audio_input_frame_size number of samples (ie: one frame) to the output context
		if (numSamples + audio_input_leftover_samples >= audio_input_frame_size) {
			// audio_input_leftover_samples contains the number of samples already in our "samples" array, left over from last time
			// we copy the remaining samples to fill up the frame to the complete frame size
			int num_new_samples = audio_input_frame_size - audio_input_leftover_samples;

			memcpy((uint8_t *)samples + (audio_input_leftover_samples * audio_sample_size * audio_channels), data, num_new_samples * audio_sample_size * audio_channels);
			numSamples -= num_new_samples;
			data += (num_new_samples * audio_sample_size * audio_channels);
			audio_input_leftover_samples = 0;

			// prepare audio frame
			int dst_nb_samples = audio_input_frame_size;
			int ret = 0;
			if (swr_ctx != NULL) {
				// do re-sampling
				dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, c->sample_rate) + audio_input_frame_size,
						c->sample_rate, c->sample_rate, AV_ROUND_UP);
				if (dst_nb_samples > max_dst_nb_samples) {
					LOGI("reallocate dst_samples \n");
					av_free(dst_samples[0]);
					ret = av_samples_alloc(dst_samples, NULL, c->channels,
										   dst_nb_samples, c->sample_fmt, 0);
					if (ret < 0) {
						LOGE("allocate dst_samples failed \n");
						return ret;
					}
					max_dst_nb_samples = dst_nb_samples;
					dst_samples_size = av_samples_get_buffer_size(NULL, c->channels, dst_nb_samples,
																  c->sample_fmt, 0);
				}

				ret = swr_convert(swr_ctx, dst_samples, dst_nb_samples,
				                          (const uint8_t **)&samples, audio_input_frame_size);
				if (ret < 0) {
					LOGE("Error while converting\n");
					return ret;
				}
			} else {
				dst_samples[0] = (uint8_t*)samples;
			}

			audio_frame->nb_samples = dst_nb_samples;
			audio_frame->pts = av_rescale_q(samples_count, (AVRational){1, c->sample_rate}, c->time_base);
			avcodec_fill_audio_frame(audio_frame, c->channels, c->sample_fmt,
					dst_samples[0], dst_samples_size, 0);

			// decode to get packet
			// we need to initialize packet every time so all the values (such as pts) are re-initialized
			av_init_packet(&audio_pkt);
			audio_pkt.data = audio_pkt_buf;
			audio_pkt.size = audio_pkt_buf_size;

		    int got_packet = 0;
			ret = avcodec_encode_audio2(c, &audio_pkt, audio_frame, &got_packet);
			if (ret < 0) {
				LOGE("Error encoding audio frame: %d\n", ret);
				return ret;
			}
			if (got_packet) {
				LOGD("got an audio packet \n");
				ret = write_frame(oc, &c->time_base, audio_st, &audio_pkt);
				if (ret < 0) {
					LOGE("Error while writing audio packet: %d \n", ret);
					return ret;
				}
			}

			samples_count += audio_frame->nb_samples;
		} else {
			// if we didn't have enough samples for a frame, we copy over however many we had and update audio_input_leftover_samples
			int num_new_samples = audio_input_frame_size - audio_input_leftover_samples;
			if (numSamples < num_new_samples) {
				num_new_samples = numSamples;
			}

			memcpy((uint8_t *)samples + (audio_input_leftover_samples * audio_sample_size * audio_channels), data, num_new_samples * audio_sample_size * audio_channels);
			numSamples -= num_new_samples;
			data += (num_new_samples * audio_sample_size * audio_channels);
			audio_input_leftover_samples += num_new_samples;
		}
	}

	return 0;
}

int VideoRecorder::supplyVideoFrame(const void *frameData, unsigned long numBytes, unsigned long timestamp)
{
	LOGD("supply video data: %lu bytes \n",numBytes);

	AVCodecContext *c = video_st->codec;
	int width = c->width, height = c->height;

	// prepare the frame
	uint8_t* data = (uint8_t*) frameData;
	int stride_y = (width % 16 == 0 ? width/16 : width/16 + 1)*16;
	int stride_uv = (width/2 % 16 == 0 ? width/2 / 16 : width/2 / 16 + 1)*16;
	// Android's YV12 format stores YUV as YCrCb
	video_frame->data[0] = data;
	video_frame->data[2] = (uint8_t*)(data + height * stride_y);
	video_frame->data[1] = (uint8_t*)(data + height * stride_y + height/2 * stride_uv);
	video_frame->linesize[0] = stride_y;
	video_frame->linesize[1] = video_frame->linesize[2] = stride_uv;

	if (timestamp_start == 0) {
		timestamp_start = timestamp;
	}
	video_frame->pts = (timestamp - timestamp_start);

	// decode to get packet
	av_init_packet(&video_pkt);
	video_pkt.data = video_pkt_buf;
	video_pkt.size = video_pkt_buf_size;
    int got_packet = 0;
	int ret = avcodec_encode_video2(c, &video_pkt, video_frame, &got_packet);
	if (ret < 0) {
		LOGE("Error encoding video frame: %d \n", ret);
		return ret;
	}
	if (got_packet) {
		LOGD("got a video packet \n");
		ret = write_frame(oc, &c->time_base, video_st, &video_pkt);
		if (ret < 0) {
			LOGE("Error while writing video packet: %d \n", ret);
			return ret;
		}
	}

	return 0;
}

