#pragma once

extern "C"
{
#include "libavutil/samplefmt.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
}
#include <iostream>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Blend.h"

using namespace std;

class VideoProgress
{
public:
	void Start();
	VideoProgress(string originVideoPath, string maskVideoPath, string resultVideoPath, BlendUtil *b);
	~VideoProgress();

private:
	string origin_path;
	string mask_path;
	string result_path;

	BlendUtil *blend_util;
	AVFrame *blend_frame;

	AVFormatContext *de_format_context;
	AVStream *de_video_stream;
	int de_video_stream_index;
	AVCodecContext *de_video_context;
	AVStream *de_audio_stream;
	int de_audio_stream_index;
	AVCodecContext *de_audio_context;
	AVFrame *de_frame;
	AVPacket de_packet;
	int de_got_frame;
	AVRational de_frame_rate;

	AVFormatContext *mask_format_context;
	AVStream *mask_video_stream;
	int mask_video_stream_index;
	AVCodecContext *mask_video_context;
	AVFrame *mask_frame;
	AVPacket mask_packet;

    AVFormatContext *en_format_context;
	AVStream *en_video_stream;
	AVCodecContext *en_video_context;
	AVStream *en_audio_stream;
	AVCodecContext *en_audio_context;
	AVCodec *en_video;
	AVCodec *en_audio;
	AVPacket en_packet;
	int en_got_packet;
	int video_frame_count;
	int audio_frame_count;

	void open_codec_context(
		AVFormatContext *FormatContext,
		enum AVMediaType Type,
		int *StreamIndex,
		AVStream **Stream,
		AVCodecContext **CodecContext,
		AVRational *FrameRate);

	void write_video_frame(
		AVFormatContext *FormatContext,
		AVCodecContext *CodecContext,
		AVFrame *Frame,
		int *GotPacket,
		AVPacket *Packet,
		AVStream *Stream);

	void write_audio_frame(
		AVFormatContext *FormatContext,
		AVCodecContext *CodecContext,
		AVFrame *Frame,              
		int *GotPacket,            
		AVPacket *Packet,         
		AVStream *Stream);

	int decode_packet(
		AVCodecContext *VideoCodecContext,
		AVCodecContext *AudioCodecContext,
		AVPacket Packet,
		AVFrame *Frame,
		int *GotFrame);

	void open_encoder(
		enum AVCodecID CodecID,
		AVStream *OriginStream,
		AVCodecContext *OriginCodecContext,
		AVCodec **Codec,
		AVCodecContext **CodecContext);

	void add_stream(
		AVFormatContext *FormatContext, 
		AVCodec **Codec,                  
		AVCodecContext **CodecContext,
		AVStream **Stream);
};