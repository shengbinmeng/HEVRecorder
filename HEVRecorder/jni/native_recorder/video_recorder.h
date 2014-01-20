#ifndef __VIDEO_RECORDER_H__
#define __VIDEO_RECORDER_H__

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
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
	bool setVideoOptions(VideoFrameFormat fmt, int width, int height, unsigned long bitrate);
	bool setAudioOptions(AudioSampleFormat fmt, int channels, unsigned long samplerate, unsigned long bitrate);

	bool open(const char* file, bool hasAudio);

	bool close();

	// After this succeeds, you can call SupplyVideoFrame and SupplyAudioSamples
	bool start();

	// Supply a video frame
	void supplyVideoFrame(const void* frame, unsigned long numBytes, unsigned long timestamp);

	// Supply audio samples
	void supplyAudioSamples(const void* samples, unsigned long numSamples);

private:
	AVStream *add_audio_stream(enum AVCodecID codec_id);
	void open_audio();
	void write_audio_frame(AVStream *st);

	AVStream *add_video_stream(enum AVCodecID codec_id);
	AVFrame *alloc_frame(enum PixelFormat pix_fmt, int width, int height);
	void open_video();
	void write_video_frame(AVStream *st);

	// audio related vars
	AVStream *audio_st;
	AVFrame *audio_frame;
	AVPacket audio_pkt;

	int16_t *samples;
	uint8_t *audio_pkt_buf;
	int audio_pkt_buf_size;
	int audio_input_frame_size;

	unsigned long audio_input_leftover_samples;

	int audio_channels;				// number of channels (2)
	unsigned long audio_bit_rate;		// codec's output bitrate
	unsigned long audio_sample_rate;		// number of samples per second
	int audio_sample_size;					// size of each sample in bytes (16-bit = 2)
	AVSampleFormat audio_sample_format;

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
	SwsContext *img_convert_ctx;

	unsigned long timestamp_base;

	// common
	AVFormatContext *oc;
	AVDictionary* pAVDictionary = NULL;
	uint8_t *frame_buf;
};


#endif
