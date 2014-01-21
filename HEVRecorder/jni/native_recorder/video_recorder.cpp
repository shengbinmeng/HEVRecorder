#include <time.h>
#include"jni_utils.h"
#include "video_recorder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
}

#define LOG_TAG "VideoRecorder"

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
    pkt->pts = av_rescale_q_rnd(pkt->pts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    pkt->dts = av_rescale_q_rnd(pkt->dts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    pkt->duration = av_rescale_q(pkt->duration, *time_base, st->time_base);
    pkt->stream_index = st->index;

    /* Write the compressed frame to the media file. */
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

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
	samples = NULL;
	audio_pkt_buf = NULL;
	audio_st = NULL;
	audio_input_leftover_samples = 0;

	video_frame = audio_frame = NULL;
	video_pkt_buf = NULL;
	video_st = NULL;
	img_convert_ctx = NULL;

	oc = NULL;

	av_log_set_callback(ffmpeg_log_callback);
}

VideoRecorder::~VideoRecorder()
{

}

bool VideoRecorder::open(const char *file, bool hasAudio)
{
	//fopen end
	av_register_all();
	avcodec_register_all();

	avformat_alloc_output_context2(&oc, NULL, NULL, file);
	if (!oc) {
		LOGE("alloc_output_context failed \n");
		return false;
	}

	video_st = add_video_stream(AV_CODEC_ID_HEVC);
	if (hasAudio) {
		audio_st = add_audio_stream(CODEC_ID_AAC);
	}

	// for debug
	av_dump_format(oc, 0, file, 1);

	open_video();
	if (hasAudio) {
		open_audio();
	}

	if (avio_open(&oc->pb, file, AVIO_FLAG_WRITE) < 0) {
		LOGE("open file failed: %s \n", file);
		return false;
	}

	avformat_write_header(oc, &pAVDictionary);

	LOGI("recoder opened \n");
	return true;
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
	c->channel_layout = AV_CH_LAYOUT_STEREO;

	if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	return st;
}

void VideoRecorder::open_audio()
{
	AVCodecContext *c = audio_st->codec;
	AVCodec *codec = avcodec_find_encoder(c->codec_id);
	if (!codec) {
		LOGE("find audio encoder failed \n");
		return;
	}

	if (avcodec_open2(c, codec, NULL) < 0) {
		LOGE("open audio codec failed \n");
		return;
	}

	audio_pkt_buf_size = 10000;
	audio_pkt_buf = (uint8_t *)av_malloc(audio_pkt_buf_size);
	if (!audio_pkt_buf) {
		LOGE("allocate audio_pkt_buf failed \n");
		return;
	}
	audio_pkt.data = audio_pkt_buf;

	audio_frame = av_frame_alloc();
	if (!audio_frame) {
		LOGE("avcodec_alloc_frame for audio failed \n");
		return;
	}
	audio_input_frame_size = c->frame_size;
	samples = (int16_t *)av_malloc(audio_input_frame_size * audio_sample_size * c->channels);

	audio_input_leftover_samples = 0;
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
	c->thread_count = 2;
	c->pix_fmt = PIX_FMT_YUV420P;
	av_opt_set(c->priv_data, "preset", "ultrafast",0);
	av_opt_set(c->priv_data, "wpp", "4",0);
	av_opt_set(c->priv_data, "disable_sei", "1",0);
	av_opt_set(c->priv_data, "HM_compatibility", "12",0);
	return st;
}

void VideoRecorder::open_video()
{
	timestamp_start = 0;

	if (!video_st) {
		LOGE("no video stream \n");
		return;
	}

	AVCodecContext *c = video_st->codec;
	AVCodec *codec = avcodec_find_encoder(c->codec_id); //why not use c->codec ?
	if (avcodec_open2(c, codec, NULL) < 0) {
		LOGE("avcodec_open2 failed \n");
		return;
	}

	// We assume the encoded frame will be smaller in size than an equivalent raw frame in RGBA8888 format ... a pretty safe assumption!
	video_pkt_buf_size = c->width * c->height * 4;
	video_pkt_buf = (uint8_t *)av_malloc(video_pkt_buf_size);
	if (!video_pkt_buf) {
		LOGE("could not allocate video_pkt_buf\n");
		return;
	}

	video_frame = av_frame_alloc();
	if (!video_frame) {
		LOGE("avcodec_alloc_frame for video failed \n");
		return;
	}
}

bool VideoRecorder::close()
{
	if (oc) {
		// flush out delayed frames
		int out_size;
		av_init_packet(&video_pkt);
		video_pkt.data = video_pkt_buf;
		video_pkt.size = video_pkt_buf_size;
		AVCodecContext *c = video_st->codec;
		int got_packet = 0;
		while (avcodec_encode_video2(c, &video_pkt, NULL, &got_packet) == 0 && got_packet == 1) {
			if (got_packet) {
				LOGD("got a video packet \n");
				int ret = write_frame(oc, &c->time_base, video_st, &video_pkt);
				if (ret < 0) {
					LOGE("Error while writing video packet: %d \n", ret);
					return false;
				}
				av_init_packet(&video_pkt);
				video_pkt.data = video_pkt_buf;
				video_pkt.size = video_pkt_buf_size;
			}
		}

		av_write_trailer(oc);
	}

	if (video_st)
		avcodec_close(video_st->codec);

	if (video_pkt_buf) {
		av_free(video_pkt_buf);
	}

	if (video_frame) {
		avcodec_free_frame(&video_frame);
	}

	if (img_convert_ctx) {
		sws_freeContext(img_convert_ctx);
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

	if (oc) {
		for (int i = 0; i < oc->nb_streams; i++) {
			av_freep(&oc->streams[i]->codec);
			av_freep(&oc->streams[i]);
		}
		avio_close(oc->pb);
		av_free(oc);
	}
	LOGI("recorder closed \n");
	return true;
}

bool VideoRecorder::setVideoOptions(VideoFrameFormat fmt, int width, int height, unsigned long bitrate)
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
			return false;
	}
	video_width = width;
	video_height = height;
	video_bitrate = bitrate;
	return true;
}

