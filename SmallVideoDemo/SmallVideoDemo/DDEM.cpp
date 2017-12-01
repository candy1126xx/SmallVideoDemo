#pragma once

#include "DDEM.h"

using namespace std;

void VideoProgress::open_codec_context(
	AVFormatContext *FormatContext,   // in  : �����ļ����װ������
	enum AVMediaType Type,            // in  : ��Ƶor��Ƶ
	int *StreamIndex,                  // out : �����
	AVStream **Stream,                // out : ��
	AVCodecContext **CodecContext,
	AVRational *FrameRate)   // out : �����ļ�����������
{
	AVCodec *codec = NULL;
	// ��ȡ�����
	*StreamIndex = av_find_best_stream(FormatContext, Type, -1, -1, NULL, 0);
	// ��ȡcodec
	*Stream = FormatContext->streams[*StreamIndex];
	codec = avcodec_find_decoder((*Stream)->codecpar->codec_id);
	// ��ȡcodec_context
	*CodecContext = avcodec_alloc_context3(codec);
	// �Ѳ�����stream���Ƹ�codec_context
	avcodec_parameters_to_context(*CodecContext, (*Stream)->codecpar);
	// ��codec
	avcodec_open2(*CodecContext, codec, NULL);
	// �������Ƶ����¼֡�ʣ���Ƶû��֡��
	if (FrameRate != NULL && Type == AVMEDIA_TYPE_VIDEO)
		*FrameRate = AVRational{ ((*Stream)->r_frame_rate).den, ((*Stream)->r_frame_rate).num };
}

