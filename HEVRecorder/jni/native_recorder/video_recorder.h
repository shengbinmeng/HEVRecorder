#ifndef __VIDEO_RECORDER_H__
#define __VIDEO_RECORDER_H__

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/error.h>
}

enum VideoFrameFormat {
	VideoFrameFormatYUV420P=0,
	VideoFrameFormatNV12,
	VideoFrameFormatNV21,
	VideoFrameFormatRGB24,
	VideoFrameFormatBGR24,
	VideoFrameFormatARGB,
	VideoFrameFormatRGBA,
	VideoFrameFormatABGR,
	VideoFrameFormatBGRA,
	VideoFrameFormatRGB565LE,
	VideoFrameFormatRGB565BE,
	VideoFrameFormatBGR565LE,
	VideoFrameFormatBGR565BE,
	VideoFrameFormatMax
};

enum AudioSampleFormat {
	AudioSampleFormatU8=0,
	AudioSampleFormatS16,
	AudioSampleFormatS32,
	AudioSampleFormatFLT,
	AudioSampleFormatDBL,
	AudioSampleFormatMax
};

class VideoRecorder {
public:
	VideoRecorder();
	virtual ~VideoRecorder();

	// Return true on success, false on failure

	// Call these first
	int setVideoOptions(VideoFrameFormat fmt, int width, int height, unsigned long bitrate);
	int setAudioOptions(AudioSampleFormat fmt, int channels, unsigned long samplerate, unsigned long bitrate);

	int open(const char* file, bool hasAudio);

	int close();

	// Supply a video frame
	int supplyVideoFrame(const void* frame, unsigned long numBytes, unsigned long timestamp);

	// Supply audio samples
	int supplyAudioSamples(const void* samples, unsigned long numSamples);

private:
	// audio related vars
	AVStream *audio_st;
	AVFrame *audio_frame;
	AVPacket audio_pkt;
	uint8_t *audio_pkt_buf;
	int audio_pkt_buf_size;

	void *samples;
	uint8_t **dst_samples;
	int dst_samples_size;
	int max_dst_nb_samples;
	unsigned long samples_count;
	int audio_input_frame_size;
	unsigned long audio_input_leftover_samples;

	int audio_channels;				// number of channels (2)
	unsigned long audio_bit_rate;		// codec's output bitrate
	unsigned long audio_sample_rate;		// number of samples per second
	int audio_sample_size;					// size of each sample in bytes (16-bit = 2)
	AVSampleFormat audio_sample_format;
	SwrContext *swr_ctx;

	// video related vars
	AVStream *video_st;
	AVFrame *video_frame; // video frame after being converted to desired format, e.g. YUV420P
	AVPacket video_pkt;
	uint8_t *video_pkt_buf;
	int video_pkt_buf_size;

	int video_width;
	int video_height;
	unsigned long video_bitrate;
	PixelFormat video_pixfmt;
	unsigned long timestamp_start;

	// common
	AVFormatContext *oc;
	AVDictionary* pAVDictionary = NULL;

	AVStream *add_audio_stream(enum AVCodecID codec_id);
	int open_audio();

	AVStream *add_video_stream(enum AVCodecID codec_id);
	AVFrame *alloc_frame(enum PixelFormat pix_fmt, int width, int height);
	int open_video();
};


#endif
