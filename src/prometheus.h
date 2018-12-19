/*
 * prometheus_exporter.h
 *
 *  Created on: Aug 29, 2018
 *      Author: johan
 */

#ifndef SRC_PROMETHEUS_H_
#define SRC_PROMETHEUS_H_

void prometheus_clear(void);
void prometheus_add(char * str);
void prometheus_init(void);
void prometheus_close(void);

#endif /* SRC_PROMETHEUS_H_ */
