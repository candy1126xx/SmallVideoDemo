extern "C" 
{
#include "libavutil/samplefmt.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
}
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

using namespace std;

static string origin_path = "test.mp4";
static string yuv_path = "result.yuv";
static string h264_path = "result.h264";
static string output_path = "result.mp4";

static AVFormatContext *de_format_context;
static AVStream *de_video_stream;
static int de_video_stream_index;
static AVCodecContext *de_video_context;
static AVStream *de_audio_stream;
static int de_audio_stream_index;
static AVCodecContext *de_audio_context;
static AVFrame *de_frame;
static AVPacket de_packet;
static int de_got_frame;
static AVRational de_frame_rate;

static FILE *video_dst_file = NULL;
static uint8_t *video_dst_data[4] = { NULL };
static int video_dst_linesize[4];
static int video_dst_bufsize;

static FILE *h264_file = NULL;
static AVCodecContext *h264_codec_context;
static AVCodec *h264_codec;
static AVPacket h264_packet;

static AVFormatContext *en_format_context;
static AVStream *en_video_stream;
static AVCodecContext *en_video_context;
static AVStream *en_audio_stream;
static AVCodecContext *en_audio_context;
static AVCodec *en_video;
static AVCodec *en_audio;
static AVPacket en_packet;
static int en_got_packet;
static int video_frame_count;
static int audio_frame_count;

// �򿪽�����
// in  : fmt_ctx��type
// out : stream_idx��dec_ctx
static void open_codec_context(
	AVFormatContext *FormatContext,   // in  : �����ļ����װ������
	enum AVMediaType Type,            // in  : ��Ƶor��Ƶ
	int *StreamIndex,                  // out : �����
	AVStream **Stream,                // out : ��
	AVCodecContext **CodecContext)   // out : �����ļ�����������
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
	if (Type == AVMEDIA_TYPE_VIDEO)
		de_frame_rate = AVRational{ ((*Stream)->r_frame_rate).den, ((*Stream)->r_frame_rate).num };
}

// ������Ƶframe��ι��muxer�����յ�file
static void write_video_frame(
	AVFormatContext *FormatContext, // in  : ����ļ���װ������
	AVCodecContext *CodecContext,   // in  : ����ļ�����������
	AVFrame *Frame,                 // in  : ���������ݵĻ�����
	int *GotPacket,                 // in  : �������
	AVPacket *Packet,               // in  : �ѱ������ݵĻ�����
	AVStream *Stream)               // in  : ����ļ���Ƶ��
{
	// �����YUV
	av_image_copy(video_dst_data, video_dst_linesize,
		(const uint8_t **)(Frame->data), Frame->linesize,
		CodecContext->pix_fmt, CodecContext->width, CodecContext->height);
	fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);

	// �����H264
	*GotPacket = 0;
	avcodec_encode_video2(h264_codec_context, &h264_packet, Frame, GotPacket);
	if (*GotPacket) {
		fwrite(h264_packet.data, h264_packet.size, 1, h264_file);
		av_packet_unref(&h264_packet);
	}

	// �����MP4
	Frame->pts = video_frame_count++;
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

// ������Ƶframe��ι��muxer�����յ�file
static void write_audio_frame(
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
static int decode_packet(
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
			write_video_frame(en_format_context, en_video_context, Frame, &en_got_packet, &en_packet, en_video_stream);
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
static void open_encoder(
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
static void add_stream(
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

int main() 
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
	open_codec_context(de_format_context, AVMEDIA_TYPE_VIDEO, &de_video_stream_index, &de_video_stream, &de_video_context);
	// ����Ƶ������
	open_codec_context(de_format_context, AVMEDIA_TYPE_AUDIO, &de_audio_stream_index, &de_audio_stream, &de_audio_context);
	// �������frame
	de_frame = av_frame_alloc();
	// ��ʼ������packet
	av_init_packet(&de_packet);
	//------------------------------------------�����ʼ��

	//------------------------------------------YUV��ʼ��
	video_dst_file = fopen(yuv_path.data(), "wb");
	video_dst_bufsize = av_image_alloc(video_dst_data, video_dst_linesize, de_video_context->width, de_video_context->height, de_video_context->pix_fmt, 1);
	//------------------------------------------YUV��ʼ��

	//------------------------------------------H264��ʼ��
	h264_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	h264_codec_context = avcodec_alloc_context3(h264_codec);
	avcodec_parameters_to_context(h264_codec_context, de_video_stream->codecpar);
	h264_codec_context->time_base = de_video_stream->time_base;
	h264_codec_context->gop_size = de_video_context->gop_size;
	h264_codec_context->pix_fmt = de_video_context->pix_fmt;
	avcodec_open2(h264_codec_context, h264_codec, NULL);
	h264_file = fopen(h264_path.data(), "wb");
	av_init_packet(&h264_packet);
	//------------------------------------------H264��ʼ��

	//------------------------------------------MP4��ʼ��
	// �����װ������
	avformat_alloc_output_context2(&en_format_context, NULL, NULL, output_path.data());
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
	avio_open(&en_format_context->pb, output_path.data(), AVIO_FLAG_WRITE);
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
	do {
		avcodec_encode_video2(h264_codec_context, &h264_packet, NULL, &en_got_packet);
		if (en_got_packet) {
			fwrite(h264_packet.data, 1, h264_packet.size, h264_file);
			av_free_packet(&h264_packet);
		}
	} while (en_got_packet);

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

	fclose(video_dst_file);
	av_free(video_dst_data[0]);

	fclose(h264_file);
	avcodec_close(h264_codec_context);
	av_free(h264_codec_context);

	avcodec_free_context(&en_video_context);
	avcodec_free_context(&en_audio_context);
	avio_closep(&en_format_context->pb);
	avformat_free_context(en_format_context);

	return ret;
}