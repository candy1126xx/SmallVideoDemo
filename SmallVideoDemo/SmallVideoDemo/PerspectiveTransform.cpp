#pragma once

#include "PerspectiveTransform.h"

using namespace std;
using namespace cv;

void TransformUtil::SetParameters(CvPoint2D32f points[4], string picPath)
{
	pointLT = points[0];
	pointRT = points[1];
	pointLB = points[2];
	pointRB = points[3];
	path = picPath;
}

void TransformUtil::Calculate()
{
	resultSize.width = max(pointRT.x, pointRB.x) - min(pointLT.x, pointLB.x);
	resultSize.height = max(pointLB.y, pointRB.y) - min(pointLT.y, pointRT.y);
	resultOrigin.x = min(pointLT.x, pointLB.x);
	resultOrigin.y = min(pointLT.y, pointRT.y);

	// Debug
	if (resultOrigin.x < 0)
	{
		resultOrigin.x = 0;
	}
	if (resultOrigin.y < 0)
	{
		resultOrigin.y = 0;
	}

	dstTri[0] = CvPoint2D32f{ pointLT.x - resultOrigin.x, pointLT.y - resultOrigin.y };
	dstTri[1] = CvPoint2D32f{ pointRT.x - resultOrigin.x, pointRT.y - resultOrigin.y };
	dstTri[2] = CvPoint2D32f{ pointLB.x - resultOrigin.x, pointLB.y - resultOrigin.y };
	dstTri[3] = CvPoint2D32f{ pointRB.x - resultOrigin.x, pointRB.y - resultOrigin.y };
}

void TransformUtil::CreateResultImage(Mat *resultMat)
{
	//载入图像
	src = cvLoadImage(path.data(), CV_LOAD_IMAGE_UNCHANGED);
	srcTri[0].x = 0;
	srcTri[0].y = 0;
	srcTri[1].x = src->width;
	srcTri[1].y = 0;
	srcTri[2].x = 0;
	srcTri[2].y = src->height;
	srcTri[3].x = src->width;
	srcTri[3].y = src->height;
	// 创建输出图像
	dst = cvCreateImage(resultSize, src->depth, src->nChannels);
	dst->origin = src->origin;
	cvZero(dst);
	// 计算透视矩阵
	warp_mat = cvCreateMat(3, 3, CV_32FC1);
	cvGetPerspectiveTransform(srcTri, dstTri, warp_mat);
	// 透视变换
	cvWarpPerspective(src, dst, warp_mat);
	// 转换(深拷贝)
	Mat temp;
	temp = cvarrToMat(dst);
	*resultMat = temp.clone();
	// 释放
	cvReleaseImage(&src);
	cvReleaseImage(&dst);
	cvReleaseMat(&warp_mat);
}

CvPoint2D32f* TransformUtil::GetResultOrigin()
{
	return &resultOrigin;
}

TransformUtil::TransformUtil()
{

}

TransformUtil::~TransformUtil()
{

}