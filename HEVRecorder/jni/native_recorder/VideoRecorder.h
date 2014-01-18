#ifndef _AVR_VIDEORECORDER_H_
#define _AVR_VIDEORECORDER_H_

// Encodes video to H.264
// Encodes audio to AAC-LC
// Outputs to MP4 file
#define UINT64_MAX    0xffffffffffffffffU
#define UINT64_C(x) ((x) + (UINT64_MAX - UINT64_MAX))

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

	// Use this to get an instance of VideoRecorder. Use delete operator to delete it.
	static VideoRecorder* New();

	// Return true on success, false on failure

	// Call these first
	virtual bool SetVideoOptions(VideoFrameFormat fmt,int width,int height,unsigned long bitrate)=0;
	virtual bool SetAudioOptions(AudioSampleFormat fmt,int channels,unsigned long samplerate,unsigned long bitrate)=0;

	// Call after SetVideoOptions/SetAudioOptions
	virtual bool Open(const char* mp4file,bool hasAudio,bool dbg)=0;
	// Call last
	virtual bool Close()=0;

	// After this succeeds, you can call SupplyVideoFrame and SupplyAudioSamples
	virtual bool Start()=0;

	// Supply a video frame
	virtual void SupplyVideoFrame(const void* frame,unsigned long numBytes,unsigned long timestamp)=0;
	// Supply audio samples
	virtual void SupplyAudioSamples(const void* samples,unsigned long numSamples)=0;
};


#endif // _AVR_VIDEORECORDER_H_
