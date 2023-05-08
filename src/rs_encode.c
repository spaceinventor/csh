/*
 * rs-encode.c
 *
 * @author Phil-Karn
 * Copyright Feb 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 *
 *  Created on: Jul 5, 2018
 *      Author: johan
 */

#include <string.h>
#include "rs.h"

/* Portable C version */
void encode_rs_8(data_t *data, data_t *parity,int pad) {

/* The guts of the Reed-Solomon encoder, meant to be #included
 * into a function body with the following typedefs, macros and variables supplied
 * according to the code parameters:

 * data_t - a typedef for the data symbol
 * data_t data[] - array of NN-NROOTS-PAD and type data_t to be encoded
 * data_t parity[] - an array of NROOTS and type data_t to be written with parity symbols
 * NROOTS - the number of roots in the RS code generator polynomial,
 *          which is the same as the number of parity symbols in a block.
 *          Integer variable or literal.

 * NN - the total number of symbols in a RS block. Integer variable or literal.
 * PAD - the number of pad symbols in a block. Integer variable or literal.
 * ALPHA_TO - The address of an array of NN elements to convert Galois field
 *            elements in index (log) form to polynomial form. Read only.
 * INDEX_OF - The address of an array of NN elements to convert Galois field
 *            elements in polynomial form to index (log) form. Read only.
 * MODNN - a function to reduce its argument modulo NN. May be inline or a macro.
 * GENPOLY - an array of NROOTS+1 elements containing the generator polynomial in index form

 * The memset() and memmove() functions are used. The appropriate header
 * file declaring these functions (usually <string.h>) must be included by the calling
 * program.

 * Copyright 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */

  int i, j;
  data_t feedback;

  memset(parity,0,NROOTS*sizeof(data_t));

  for(i=0;i<NN-NROOTS-PAD;i++){
    feedback = INDEX_OF[TalToConventional[data[i]] ^ parity[0]];
    if(feedback != A0){      /* feedback term is non-zero */
#ifdef UNNORMALIZED
      /* This line is unnecessary when GENPOLY[NROOTS] is unity, as it must
       * always be for the polynomials constructed by init_rs()
       */
      feedback = MODNN(NN - GENPOLY[NROOTS] + feedback);
#endif
      for(j=1;j<NROOTS;j++)
        parity[j] ^= ALPHA_TO[MODNN(feedback + GENPOLY[NROOTS-j])];
    }
    /* Shift */
    memmove(&parity[0],&parity[1],sizeof(data_t)*(NROOTS-1));
    if(feedback != A0)
      parity[NROOTS-1] = ALPHA_TO[MODNN(feedback + GENPOLY[0])];
    else
      parity[NROOTS-1] = 0;
  }

  for(j=0;j<NROOTS;j++)
    parity[j] = TalToDualBasis[parity[j]];
}
