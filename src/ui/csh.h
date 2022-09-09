#pragma once

struct queueParam {
	int param_id;
	float value;
	struct queueParam *previousInLine;
	struct queueParam *nextInLine;
};

struct queueParam *newQueueParam(const int param_id, const float value);

