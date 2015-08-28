/*
 * hls_video_recorder.cpp
 *
 *  Created on: Dec 25, 2014
 *      Author: shengbin
 */

#include "hls_video_recorder.h"
#include "jni_utils.h"

#define LOG_TAG "HlsVideoRecorder"

#define SERVER_IP "192.168.3.102"
#define SERVER_PORT 9898
#define FRAME_NUMBER_PER_SEGMENT 24

HlsVideoRecorder::HlsVideoRecorder()
{
	tsFileCount = 0;
	sendFileCount = 0;
	segmentFrameCount = 0;
}

HlsVideoRecorder::~HlsVideoRecorder()
{

}

int HlsVideoRecorder::open(const char * path, bool hasAudio)
{
	LOGI("opening recorder \n");

	av_register_all();
	avcodec_register_all();

	tsFileCount = 0;
	strcpy(tsFilePrefix, path);
	sprintf(tsFile, "%s_%d.ts", tsFilePrefix, tsFileCount);
	char *file = tsFile;
	int ret = avformat_alloc_output_context2(&oc, NULL, NULL, file);
	if (!oc) {
		LOGE("alloc_output_context failed \n");
		return -1;
	}

	ret = tcpClient.openConnection(SERVER_IP, SERVER_PORT);
	if (ret < 0) {
		LOGE("open TCP connection failed \n");
		return -1;
	}

	video_st = add_video_stream(AV_CODEC_ID_HEVC);
	if (hasAudio) {
		audio_st = add_audio_stream(AV_CODEC_ID_AAC);
	}

	// dump format information (for debug purpose only)
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

int HlsVideoRecorder::close()
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

		avio_flush(oc->pb);
		tcpClient.sendFile(tsFile);
		tsFileCount++;
	}

	tcpClient.closeConnection();

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


int HlsVideoRecorder::new_segment()
{
	av_write_trailer(oc);
	avio_flush(oc->pb);
	avio_close(oc->pb);

	//TODO: We should send the file in another thread!
	int ret = tcpClient.sendFile(tsFile);
	if (ret < 0) {
		return ret;
	}
	LOGI("TS file %d is sent \n", tsFileCount);
	tsFileCount++;

	sprintf(tsFile, "%s_%d.ts", tsFilePrefix, tsFileCount);
	char *file = tsFile;

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

	return 0;
}

int HlsVideoRecorder::write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    // rescale output packet timestamp values from codec to stream timebase
    pkt->pts = av_rescale_q_rnd(pkt->pts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    pkt->dts = av_rescale_q_rnd(pkt->dts, *time_base, st->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    pkt->duration = av_rescale_q(pkt->duration, *time_base, st->time_base);
    pkt->stream_index = st->index;
    // write the compressed frame to the media file
    int ret = -1;
	pthread_mutex_lock(&write_mutex);
    ret = av_interleaved_write_frame(fmt_ctx, pkt);
	pthread_mutex_unlock(&write_mutex);

	if (st == video_st) {
		segmentFrameCount++;
		if (segmentFrameCount == FRAME_NUMBER_PER_SEGMENT) {
			LOGI("new segment \n");
			int ret = new_segment();
			if (ret < 0) {
				LOGE("new segment failed \n");
				return ret;
			}
			segmentFrameCount = 0;
		}
	}
	return ret;
}
