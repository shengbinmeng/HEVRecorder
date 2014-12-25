/*
 * hls_video_recorder.h
 *
 *  Created on: Dec 25, 2014
 *      Author: shengbin
 */

#ifndef HLS_VIDEO_RECORDER_H_
#define HLS_VIDEO_RECORDER_H_

#include "video_recorder.h"
#include "tcp_client.h"

class HlsVideoRecorder : public VideoRecorder {
public:
	HlsVideoRecorder();
	virtual ~HlsVideoRecorder();

	virtual int open(const char *path, bool hasAudio);
	virtual int close();

private:
	TcpClient  tcpClient;
	char tsFilePrefix[1024];
	char tsFile[1024];
	int tsFileCount;
	int sendFileCount;
	int segmentFrameCount;

	virtual int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt);
	int new_segment();
};

#endif /* HLS_VIDEO_RECORDER_H_ */
