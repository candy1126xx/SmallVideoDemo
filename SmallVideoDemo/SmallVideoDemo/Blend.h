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
#include <opencv2/opencv.hpp>
#include "PerspectiveTransform.h"
#include "Convert.h"
#include "ReadMocha.h"

using namespace cv;
using namespace std;

class BlendUtil
{
public:
	AVFrame* OnFrame(int frameIndex, AVFrame *originFrame, AVFrame *maskFrame, AVFrame *resultFrame);
	BlendUtil(string p, ReadMochaUtil *r);
	~BlendUtil();

private:
	void Transform();
	void Mask(int frameIndex);
	ConvertUtil convertUtil;
	TransformUtil transformUtil;
	ReadMochaUtil *readMochaUtil;
	string picPath;
	Point2f LT, RT, LB, RB, originOrigin, maskOrigin;
	Mat originMat, maskMat, picMat;
};