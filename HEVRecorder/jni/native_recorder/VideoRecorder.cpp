// compiles on MacOS X with: g++ -DTESTING VideoRecorder.cpp -o v -lavcodec -lavformat -lavutil -lswscale -lx264 -g
#include <time.h>
#ifdef ANDROID
#include"jni_utils.h"
#endif

#ifdef TESTING
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

#include "VideoRecorder.h"

#define LOG_TAG "native_recorder"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}

// Do not use C++ exceptions, templates, or RTTI

VideoRecorder *recorder;
uint16_t *sound_buffer;
uint8_t *video_buffer;
int isRecording;
int frameCnt;
class VideoRecorderImpl : public VideoRecorder {
public:
	VideoRecorderImpl();
	~VideoRecorderImpl();

	bool SetVideoOptions(VideoFrameFormat fmt, int width, int height, unsigned long bitrate);
	bool SetAudioOptions(AudioSampleFormat fmt, int channels, unsigned long samplerate, unsigned long bitrate);

	bool Open(const char *mp4File, bool hasAudio, bool dbg);
	bool Close();

	bool Start();

	void SupplyVideoFrame(const void *frame, unsigned long numBytes, unsigned long timestamp);
	void SupplyAudioSamples(const void *samples, unsigned long numSamples);

private:
	AVStream *add_audio_stream(enum AVCodecID codec_id);
	void open_audio();
	void write_audio_frame(AVStream *st);

	AVStream *add_video_stream(enum AVCodecID codec_id);
	AVFrame *alloc_picture(enum PixelFormat pix_fmt, int width, int height);
	void open_video();
	void write_video_frame(AVStream *st);

	// audio related vars
	int16_t *samples;
	uint8_t *audio_outbuf;
	int audio_outbuf_size;
	int audio_input_frame_size;
	AVStream *audio_st;

	unsigned long audio_input_leftover_samples;

	int audio_channels;				// number of channels (2)
	unsigned long audio_bit_rate;		// codec's output bitrate
	unsigned long audio_sample_rate;		// number of samples per second
	int audio_sample_size;					// size of each sample in bytes (16-bit = 2)
	AVSampleFormat audio_sample_format;

	// video related vars
	uint8_t *video_outbuf;
	int video_outbuf_size;
	AVStream *video_st;

	int video_width;
	int video_height;
	unsigned long video_bitrate;
	PixelFormat video_pixfmt;
	AVFrame *picture;			// video frame after being converted to x264-friendly YUV420P
	AVFrame *tmp_picture;		// video frame before conversion (RGB565)
	SwsContext *img_convert_ctx;

	unsigned long timestamp_base;

	// common
	AVFormatContext *oc;
	AVDictionary* pAVDictionary = NULL;
	uint8_t *picture_buf;

};

VideoRecorder::VideoRecorder()
{

}

VideoRecorder::~VideoRecorder()
{

}

VideoRecorderImpl::VideoRecorderImpl()
{
	samples = NULL;
	audio_outbuf = NULL;
	audio_st = NULL;

	audio_input_leftover_samples = 0;

	video_outbuf = NULL;
	video_st = NULL;

	picture = NULL;
	tmp_picture = NULL;
	img_convert_ctx = NULL;

	oc = NULL;
}

VideoRecorderImpl::~VideoRecorderImpl()
{

}

bool VideoRecorderImpl::Open(const char *mp4File, bool hasAudio, bool dbg)
{
	av_register_all();
	avcodec_register_all();
	avformat_alloc_output_context2(&oc, NULL, NULL, mp4File);
	if (!oc) {
		LOGE("could not deduce output format from file extension\n");
		return false;
	}
	video_st = add_video_stream(AV_CODEC_ID_HEVC);

	if(hasAudio)
		audio_st = add_audio_stream(CODEC_ID_AAC);

	if(dbg)
		av_dump_format(oc, 0, mp4File, 1);

	open_video();

	if(hasAudio)
		open_audio();

	if (avio_open(&oc->pb, mp4File, AVIO_FLAG_WRITE) < 0) {
		LOGE("could not open '%s'\n", mp4File);
		return false;
	}

	avformat_write_header(oc,&pAVDictionary);

	return true;
}

