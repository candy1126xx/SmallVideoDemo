#pragma once

#include "Blend.h"

using namespace std;

// Í¸ÊÓ±ä»»
void BlendUtil::Transform()
{
	CvPoint2D32f points[4] = { LT, RT, LB, RB };
	transformUtil.SetParameters(points, picPath);
	transformUtil.Calculate();
	transformUtil.CreateResultImage(&picMat);
}

// ÑÚÄ£
void BlendUtil::Mask(int frameIndex)
{
	Point2f origin = *(transformUtil.GetResultOrigin());
	int width = picMat.size().width;
	int height = picMat.size().height;
	Mat originROI;
	originROI = originMat(Rect(origin.x, origin.y, width, height));
	Mat maskROI;
	maskROI = maskMat(Rect(origin.x, origin.y, width, height));
	picMat.copyTo(originROI, maskROI);
}

AVFrame* BlendUtil::OnFrame(int frameIndex, AVFrame *originFrame, AVFrame *maskFrame, AVFrame *resultFrame)
{
	if (readMochaUtil->HasPoint(frameIndex))
	{
		convertUtil.AVFrame2Mat(originFrame, &originMat);
		convertUtil.AVFrame2Mat(maskFrame, &maskMat);
		readMochaUtil->GetFramePoint(frameIndex, &LT, &RT, &LB, &RB);
		Transform();
		Mask(frameIndex);
		convertUtil.Mat2AVFrame(&originMat, resultFrame);
		resultFrame->format = originFrame->format;
		resultFrame->pict_type = originFrame->pict_type;
		av_frame_set_pkt_pos(resultFrame, originFrame->pkt_pos);
		av_frame_set_pkt_duration(resultFrame, originFrame->pkt_duration);
		av_frame_set_pkt_size(resultFrame, originFrame->pkt_size);
		return resultFrame;
	}
	else
	{
		return originFrame;
	}
}

BlendUtil::BlendUtil(string p, ReadMochaUtil *r)
{
	picPath = p;
	readMochaUtil = r;
}

BlendUtil::~BlendUtil()
{
}