#pragma once

#include "Convert.h"

using namespace cv;

void ConvertUtil::AVFrame2Mat(AVFrame *frame, Mat *mat)
{
	AVFrame *dst;
	enum AVPixelFormat src_pixfmt = AV_PIX_FMT_YUV420P;
	enum AVPixelFormat dst_pixfmt = AV_PIX_FMT_BGR24;
	int w = frame->width, h = frame->height;
	int size = avpicture_get_size(dst_pixfmt, w, h);

	dst = av_frame_alloc();
	uint8_t *out_buffer;
	out_buffer = new uint8_t[avpicture_get_size(dst_pixfmt, w, h)];
	avpicture_fill((AVPicture *)dst, out_buffer, dst_pixfmt, w, h);
	
	SwsContext *AVFrame2MatCtx = sws_getContext(w, h, src_pixfmt, w, h, dst_pixfmt,
		SWS_BICUBIC, NULL, NULL, NULL);
	sws_scale(AVFrame2MatCtx, frame->data, frame->linesize, 0, h,
		dst->data, dst->linesize);

	*mat = cv::Mat(h, w, CV_8UC3);
	memcpy((uint8_t *)(mat->data), dst->data[0], size);

	sws_freeContext(AVFrame2MatCtx);
}

void ConvertUtil::Mat2AVFrame(Mat *mat, AVFrame *resultframe)
{
	AVFrame *src;
	enum AVPixelFormat src_pixfmt = AV_PIX_FMT_BGR24;
	enum AVPixelFormat dst_pixfmt = AV_PIX_FMT_YUV420P;
	int w = mat->size().width, h = mat->size().height;
	int size = avpicture_get_size(src_pixfmt, w, h);

	src = av_frame_alloc();
	uint8_t *src_buffer;
	src_buffer = new uint8_t[avpicture_get_size(src_pixfmt, w, h)];
	avpicture_fill((AVPicture *)src, src_buffer, src_pixfmt, w, h);
	uint8_t *dst_buffer;
	dst_buffer = new uint8_t[avpicture_get_size(dst_pixfmt, w, h)];
	avpicture_fill((AVPicture *)resultframe, dst_buffer, dst_pixfmt, w, h);

	memcpy(src->data[0], (uint8_t *)mat->data, size);

	SwsContext *Mat2AVFrameCtx = sws_getContext(w, h, src_pixfmt, w, h, dst_pixfmt,
		SWS_BICUBIC, NULL, NULL, NULL);
	sws_scale(Mat2AVFrameCtx, src->data, src->linesize, 0, h,
		resultframe->data, resultframe->linesize);
	resultframe->width = w;
	resultframe->height = h;

	sws_freeContext(Mat2AVFrameCtx);
}

ConvertUtil::ConvertUtil()
{
}

ConvertUtil::~ConvertUtil()
{
}