AVStream *VideoRecorderImpl::add_audio_stream(enum AVCodecID codec_id)
{
	AVCodecContext *c;
	AVStream *st;
    AVCodec *codec;
    codec= avcodec_find_encoder(c->codec_id);
	st = avformat_new_stream(oc,codec);
	if (!st) {
		LOGE("could not alloc stream\n");
		return NULL;
	}

	c = st->codec;
	c->codec_id = codec_id;
	c->codec_type = AVMEDIA_TYPE_AUDIO;
	c->sample_fmt = audio_sample_format;
	c->bit_rate = audio_bit_rate;
	c->sample_rate = audio_sample_rate;
	c->channels = audio_channels;
	c->profile = FF_PROFILE_AAC_LOW;

	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;

	return st;
}

void VideoRecorderImpl::open_audio()
{
	AVCodecContext *c;
	AVCodec *codec;

	c = audio_st->codec;

	codec = avcodec_find_encoder(c->codec_id);
	if (!codec) {
		LOGE("audio codec not found\n");
		return;
	}

	if (avcodec_open2(c, codec,NULL) < 0) {
		LOGE("could not open audio codec\n");
		return;
	}

	audio_outbuf_size = 10000; // XXX TODO
	audio_outbuf = (uint8_t *)av_malloc(audio_outbuf_size);

	audio_input_frame_size = c->frame_size;
	samples = (int16_t *)av_malloc(audio_input_frame_size * audio_sample_size * c->channels);

	audio_input_leftover_samples = 0;
}

AVStream *VideoRecorderImpl::add_video_stream(enum AVCodecID codec_id)
{
	AVCodecContext *c;
	AVStream *st;
	AVCodec* pCodecH265;
	pCodecH265 = avcodec_find_encoder(codec_id);
	if(!pCodecH265)
	{
	  fprintf(stderr, "hevc codec not found\n");
	  exit(1);
	}
	st = avformat_new_stream(oc, NULL);
	if (!st) {
		LOGD("could not alloc stream\n");
		return NULL;
	}
	st->codec= avcodec_alloc_context3(pCodecH265);
	c = st->codec;

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

AVFrame *VideoRecorderImpl::alloc_picture(enum PixelFormat pix_fmt, int width, int height)
{
	AVFrame *pict;

	int size;

	pict = avcodec_alloc_frame();
	if (!pict) {
		LOGE("could not allocate picture frame\n");
		return NULL;
	}

	size = avpicture_get_size(pix_fmt, width, height);
	picture_buf = (uint8_t *)av_malloc(size);
	if (!picture_buf) {
		av_free(pict);
		LOGE("could not allocate picture frame buf\n");
		return NULL;
	}
	avpicture_fill((AVPicture *)pict, picture_buf,
				   pix_fmt, width, height);
	return pict;
}

void VideoRecorderImpl::open_video()
{
	AVCodec *codec;
	AVCodecContext *c;

	timestamp_base = 0;

	if(!video_st) {
		LOGE("tried to open_video without a valid video_st (add_video_stream must have failed)\n");
		return;
	}

	c = video_st->codec;

	codec = avcodec_find_encoder(c->codec_id);
	if (!codec) {
		LOGE("codec not found\n");
		return;
	}

	if (avcodec_open2(c, codec,NULL) < 0) {
		LOGE("could not open codec\n");
		return;
	}

	video_outbuf = NULL;
	if (!(oc->oformat->flags & AVFMT_RAWPICTURE)) {
		video_outbuf_size = c->width * c->height * 4; // We assume the encoded frame will be smaller in size than an equivalent raw frame in RGBA8888 format ... a pretty safe assumption!
		video_outbuf = (uint8_t *)av_malloc(video_outbuf_size);
		if(!video_outbuf) {
			LOGE("could not allocate video_outbuf\n");
			return;
		}
		LOGD("video_buf alloc %d\n",video_outbuf_size);
	}

	// the AVFrame the YUV frame is stored after conversion
	picture = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!picture) {
		LOGE("Could not allocate picture\n");
		return;
	}
/*
	// the src AVFrame before conversion
	tmp_picture = alloc_picture(video_pixfmt, c->width, c->height);
	if (!tmp_picture) {
		LOGE("Could not allocate temporary picture\n");
		return;
	}
	// Instead of allocating the video frame buffer and attaching it tmp_picture, thereby incurring an unnecessary memcpy() in SupplyVideoFrame,
	// we only allocate the tmp_picture structure and set it up with default values. tmp_picture->data[0] is then reassigned to the incoming
	// frame data on the SupplyVideoFrame() call.

	if(video_pixfmt != PIX_FMT_YUV420P) {
		LOGE("We've hardcoded linesize in tmp_picture for PIX_FMT_YUV420P only!!\n");
		return;
	}

	img_convert_ctx = sws_getContext(video_width, video_height, PIX_FMT_NV21, c->width, c->height, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);
	if(img_convert_ctx==NULL) {
		LOGE("Could not initialize sws context\n");
		return;
	}*/
}

