#pragma once

#include "DDEM.h"

using namespace std;

void VideoProgress::open_codec_context(
	AVFormatContext *FormatContext,   // in  : 输入文件解封装上下文
	enum AVMediaType Type,            // in  : 视频or音频
	int *StreamIndex,                  // out : 流序号
	AVStream **Stream,                // out : 流
	AVCodecContext **CodecContext,
	AVRational *FrameRate)   // out : 输入文件解码上下文
{
	AVCodec *codec = NULL;
	// 获取流序号
	*StreamIndex = av_find_best_stream(FormatContext, Type, -1, -1, NULL, 0);
	// 获取codec
	*Stream = FormatContext->streams[*StreamIndex];
	codec = avcodec_find_decoder((*Stream)->codecpar->codec_id);
	// 获取codec_context
	*CodecContext = avcodec_alloc_context3(codec);
	// 把参数从stream复制给codec_context
	avcodec_parameters_to_context(*CodecContext, (*Stream)->codecpar);
	// 打开codec
	avcodec_open2(*CodecContext, codec, NULL);
	// 如果是视频，记录帧率；音频没有帧率
	if (FrameRate != NULL && Type == AVMEDIA_TYPE_VIDEO)
		*FrameRate = AVRational{ ((*Stream)->r_frame_rate).den, ((*Stream)->r_frame_rate).num };
}

// 编码视频frame，喂给muxer，最终到file
void VideoProgress::write_video_frame(
	AVFormatContext *FormatContext, // in  : 输出文件封装上下文
	AVCodecContext *CodecContext,   // in  : 输出文件编码上下文
	AVFrame *Frame,                 // in  : 待编码数据的缓冲区
	int *GotPacket,                 // in  : 编码计数
	AVPacket *Packet,               // in  : 已编码数据的缓冲区
	AVStream *Stream)               // in  : 输出文件视频流
{
	// 修改Frame
	int index = video_frame_count++;
	AVFrame *modify_frame = blend_util->OnFrame(index, Frame, mask_frame, blend_frame);
	// 输出到MP4
	modify_frame->pts = index;
	*GotPacket = avcodec_send_frame(CodecContext, modify_frame);
	while (*GotPacket >= 0) {
		*GotPacket = avcodec_receive_packet(CodecContext, Packet);
		if (*GotPacket == AVERROR(EAGAIN) || *GotPacket == AVERROR_EOF)
			break;
		av_packet_rescale_ts(Packet, CodecContext->time_base, Stream->time_base);
		Packet->stream_index = Stream->index;
		av_interleaved_write_frame(FormatContext, Packet);
		av_packet_unref(Packet);
	}
}

// 编码音频frame，喂给muxer，最终到file
void VideoProgress::write_audio_frame(
	AVFormatContext *FormatContext, // in  : 输出文件封装上下文
	AVCodecContext *CodecContext,   // in  : 输出文件编码上下文
	AVFrame *Frame,                 // in  : 待编码数据的缓冲区
	int *GotPacket,                 // in  : 编码计数
	AVPacket *Packet,               // in  : 已编码数据的缓冲区
	AVStream *Stream)               // in  : 输出文件视频流
{
	// 输出到MP4
	Frame->pts = audio_frame_count++;
	*GotPacket = avcodec_send_frame(CodecContext, Frame);
	while (*GotPacket >= 0) {
		*GotPacket = avcodec_receive_packet(CodecContext, Packet);
		if (*GotPacket == AVERROR(EAGAIN) || *GotPacket == AVERROR_EOF)
			break;
		av_packet_rescale_ts(Packet, CodecContext->time_base, Stream->time_base);
		Packet->stream_index = Stream->index;
		av_interleaved_write_frame(FormatContext, Packet);
		av_packet_unref(Packet);
	}
}

