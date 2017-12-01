#include<string>
#include<opencv2/opencv.hpp>
#include "PerspectiveTransform.h"
#include "DDEM.h"
#include "Blend.h"
#include "ReadMocha.h"

static string picPath = "material/pic.jpg";
static string originVideoPath = "material/test.mp4";
static string maskVideoPath = "material/mask.mp4";
static string resultVideoPath = "material/result.mp4";
static string mochaPointPath = "material/mochaPoint.txt";

int main()
{
	ReadMochaUtil readMochaUtil;
	readMochaUtil.Read(mochaPointPath);
	BlendUtil blendUtil(picPath, &readMochaUtil);
	VideoProgress videoProgress(originVideoPath, maskVideoPath, resultVideoPath, &blendUtil);
	videoProgress.Start();
}