bool VideoRecorderImpl::Close()
{
	if(oc) {
		// flush out delayed frames
		AVPacket pkt;
		int out_size;
		av_init_packet(&pkt);
		pkt.data=video_outbuf;
		pkt.size=video_outbuf_size;
		AVCodecContext *c = video_st->codec;
		int got_packet=0;
		while(avcodec_encode_video2(c, &pkt,NULL, &got_packet)==0) {

			LOGD("write!!!!!!!");
			if (c->coded_frame->pts != AV_NOPTS_VALUE)
				pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, video_st->time_base);

			if(av_interleaved_write_frame(oc, &pkt) != 0) {
				LOGE("Unable to write video frame when flushing delayed frames\n");
				return false;
			}
			else {
				LOGD("wrote delayed frame of size %d\n", out_size);
			}
		}

		av_write_trailer(oc);
	}

	if(video_st)
		avcodec_close(video_st->codec);

	if(picture) {
		av_free(picture->data[0]);
		av_free(picture);
	}

	if(tmp_picture) {
		// tmp_picture->data[0] is no longer allocated by us
		//av_free(tmp_picture->data[0]);
		av_free(tmp_picture);
	}

	if(img_convert_ctx) {
		sws_freeContext(img_convert_ctx);
	}

	if(video_outbuf)
		av_free(video_outbuf);

	if(audio_st)
		avcodec_close(audio_st->codec);

	if(samples)
		av_free(samples);

	if(audio_outbuf)
		av_free(audio_outbuf);

	if(oc) {
		for(int i = 0; i < oc->nb_streams; i++) {
			av_freep(&oc->streams[i]->codec);
			av_freep(&oc->streams[i]);
		}
		avio_close(oc->pb);
		av_free(oc);
	}
	return true;
}

bool VideoRecorderImpl::SetVideoOptions(VideoFrameFormat fmt, int width, int height, unsigned long bitrate)
{
	switch(fmt) {
		case VideoFrameFormatYUV420P: video_pixfmt=PIX_FMT_YUV420P; break;
		case VideoFrameFormatNV12: video_pixfmt=PIX_FMT_NV12; break;
		case VideoFrameFormatNV21: video_pixfmt=PIX_FMT_NV21; break;
		case VideoFrameFormatRGB24: video_pixfmt=PIX_FMT_RGB24; break;
		case VideoFrameFormatBGR24: video_pixfmt=PIX_FMT_BGR24; break;
		case VideoFrameFormatARGB: video_pixfmt=PIX_FMT_ARGB; break;
		case VideoFrameFormatRGBA: video_pixfmt=PIX_FMT_RGBA; break;
		case VideoFrameFormatABGR: video_pixfmt=PIX_FMT_ABGR; break;
		case VideoFrameFormatBGRA: video_pixfmt=PIX_FMT_BGRA; break;
		case VideoFrameFormatRGB565LE: video_pixfmt=PIX_FMT_RGB565LE; break;
		case VideoFrameFormatRGB565BE: video_pixfmt=PIX_FMT_RGB565BE; break;
		case VideoFrameFormatBGR565LE: video_pixfmt=PIX_FMT_BGR565LE; break;
		case VideoFrameFormatBGR565BE: video_pixfmt=PIX_FMT_BGR565BE; break;
		default: LOGE("Unknown frame format passed to SetVideoOptions!\n"); return false;
	}
	video_width = width;
	video_height = height;
	video_bitrate = bitrate;
	return true;
}

