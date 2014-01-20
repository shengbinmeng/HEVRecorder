#include <time.h>
#include"jni_utils.h"

#include "video_recorder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}

#define LOG_TAG "VideoRecorder"

VideoRecorder *recorder;
uint16_t *sound_buffer;
uint8_t *video_buffer;
int isRecording;
int frameCnt;

VideoRecorder::VideoRecorder()
{
	samples = NULL;
	audio_pkt_buf = NULL;
	audio_st = NULL;

	audio_input_leftover_samples = 0;

	video_pkt_buf = NULL;
	video_st = NULL;

	picture = NULL;
	tmp_picture = NULL;
	img_convert_ctx = NULL;

	oc = NULL;
}

VideoRecorder::~VideoRecorder()
{

}

bool VideoRecorder::open(const char *file, bool hasAudio)
{
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

	// debug
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

	return true;
}

AVStream *VideoRecorder::add_audio_stream(enum AVCodecID codec_id)
{
    AVCodec *codec = avcodec_find_encoder(codec_id);
	AVStream *st = avformat_new_stream(oc, codec);
	if (!st) {
		LOGE("could not alloc stream\n");
		return NULL;
	}

	AVCodecContext *c = st->codec;
	c->codec_id = codec_id;
	c->codec_type = AVMEDIA_TYPE_AUDIO;
	c->sample_fmt = audio_sample_format;
	c->bit_rate = audio_bit_rate;
	c->sample_rate = audio_sample_rate;
	c->channels = audio_channels;
	c->profile = FF_PROFILE_AAC_LOW;

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
		LOGE("audio codec not found\n");
		return;
	}

	if (avcodec_open2(c, codec, NULL) < 0) {
		LOGE("could not open audio codec\n");
		return;
	}

	// for output
	audio_pkt_buf_size = 10000;
	audio_pkt_buf = (uint8_t *)av_malloc(audio_pkt_buf_size);
	if (!audio_pkt_buf) {
		LOGE("could not allocate audio_pkt_buf \n");
		return;
	}
	audio_pkt.data = audio_pkt_buf;

	// for input
	audio_frame = avcodec_alloc_frame();
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
		LOGE("find encoder failed \n");
	}
	AVStream *st = avformat_new_stream(oc, NULL);
	if (!st) {
		LOGD("could not alloc stream\n");
		return NULL;
	}
	st->codec = avcodec_alloc_context3(codec);

	AVCodecContext *c = st->codec;
	/* put sample parameters */
	c->bit_rate = video_bitrate;
	c->width = video_width;
	c->height = video_height;
	//c->time_base.num = 15000; // w
	//c->time_base.den = 1000;  // w
	c->gop_size=25;
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
	AVCodec *codec;
	AVCodecContext *c;

	timestamp_base = 0;

	if (!video_st) {
		LOGE("no video stream \n");
		return;
	}

	AVCodecContext *c = video_st->codec;
	AVCodec *codec = avcodec_find_encoder(c->codec_id); //why not use c->codec ?
	if (avcodec_open2(c, codec,NULL) < 0) {
		LOGE("avcodec_open2 failed \n");
		return;
	}

	// for output
	video_pkt_buf_size = c->width * c->height * 4; // We assume the encoded frame will be smaller in size than an equivalent raw frame in RGBA8888 format ... a pretty safe assumption!
	video_pkt_buf = (uint8_t *)av_malloc(video_pkt_buf_size);
	if (!video_pkt_buf) {
		LOGE("could not allocate video_pkt_buf\n");
		return;
	}

	// for input
	video_frame = avcodec_alloc_frame();
	if (!video_frame) {
		LOGE("avcodec_alloc_frame for video failed \n");
		return;
	}

	int size = avpicture_get_size(c->pix_fmt, c->width, c->height);
	frame_buf = (uint8_t *)av_malloc(size);
	if (!frame_buf) {
		av_free(frame);
		LOGE("allocate frame buffer failed \n");
		return;
	}
	avpicture_fill((AVPicture *)video_frame, frame_buf,
			c->pix_fmt, c->width, c->height);
}

bool VideoRecorder::close()
{
	if (oc) {
		// flush out delayed frames
		int out_size;
		av_init_packet(&video_pkt);
		AVCodecContext *c = video_st->codec;
		int got_packet = 0;
		while (avcodec_encode_video2(c, &video_pkt, NULL, &got_packet) == 0) {
			if (c->coded_frame->pts != AV_NOPTS_VALUE) {
				video_pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, video_st->time_base);
			}

			if (av_interleaved_write_frame(oc, &video_pkt) != 0) {
				LOGE("Unable to write video frame when flushing delayed frames \n");
				return false;
			} else {
				LOGD("wrote delayed frame of size %d\n", out_size);
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
			LOGE("Unknown sample format passed to SetAudioOptions!\n");
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

			// decode to get packet
			av_init_packet(&audio_pkt);
		    int got_packet = 0;
			int ret = avcodec_encode_audio2(c, &audio_pkt, audio_frame, &got_packet);
			if (ret < 0) {
				LOGE("Error encoding audio frame: %s\n", av_err2str(ret));
				return;
			}
			if (got_packet) {
				//TODO: time stamp

				audio_pkt.stream_index = audio_st->index;
				ret = av_interleaved_write_frame(oc, &audio_pkt);
				if (ret < 0) {
					LOGE("Error while writing audio packet: %s \n", av_err2str(ret));
					return;
				}
			}
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

	//TODO: prepare video frame
	memcpy(video_frame->data[0], frameData, numBytes);
	if (timestamp_base == 0) {
		timestamp_base = timestamp;
	}
	video_frame->pts = 90 * (timestamp - timestamp_base);	// assuming millisecond timestamp and 90 kHz timebase

	// decode to get packet
	av_init_packet(&video_pkt);
    int got_packet = 0;
	int ret = avcodec_encode_video2(c, &video_pkt, video_frame, &got_packet);
	if (ret < 0) {
		LOGE("Error encoding video frame: %s \n", av_err2str(ret));
		return;
	}
	if (got_packet && video_pkt.size) {
		//TODO: time stamp

		video_pkt.stream_index = video_st->index;
		ret = av_interleaved_write_frame(oc, &video_pkt);
		if (ret < 0) {
			LOGE("Error while writing video packet: &s \n", av_err2str(ret));
			return;
		}
	}
}

