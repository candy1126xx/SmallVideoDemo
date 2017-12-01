#pragma once

#include<iostream>
#include<string>
#include<opencv2/opencv.hpp>

using namespace std;
using namespace cv;

class TransformUtil
{
public:
	void SetParameters(CvPoint2D32f points[4], string picPath);
	void Calculate();
	void CreateResultImage(Mat *resultMat);
	CvPoint2D32f* GetResultOrigin();
	TransformUtil();
	~TransformUtil();

private:
	string path;
	CvPoint2D32f pointLT, pointRT, pointLB, pointRB; // �����ĵ�����
	CvSize resultSize;
	CvPoint2D32f resultOrigin;                       // �任���ͼ����ԭ��Ƶ֡�е�ԭ������
	CvPoint2D32f srcTri[4], dstTri[4];
	CvMat *warp_mat;
	IplImage *src, *dst;
	Mat resultMat;
};