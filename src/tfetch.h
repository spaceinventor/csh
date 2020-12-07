/*
 * tfetch.h
 *
 *  Created on: Nov 25, 2020
 *      Author: johan
 */

#ifndef SRC_TFETCH_H_
#define SRC_TFETCH_H_

#include <vmem/vmem.h>

/* Time fetch vmem must be defined elsewhere */
extern const vmem_t vmem_tfetch;

void tfetch_onehz(void);


#endif /* SRC_TFETCH_H_ */
