#pragma once

extern "C"
{
#include "libswscale/swscale.h"
#include "libavutil/samplefmt.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
}
#include <opencv2/opencv.hpp>

using namespace cv;

class ConvertUtil
{
public:
	void AVFrame2Mat(AVFrame *frame, Mat *mat);
	void Mat2AVFrame(Mat *mat, AVFrame *frame);
	ConvertUtil();
	~ConvertUtil();
private:
	
};