bool VideoRecorder::setAudioOptions(AudioSampleFormat fmt, int channels, unsigned long samplerate, unsigned long bitrate)
{
	switch (fmt) {
		case AudioSampleFormatU8: audio_sample_format = AV_SAMPLE_FMT_U8; audio_sample_size = 1; break;
		case AudioSampleFormatS16: audio_sample_format = AV_SAMPLE_FMT_S16; audio_sample_size = 2; break;
		case AudioSampleFormatS32: audio_sample_format = AV_SAMPLE_FMT_S32; audio_sample_size = 4; break;
		case AudioSampleFormatFLT: audio_sample_format = AV_SAMPLE_FMT_FLT; audio_sample_size = 4; break;
		case AudioSampleFormatDBL: audio_sample_format = AV_SAMPLE_FMT_DBL; audio_sample_size = 8; break;
		default:
			LOGE("Unknown sample format passed to setAudioOptions!\n");
			return false;
	}
	audio_channels = channels;
	audio_bit_rate = bitrate;
	audio_sample_rate = samplerate;
	return true;
}

bool VideoRecorder::start()
{
	return true;
}

void VideoRecorder::supplyAudioSamples(const void *sampleData, unsigned long numSamples)
{
	// check whether there is any audio stream (hasAudio=true)
	if (audio_st == NULL) {
		LOGE("tried to supply an audio frame when no audio stream was present\n");
		return;
	}
	AVCodecContext *c = audio_st->codec;
	uint8_t *samplePtr = (uint8_t *)sampleData;

	// numSamples is supplied by the codec.. should be c->frame_size (1024 for AAC)
	// if it's more we go through it c->frame_size samples at a time
	while (numSamples) {
		// need to init packet every time so all the values (such as pts) are re-initialized

		// if we have enough samples for a frame, we write out c->frame_size number of samples (ie: one frame) to the output context
		if (numSamples + audio_input_leftover_samples >= c->frame_size) {
			// audio_input_leftover_samples contains the number of samples already in our "samples" array, left over from last time
			// we copy the remaining samples to fill up the frame to the complete frame size
			int num_new_samples = c->frame_size - audio_input_leftover_samples;

			memcpy((uint8_t *)samples + (audio_input_leftover_samples * audio_sample_size * audio_channels), samplePtr, num_new_samples * audio_sample_size * audio_channels);
			numSamples -= num_new_samples;
			samplePtr += (num_new_samples * audio_sample_size * audio_channels);
			audio_input_leftover_samples = 0;

			//TODO: prepare audio frame
			audio_frame->nb_samples = c->frame_size;
			audio_frame->pts = av_rescale_q(samples_count, (AVRational){1, c->sample_rate}, c->time_base);
			avcodec_fill_audio_frame(audio_frame, c->channels, c->sample_fmt,
					(const uint8_t*)samples, audio_input_frame_size * audio_sample_size * c->channels, 0);

			// decode to get packet
			av_init_packet(&audio_pkt);
		    int got_packet = 0;
			int ret = avcodec_encode_audio2(c, &audio_pkt, audio_frame, &got_packet);
			if (ret < 0) {
				LOGE("Error encoding audio frame: %d\n", ret);
				return;
			}
			if (got_packet) {
				LOGD("got an audio packet \n");
				ret = write_frame(oc, &c->time_base, audio_st, &audio_pkt);
				if (ret < 0) {
					LOGE("Error while writing audio packet: %d \n", ret);
					return;
				}
			}
			samples_count += audio_frame->nb_samples;
		} else {
			// if we didn't have enough samples for a frame, we copy over however many we had and update audio_input_leftover_samples
			int num_new_samples = c->frame_size - audio_input_leftover_samples;
			if (numSamples < num_new_samples) {
				num_new_samples = numSamples;
			}

			memcpy((uint8_t *)samples + (audio_input_leftover_samples * audio_sample_size * audio_channels), samplePtr, num_new_samples * audio_sample_size * audio_channels);
			numSamples -= num_new_samples;
			samplePtr += (num_new_samples * audio_sample_size * audio_channels);
			audio_input_leftover_samples += num_new_samples;
		}
	}
}

void VideoRecorder::supplyVideoFrame(const void *frameData, unsigned long numBytes, unsigned long timestamp)
{
	AVCodecContext *c = video_st->codec;

	int width = c->width, height = c->height;
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
		return;
	}
	if (got_packet) {
		LOGD("got a video packet \n");
		ret = write_frame(oc, &c->time_base, video_st, &video_pkt);
		if (ret < 0) {
			LOGE("Error while writing video packet: %d \n", ret);
			return;
		}
	}
}

