#pragma once

#include <iostream> 
#include <fstream>
#include <string>
#include <opencv2/opencv.hpp>
#include "FramePoint.h"

using namespace cv;
using namespace std;

class ReadMochaUtil
{
public:
	void Read(string mochaPath);
	void GetFramePoint(int frameIndex, Point2f *LT, Point2f *RT, Point2f *LB, Point2f *RB);
	bool HasPoint(int frameIndex);
	ReadMochaUtil();
	~ReadMochaUtil();

private:
	int startIndex;
	int endIndex;
	vector<Point2f> list[4];
};