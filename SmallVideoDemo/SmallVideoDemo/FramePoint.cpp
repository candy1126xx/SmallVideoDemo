#pragma once

#include "FramePoint.h"

//插入结点  
void FramePointList::Insert(FramePoint val)
{
	FramePointNode *node = new FramePointNode(val);
	if (length == 0)
	{
		head = node;
		current = head;
	}
	else
	{
		node->next = current;
		current = node;
	}
	length++;
}

//构造函数  
FramePointList::FramePointList()
{
	head = NULL;
	length = 0;
}

//析构函数  
FramePointList::~FramePointList()
{
	FramePointNode *temp;
	for (int i = 0; i<length; i++)
	{
		temp = head;
		head = head->next;
		delete temp;
	}
}