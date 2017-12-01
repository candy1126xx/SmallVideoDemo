#pragma once

#include "ReadMocha.h"

void ReadMochaUtil::Read(string mochaPath)
{
	ifstream infile;
	infile.open(mochaPath);

	string s;
	int pin;
	int a, b, c, d;
	while (getline(infile, s))
	{
		a = s.find("Pin-0001");
		b = s.find("Pin-0002");
		c = s.find("Pin-0003");
		d = s.find("Pin-0004");
		if (a > 0)
		{
			pin = 0;
		}
		else if (b > 0)
		{
			pin = 1;
		}
		else if (c > 0)
		{
			pin = 2;
		}
		else if (d > 0)
		{
			pin = 3;
		}
		else
		{
			string buff{ "" };
			vector<float> v;
			float i;
			for (auto n : s)
			{
				if (n != '\t')
				{
					buff += n;
				}
				else if(n == '\t' && buff != "")
				{ 
					stringstream ss;
					ss << buff;
					ss >> i;
					v.push_back(i); 
					buff = "";
				}
			}
			if (buff != "")
			{
				stringstream ss;
				ss << buff;
				ss >> i;
				v.push_back(i);
			}
			list[pin].push_back(Point2f(v[1], v[2]));
		}
	}

	infile.close();

	startIndex = 304;
	endIndex = 393;
}

void ReadMochaUtil::GetFramePoint(int frameIndex, Point2f *LT, Point2f *RT, Point2f *LB, Point2f *RB)
{
	int i = frameIndex - startIndex;
	*LT = list[0][i];
	*RT = list[1][i];
	*LB = list[2][i];
	*RB = list[3][i];
}

bool ReadMochaUtil::HasPoint(int frameIndex)
{
	return frameIndex >= startIndex && frameIndex <= endIndex;
}

ReadMochaUtil::ReadMochaUtil()
{
}

ReadMochaUtil::~ReadMochaUtil()
{
}