#pragma once

#include <opencv2/opencv.hpp>

using namespace cv;

struct FramePoint
{
	int frameIndex;
	Point2f point;
};

struct FramePointNode
{
	FramePoint val;
	FramePointNode *next;
	FramePointNode(FramePoint x) :val(x), next(NULL) {}
};

class FramePointList
{
public:
	void Insert(FramePoint val);
	FramePointList();
	~FramePointList();
private:
	FramePointNode *head;
	FramePointNode *current;
	int length;
};