bool VideoRecorderImpl::SetAudioOptions(AudioSampleFormat fmt, int channels, unsigned long samplerate, unsigned long bitrate)
{
	switch(fmt) {
		case AudioSampleFormatU8: audio_sample_format=AV_SAMPLE_FMT_U8; audio_sample_size=1; break;
		case AudioSampleFormatS16: audio_sample_format=AV_SAMPLE_FMT_S16; audio_sample_size=2; break;
		case AudioSampleFormatS32: audio_sample_format=AV_SAMPLE_FMT_S32; audio_sample_size=4; break;
		case AudioSampleFormatFLT: audio_sample_format=AV_SAMPLE_FMT_FLT; audio_sample_size=4; break;
		case AudioSampleFormatDBL: audio_sample_format=AV_SAMPLE_FMT_DBL; audio_sample_size=8; break;
		default: LOGE("Unknown sample format passed to SetAudioOptions!\n"); return false;
	}
	audio_channels = channels;
	audio_bit_rate = bitrate;
	audio_sample_rate = samplerate;
	return true;
}

bool VideoRecorderImpl::Start()
{
	return true;
}

void VideoRecorderImpl::SupplyAudioSamples(const void *sampleData, unsigned long numSamples)
{
	// check whether there is any audio stream (hasAudio=true)
	if(audio_st == NULL) {
		LOGE("tried to supply an audio frame when no audio stream was present\n");
		return;
	}

	AVCodecContext *c = audio_st->codec;

	uint8_t *samplePtr = (uint8_t *)sampleData;		// using a byte pointer

	// numSamples is supplied by the codec.. should be c->frame_size (1024 for AAC)
	// if it's more we go through it c->frame_size samples at a time
	while(numSamples) {
		static AVPacket pkt;
		av_init_packet(&pkt);	// need to init packet every time so all the values (such as pts) are re-initialized

		// if we have enough samples for a frame, we write out c->frame_size number of samples (ie: one frame) to the output context
		if( (numSamples + audio_input_leftover_samples) >= c->frame_size) {
			// audio_input_leftover_samples contains the number of samples already in our "samples" array, left over from last time
			// we copy the remaining samples to fill up the frame to the complete frame size
			int num_new_samples = c->frame_size - audio_input_leftover_samples;

			memcpy((uint8_t *)samples + (audio_input_leftover_samples * audio_sample_size * audio_channels), samplePtr, num_new_samples * audio_sample_size * audio_channels);
			numSamples -= num_new_samples;
			samplePtr += (num_new_samples * audio_sample_size * audio_channels);
			audio_input_leftover_samples = 0;

			pkt.flags |= AV_PKT_FLAG_KEY;
			pkt.stream_index = audio_st->index;
			pkt.data = audio_outbuf;
			pkt.size = avcodec_encode_audio(c, audio_outbuf, audio_outbuf_size, samples);

			if (c->coded_frame && c->coded_frame->pts != AV_NOPTS_VALUE)
				pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, audio_st->time_base);

			if(av_interleaved_write_frame(oc, &pkt) != 0) {
				LOGE("Error while writing audio frame\n");
				return;
			}
		}
		else {
			// if we didn't have enough samples for a frame, we copy over however many we had and update audio_input_leftover_samples
			int num_new_samples = c->frame_size - audio_input_leftover_samples;
			if(numSamples < num_new_samples)
				num_new_samples = numSamples;

			memcpy((uint8_t *)samples + (audio_input_leftover_samples * audio_sample_size * audio_channels), samplePtr, num_new_samples * audio_sample_size * audio_channels);
			numSamples -= num_new_samples;
			samplePtr += (num_new_samples * audio_sample_size * audio_channels);
			audio_input_leftover_samples += num_new_samples;
		}
	}
}