// 解码一帧
// out : got_frame
int VideoProgress::decode_packet(
	AVCodecContext *VideoCodecContext,
	AVCodecContext *AudioCodecContext,
	AVPacket Packet,
	AVFrame *Frame,
	int *GotFrame)
{
	// 重置变量
	int decoded = Packet.size; // 一些音频解码器每次只解一部分，要解多次；另一些又不需要解多次；所以这个int标志是否完全解析
	*GotFrame = 0;

	if (Packet.stream_index == de_video_stream_index)
	{ // 如果这一帧是视频帧
		avcodec_decode_video2(VideoCodecContext, Frame, GotFrame, &Packet);
		if (*GotFrame)
		{ // 如果解码成功，修改de_frame，再编码输出到muxer
			int get_mask_frame = 0;
			do {
				av_packet_unref(&mask_packet);
				get_mask_frame = av_read_frame(mask_format_context, &mask_packet);
			} while (get_mask_frame >= 0 && mask_packet.stream_index != mask_video_stream_index);
			if (get_mask_frame >= 0)
			{
				avcodec_decode_video2(mask_video_context, mask_frame, &get_mask_frame, &mask_packet);
				if (get_mask_frame)
				{
					write_video_frame(en_format_context, en_video_context, Frame, &en_got_packet, &en_packet, en_video_stream);
				}
			}
		}
	}
	else if (Packet.stream_index == de_audio_stream_index)
	{ // 如果这一帧是音频帧，直接输出到muxer
		int size = avcodec_decode_audio4(AudioCodecContext, Frame, GotFrame, &Packet);
		decoded = size > de_packet.size ? de_packet.size : size;
		if (*GotFrame)
		{ // 如果解码成功，编码输出到muxer
			write_audio_frame(en_format_context, en_audio_context, Frame, &en_got_packet, &en_packet, en_audio_stream);
		}
	}
	return decoded;
}

