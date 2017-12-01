#include<iostream>
#include<string>
#include<opencv2/opencv.hpp>
using namespace std;

// 跟踪四点坐标
static float LT[2] = { 0, 50 };
static float RT[2] = { 300, 30 };
static float LB[2] = { 50, 400 };
static float RB[2] = { 400, 200 };
// 原图路径
static string originPath = "pic.jpg";
// 结果图路径
static string resultPath = "result.jpg";

static int max(float a, float b)
{
	return (int)(a < b ? b : a);
}

static int min(float a, float b)
{
	return (int)(a > b ? b : a);
}

// 计算结果图在视频帧中的宽高原点
static void getResultSize(CvSize *size, CvPoint2D32f *origin)
{
	size->width = max(RT[0], RB[0]) - min(LT[0], LB[0]);
	size->height = max(LB[1], RB[1]) - min(LT[1], RT[1]);
	origin->x = min(LT[0], LB[0]);
	origin->y = min(LT[1], RT[1]);
}

// 计算结果图中图像四点坐标
static void getResultPoints(CvPoint2D32f dstTri[4], CvPoint2D32f *resultOrigin)
{
	dstTri[0] = CvPoint2D32f{ LT[0] - resultOrigin->x, LT[1] - resultOrigin->y };
	dstTri[1] = CvPoint2D32f{ RT[0] - resultOrigin->x, RT[1] - resultOrigin->y };
	dstTri[2] = CvPoint2D32f{ LB[0] - resultOrigin->x, LB[1] - resultOrigin->y };
	dstTri[3] = CvPoint2D32f{ RB[0] - resultOrigin->x, RB[1] - resultOrigin->y };
}

int main()
{
	//数组声明
	CvPoint2D32f srcTri[4], dstTri[4];
	//创建数组指针
	CvMat *warp_mat = cvCreateMat(3, 3, CV_32FC1);

	IplImage *src, *dst;
	//载入图像
	src = cvLoadImage(originPath.data(), CV_LOAD_IMAGE_UNCHANGED);
    // 创建输出图像
	CvSize resultSize;
	CvPoint2D32f resultOrigin;
	getResultSize(&resultSize, &resultOrigin);
	dst = cvCreateImage(resultSize, src->depth, src->nChannels);
	dst->origin = src->origin;
	cvZero(dst);
	//构造变换矩阵
	srcTri[0].x = 0;
	srcTri[0].y = 0;
	srcTri[1].x = src->width;
	srcTri[1].y = 0;
	srcTri[2].x = 0;
	srcTri[2].y = src->height;
	srcTri[3].x = src->width;
	srcTri[3].y = src->height;

	getResultPoints(dstTri, &resultOrigin);
	//计算透视映射矩阵
	cvGetPerspectiveTransform(srcTri, dstTri, warp_mat);
	//调用函数cvWarpPerspective（）
	cvWarpPerspective(src, dst, warp_mat);
	//保存结果图
	cvSaveImage(resultPath.data(), dst);

	cvReleaseImage(&src);
	cvReleaseImage(&dst);
	cvReleaseMat(&warp_mat);

	return 0;

}