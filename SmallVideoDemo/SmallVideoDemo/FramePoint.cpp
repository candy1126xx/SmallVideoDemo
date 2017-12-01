#pragma once

#include "FramePoint.h"

//������  
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

//���캯��  
FramePointList::FramePointList()
{
	head = NULL;
	length = 0;
}

//��������  
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