// 打开编码器
void VideoProgress::open_encoder(
	enum AVCodecID CodecID,                  // in  : 编码器类型ID
	AVStream *OriginStream,                  // in  : 输入文件视频流或音频流
	AVCodecContext *OriginCodecContext,      // in  : 输入文件解码上下文
	AVCodec **Codec,                         // out : 输出文件编码器
	AVCodecContext **CodecContext)           // out : 输出文件编码上下文
{
	/* 获取编码器 */
	*Codec = avcodec_find_encoder(CodecID);
	/* 创建编码上下文 */
	*CodecContext = avcodec_alloc_context3(*Codec);
	/* 设置编码器上下文 */
	// 这一步的主要目的是赋值extardata
	avcodec_parameters_to_context(*CodecContext, OriginStream->codecpar);
	// CodecContext的time_base必须是（1，帧率）；虽然解码时获取不到音频的帧率，但这里必须给音频设置和视频同样的帧率，否则播放时时间轴不对
	(*CodecContext)->time_base = de_frame_rate;
	switch ((*Codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		(*CodecContext)->sample_fmt = OriginCodecContext->sample_fmt;
		(*CodecContext)->bit_rate = OriginCodecContext->bit_rate;
		(*CodecContext)->sample_rate = OriginCodecContext->sample_rate;
		(*CodecContext)->channels = OriginCodecContext->channels;
		(*CodecContext)->channel_layout = OriginCodecContext->channel_layout;
		break;
	case AVMEDIA_TYPE_VIDEO:
		(*CodecContext)->codec_id = OriginCodecContext->codec_id;
		(*CodecContext)->bit_rate = OriginCodecContext->bit_rate;
		(*CodecContext)->width = OriginCodecContext->width;
		(*CodecContext)->height = OriginCodecContext->height;
		(*CodecContext)->gop_size = OriginCodecContext->gop_size;
		(*CodecContext)->pix_fmt = OriginCodecContext->pix_fmt;
		break;
	}
	(*CodecContext)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	/* 打开encoder */
	avcodec_open2(*CodecContext, *Codec, NULL);
}

// 添加输出文件的流
void VideoProgress::add_stream(
	AVFormatContext *FormatContext,          // in  : 输出文件封装上下文
	AVCodec **Codec,                         // in : 输出文件编码器
	AVCodecContext **CodecContext,           // in : 输出文件编码上下文
	AVStream **Stream)                       // out : 输出文件视频流或音频流
{
	// 创建流信息
	*Stream = avformat_new_stream(FormatContext, *Codec);
	// 设置流信息
	avcodec_parameters_from_context((*Stream)->codecpar, *CodecContext);
	(*Stream)->id = FormatContext->nb_streams - 1;
	// Stream的time_base不能是（0，0），因为（0，0）不能参与计算；也不能太小，因为精度和类型限制；
	// 其余任何值都可以
	(*Stream)->time_base = AVRational{ 1, 1 };
}

void VideoProgress::Start()
{
	int ret;
	// 注册
	av_register_all();

	//------------------------------------------解码初始化
	// 打开原视频文件
	avformat_open_input(&de_format_context, origin_path.data(), NULL, NULL);
	// 获取流信息
	avformat_find_stream_info(de_format_context, NULL);
	// 打开视频解码器
	open_codec_context(de_format_context, AVMEDIA_TYPE_VIDEO, &de_video_stream_index, &de_video_stream, &de_video_context, &de_frame_rate);
	// 打开音频解码器
	open_codec_context(de_format_context, AVMEDIA_TYPE_AUDIO, &de_audio_stream_index, &de_audio_stream, &de_audio_context, NULL);
	// 申请解码frame
	de_frame = av_frame_alloc();
	blend_frame = av_frame_alloc();
	// 初始化解码packet
	av_init_packet(&de_packet);
	//------------------------------------------解码初始化

	//------------------------------------------mask解码初始化
	avformat_open_input(&mask_format_context, mask_path.data(), NULL, NULL);
	avformat_find_stream_info(mask_format_context, NULL);
	open_codec_context(mask_format_context, AVMEDIA_TYPE_VIDEO, &mask_video_stream_index, &mask_video_stream, &mask_video_context, NULL);
	mask_frame = av_frame_alloc();
	av_init_packet(&mask_packet);
	//------------------------------------------mask解码初始化

	//------------------------------------------MP4初始化
	// 申请封装上下文
	avformat_alloc_output_context2(&en_format_context, NULL, NULL, result_path.data());
	// 打开编码器
	open_encoder(en_format_context->oformat->video_codec, de_video_stream, de_video_context, &en_video, &en_video_context);
	open_encoder(en_format_context->oformat->audio_codec, de_audio_stream, de_audio_context, &en_audio, &en_audio_context);
	// 添加stream；之所以要先开编码器再加流，是因为开编码器后，CodecContext的flag将被赋值；否则会花屏
	add_stream(en_format_context, &en_video, &en_video_context, &en_video_stream);
	add_stream(en_format_context, &en_audio, &en_audio_context, &en_audio_stream);
	// 初始化编码packet
	av_init_packet(&en_packet);
	video_frame_count = 0;
	audio_frame_count = 0;
	// 打开output_file
	avio_open(&en_format_context->pb, result_path.data(), AVIO_FLAG_WRITE);
	// 写入stream_header
	avformat_write_header(en_format_context, NULL);
	//------------------------------------------MP4初始化

	//------------------------------------------工作循环
	// 循环读取源文件到de_packet
	while (av_read_frame(de_format_context, &de_packet) >= 0) {
		// 循环解码de_packet，直到size == 0
		do {
			ret = decode_packet(de_video_context, de_audio_context, de_packet, de_frame, &de_got_frame);
			if (ret < 0)
				break;
			// 解码了一部分后，data指针向后移，未解码的size减少
			de_packet.data += ret;
			de_packet.size -= ret;
		} while (de_packet.size > 0);
		// 所有packet都处理完了，解引用
		av_packet_unref(&de_packet);
	}
	//------------------------------------------工作循环

	//------------------------------------------FLUSH
	av_packet_unref(&de_packet);
	do {
		decode_packet(de_video_context, de_audio_context, de_packet, de_frame, &de_got_frame);
	} while (de_got_frame);
	//------------------------------------------FLUSH

	//------------------------------------------写文件尾
	av_write_trailer(en_format_context);
	//------------------------------------------写文件尾

end:
	avcodec_free_context(&de_video_context);
	avcodec_free_context(&de_audio_context);
	avformat_close_input(&de_format_context);
	av_frame_free(&de_frame);
	av_frame_free(&blend_frame);

	avcodec_free_context(&en_video_context);
	avcodec_free_context(&en_audio_context);
	avio_closep(&en_format_context->pb);
	avformat_free_context(en_format_context);
}

VideoProgress::VideoProgress(string originVideoPath, string maskVideoPath, string resultVideoPath, BlendUtil *b)
{
	de_format_context = NULL;
	mask_format_context = NULL;
	origin_path = originVideoPath;
	mask_path = maskVideoPath;
	result_path = resultVideoPath;
	blend_util = b;
}

VideoProgress::~VideoProgress()
{
}