// ������Ƶframe��ι��muxer�����յ�file
void VideoProgress::write_video_frame(
	AVFormatContext *FormatContext, // in  : ����ļ���װ������
	AVCodecContext *CodecContext,   // in  : ����ļ�����������
	AVFrame *Frame,                 // in  : ���������ݵĻ�����
	int *GotPacket,                 // in  : �������
	AVPacket *Packet,               // in  : �ѱ������ݵĻ�����
	AVStream *Stream)               // in  : ����ļ���Ƶ��
{
	// �޸�Frame
	int index = video_frame_count++;
	AVFrame *modify_frame = blend_util->OnFrame(index, Frame, mask_frame, blend_frame);
	// �����MP4
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

// ������Ƶframe��ι��muxer�����յ�file
void VideoProgress::write_audio_frame(
	AVFormatContext *FormatContext, // in  : ����ļ���װ������
	AVCodecContext *CodecContext,   // in  : ����ļ�����������
	AVFrame *Frame,                 // in  : ���������ݵĻ�����
	int *GotPacket,                 // in  : �������
	AVPacket *Packet,               // in  : �ѱ������ݵĻ�����
	AVStream *Stream)               // in  : ����ļ���Ƶ��
{
	// �����MP4
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

// ����һ֡
// out : got_frame
int VideoProgress::decode_packet(
	AVCodecContext *VideoCodecContext,
	AVCodecContext *AudioCodecContext,
	AVPacket Packet,
	AVFrame *Frame,
	int *GotFrame)
{
	// ���ñ���
	int decoded = Packet.size; // һЩ��Ƶ������ÿ��ֻ��һ���֣�Ҫ���Σ���һЩ�ֲ���Ҫ���Σ��������int��־�Ƿ���ȫ����
	*GotFrame = 0;

	if (Packet.stream_index == de_video_stream_index)
	{ // �����һ֡����Ƶ֡
		avcodec_decode_video2(VideoCodecContext, Frame, GotFrame, &Packet);
		if (*GotFrame)
		{ // �������ɹ����޸�de_frame���ٱ��������muxer
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
	{ // �����һ֡����Ƶ֡��ֱ�������muxer
		int size = avcodec_decode_audio4(AudioCodecContext, Frame, GotFrame, &Packet);
		decoded = size > de_packet.size ? de_packet.size : size;
		if (*GotFrame)
		{ // �������ɹ������������muxer
			write_audio_frame(en_format_context, en_audio_context, Frame, &en_got_packet, &en_packet, en_audio_stream);
		}
	}
	return decoded;
}

// �򿪱�����
void VideoProgress::open_encoder(
	enum AVCodecID CodecID,                  // in  : ����������ID
	AVStream *OriginStream,                  // in  : �����ļ���Ƶ������Ƶ��
	AVCodecContext *OriginCodecContext,      // in  : �����ļ�����������
	AVCodec **Codec,                         // out : ����ļ�������
	AVCodecContext **CodecContext)           // out : ����ļ�����������
{
	/* ��ȡ������ */
	*Codec = avcodec_find_encoder(CodecID);
	/* �������������� */
	*CodecContext = avcodec_alloc_context3(*Codec);
	/* ���ñ����������� */
	// ��һ������ҪĿ���Ǹ�ֵextardata
	avcodec_parameters_to_context(*CodecContext, OriginStream->codecpar);
	// CodecContext��time_base�����ǣ�1��֡�ʣ�����Ȼ����ʱ��ȡ������Ƶ��֡�ʣ�������������Ƶ���ú���Ƶͬ����֡�ʣ����򲥷�ʱʱ���᲻��
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
	/* ��encoder */
	avcodec_open2(*CodecContext, *Codec, NULL);
}

// �������ļ�����
void VideoProgress::add_stream(
	AVFormatContext *FormatContext,          // in  : ����ļ���װ������
	AVCodec **Codec,                         // in : ����ļ�������
	AVCodecContext **CodecContext,           // in : ����ļ�����������
	AVStream **Stream)                       // out : ����ļ���Ƶ������Ƶ��
{
	// ��������Ϣ
	*Stream = avformat_new_stream(FormatContext, *Codec);
	// ��������Ϣ
	avcodec_parameters_from_context((*Stream)->codecpar, *CodecContext);
	(*Stream)->id = FormatContext->nb_streams - 1;
	// Stream��time_base�����ǣ�0��0������Ϊ��0��0�����ܲ�����㣻Ҳ����̫С����Ϊ���Ⱥ��������ƣ�
	// �����κ�ֵ������
	(*Stream)->time_base = AVRational{ 1, 1 };
}

void VideoProgress::Start()
{
	int ret;
	// ע��
	av_register_all();

	//------------------------------------------�����ʼ��
	// ��ԭ��Ƶ�ļ�
	avformat_open_input(&de_format_context, origin_path.data(), NULL, NULL);
	// ��ȡ����Ϣ
	avformat_find_stream_info(de_format_context, NULL);
	// ����Ƶ������
	open_codec_context(de_format_context, AVMEDIA_TYPE_VIDEO, &de_video_stream_index, &de_video_stream, &de_video_context, &de_frame_rate);
	// ����Ƶ������
	open_codec_context(de_format_context, AVMEDIA_TYPE_AUDIO, &de_audio_stream_index, &de_audio_stream, &de_audio_context, NULL);
	// �������frame
	de_frame = av_frame_alloc();
	blend_frame = av_frame_alloc();
	// ��ʼ������packet
	av_init_packet(&de_packet);
	//------------------------------------------�����ʼ��

	//------------------------------------------mask�����ʼ��
	avformat_open_input(&mask_format_context, mask_path.data(), NULL, NULL);
	avformat_find_stream_info(mask_format_context, NULL);
	open_codec_context(mask_format_context, AVMEDIA_TYPE_VIDEO, &mask_video_stream_index, &mask_video_stream, &mask_video_context, NULL);
	mask_frame = av_frame_alloc();
	av_init_packet(&mask_packet);
	//------------------------------------------mask�����ʼ��

	//------------------------------------------MP4��ʼ��
	// �����װ������
	avformat_alloc_output_context2(&en_format_context, NULL, NULL, result_path.data());
	// �򿪱�����
	open_encoder(en_format_context->oformat->video_codec, de_video_stream, de_video_context, &en_video, &en_video_context);
	open_encoder(en_format_context->oformat->audio_codec, de_audio_stream, de_audio_context, &en_audio, &en_audio_context);
	// ���stream��֮����Ҫ�ȿ��������ټ���������Ϊ����������CodecContext��flag������ֵ������Ứ��
	add_stream(en_format_context, &en_video, &en_video_context, &en_video_stream);
	add_stream(en_format_context, &en_audio, &en_audio_context, &en_audio_stream);
	// ��ʼ������packet
	av_init_packet(&en_packet);
	video_frame_count = 0;
	audio_frame_count = 0;
	// ��output_file
	avio_open(&en_format_context->pb, result_path.data(), AVIO_FLAG_WRITE);
	// д��stream_header
	avformat_write_header(en_format_context, NULL);
	//------------------------------------------MP4��ʼ��

	//------------------------------------------����ѭ��
	// ѭ����ȡԴ�ļ���de_packet
	while (av_read_frame(de_format_context, &de_packet) >= 0) {
		// ѭ������de_packet��ֱ��size == 0
		do {
			ret = decode_packet(de_video_context, de_audio_context, de_packet, de_frame, &de_got_frame);
			if (ret < 0)
				break;
			// ������һ���ֺ�dataָ������ƣ�δ�����size����
			de_packet.data += ret;
			de_packet.size -= ret;
		} while (de_packet.size > 0);
		// ����packet���������ˣ�������
		av_packet_unref(&de_packet);
	}
	//------------------------------------------����ѭ��

	//------------------------------------------FLUSH
	av_packet_unref(&de_packet);
	do {
		decode_packet(de_video_context, de_audio_context, de_packet, de_frame, &de_got_frame);
	} while (de_got_frame);
	//------------------------------------------FLUSH

	//------------------------------------------д�ļ�β
	av_write_trailer(en_format_context);
	//------------------------------------------д�ļ�β

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