void VideoRecorderImpl::SupplyVideoFrame(const void *frameData, unsigned long numBytes, unsigned long timestamp)
{
	if(!video_st) {
		LOGE("tried to SupplyVideoFrame when no video stream was present\n");
		return;
	}

	AVCodecContext *c = video_st->codec;

	memcpy(picture->data[0], frameData, numBytes);
	// Don't copy the frame unnecessarily! Simply point tmp_picture->data[0] to the incoming frame
	//tmp_picture->data[0]=(uint8_t*)frameData;

	// if the input pixel format is not YUV420P, we'll assume
	// it's stored in tmp_picture, so we'll convert it to YUV420P
	// and store it in "picture"
	// if it's already in YUV420P format we'll assume it's stored in
	// "picture" from before

	if(timestamp_base == 0)
		timestamp_base = timestamp;

	//picture->pts = 90 * (timestamp - timestamp_base);	// assuming millisecond timestamp and 90 kHz timebase
	LOGD("picture DATA:\n%s\n-----------------------\n",picture->data[0]);

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data =video_outbuf;
	pkt.size = 352*288*4;

    int got_packet=0;
	int ret=avcodec_encode_video2(c, &pkt, picture, &got_packet);
	LOGD("avcodec_encode_video returned %d\n", got_packet);

	if((!ret) && got_packet && c->coded_frame) {
			if (c->coded_frame->pts != AV_NOPTS_VALUE){
					c->coded_frame->key_frame = !!(pkt.flags & AV_PKT_FLAG_KEY);
			}
			if(av_interleaved_write_frame(oc, &pkt) != 0) {
					LOGE("Unable to write video frame\n");
					return;
			}
	}
}

VideoRecorder* VideoRecorder::New()
{
	return (VideoRecorder*)(new VideoRecorderImpl);
}
int native_recorder_open(){
	recorder = new VideoRecorderImpl();
	recorder->SetAudioOptions(AudioSampleFormatS16, 2, 44100, 64000);
	recorder->SetVideoOptions(VideoFrameFormatYUV420P, 352, 288, 400000);
	char filename[512], timenow[100];
	time_t now = time(0);
	strftime(timenow, 100, "%Y-%m-%d-%H-%M-%S", localtime (&now));
	sprintf(filename, "/sdcard/xxx-%s.flv", timenow);
	recorder->Open(filename, false, true);
	isRecording=1;
	frameCnt=0;
	return 0;
}
int native_recorder_encode_video(JNIEnv *env, jobject thiz, jbyteArray array){

	jbyte* data = env->GetByteArrayElements(array, NULL);
	jsize length = env->GetArrayLength(array);

	LOGD("encoder get %d bytes \n", length);
	//LOGD("encoder get %s bytes \n", data);
	 //640*480*2
	recorder->SupplyVideoFrame( data, length, (25 * frameCnt)+1);
	frameCnt++;
	delete data;
	return 0;
}
int native_recorder_encode_sound(JNIEnv *env, jobject thiz, jbyteArray array){
	jbyte* data = env->GetByteArrayElements(array, NULL);
	jsize length = env->GetArrayLength(array);
	recorder->SupplyAudioSamples(sound_buffer, length);
	return 0;
}
int native_recorder_close(){
	delete sound_buffer;

	recorder->Close();
	LOGD("CLOSE");

	delete recorder;
	return 0;
}
static JNINativeMethod gMethods[] = {
    {"native_recorder_open", "()I", (void *)native_recorder_open},
    {"native_recorder_encode_video", "([B)I", (void *)native_recorder_encode_video},
    {"native_recorder_encode_sound", "([B)I", (void *)native_recorder_encode_sound},
    {"native_recorder_close", "()I", (void *)native_recorder_close},
};

int register_native_methods(JNIEnv *env)
{
	return jniRegisterNativeMethods(env, "pku/shengbin/hevrecorder/RecordingActivity", gMethods, sizeof(gMethods) / sizeof(gMethods[0]));
}

