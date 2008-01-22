/** \file apop_histogram.c 

This implements a one-d histogram representing an empirical
distribution. It is primarily a wrapper for the GSL's comparable functions
in the standard \c apop_model form, for easy comparison with other models.

Copyright (c) 2007 by Ben Klemens.  Licensed under the modified GNU GPL v2; see COPYING and COPYING2.  */

#include "asst.h"
#include "model.h"
#include "types.h"
#include "stats.h"
#include "mapply.h"
#include "settings.h"
#include <gsl/gsl_math.h>
#include <gsl/gsl_histogram.h>
#include <stdio.h>
#include <assert.h>

apop_model apop_histogram;

/** Allocate the parameters for the \c apop_histogram model.

  \param    data The input data. As with other distributions, the data
  should be in the matrix element of the \c apop_data set, and can have any dimensions
  (\f$1\times 10000\f$, \f$10000\times 1\f$, \f$100\times 100\f$...).
  \param bins How many bins should the PDF have?
 */
apop_histogram_settings *apop_histogram_settings_alloc(apop_data *data, int bins){
    //header is in model.h.
  apop_histogram_settings *hp = malloc(sizeof(*hp));
  size_t              i, j, sum = 0;
  double              minv    = GSL_POSINF,
                      maxv    = GSL_NEGINF,
                      minm    = GSL_POSINF,
                      maxm    = GSL_NEGINF;
    if (data->vector) gsl_vector_minmax(data->vector, &minv, &maxv);
    if (data->matrix) gsl_matrix_minmax(data->matrix, &minm, &maxm);
    hp->pdf = gsl_histogram_alloc(bins);
    gsl_histogram_set_ranges_uniform(hp->pdf, GSL_MIN(minv,minm), GSL_MAX(maxv,maxm));
   //add infinity bins.
    double  *newbins = malloc(sizeof(double)* ( bins +3));
    newbins[0]                  = GSL_NEGINF;
    memcpy((newbins + 1), hp->pdf->range, sizeof(double) * (bins+1));
    newbins[bins+1]      += 2*GSL_DBL_EPSILON; //so max won't fall in the infinity bin.
    newbins[bins+2]         = GSL_POSINF;
    gsl_histogram_free(hp->pdf);
    hp->pdf = gsl_histogram_alloc(bins+2);
    gsl_histogram_set_ranges(hp->pdf, newbins,bins+3);

    if (data->vector)
        for (i=0; i< data->vector->size; i++){
            gsl_histogram_increment(hp->pdf,gsl_vector_get(data->vector,i));
            sum++;
        }
    if (data->matrix)
        for (i=0; i< data->matrix->size1; i++)
            for (j=0; j< data->matrix->size2; j++){
            gsl_histogram_increment(hp->pdf,gsl_matrix_get(data->matrix,i,j));
            sum ++;
        }
    for (i=0; i< hp->pdf->n; i++)
        hp->pdf->bin[i]    /= (sum + 0.0);
    hp->cdf =NULL;
    hp->histobase =NULL;
    hp->kernelbase =NULL;
    return hp;
}

void * apop_histogram_settings_copy(apop_histogram_settings *in){
    apop_histogram_settings *out = malloc(sizeof(apop_histogram_settings));
    out->pdf = gsl_histogram_clone(in->pdf);
    out->cdf = NULL; //the GSL doesn't provide a copy function, so screw it---just regenerate.
    if (in->histobase)
        out->histobase = apop_model_copy(*in->histobase);
    if (in->kernelbase)
        out->kernelbase = apop_model_copy(*in->kernelbase);
    return out;
}

void apop_histogram_settings_free(apop_histogram_settings *in){
    //I'll come back to this later...
    /*gsl_histogram_free(in->pdf);
    gsl_histogram_pdf_free(in->cdf);
    apop_model_free(in->histobase);
    apop_model_free(in->kernelbase);*/
}


apop_model *est(apop_data *d, apop_model *in){
    apop_model *out = apop_model_copy(*in);
    Apop_settings_add_group(out, apop_histogram, d, 1000);
    return out;
}

static gsl_histogram *gpdf;

static double one_vector(gsl_vector *in){
  size_t    i, k;
  double    product = 0;
    for (i=0; i< in->size; i++){
        gsl_histogram_find(gpdf, gsl_vector_get(in, i), &k);
        product += gsl_histogram_get (gpdf, k);
    }
    return product;
}

static double histogram_p(apop_data *d, apop_model *parameters){
  apop_histogram_settings *hp = apop_settings_get_group(parameters, "apop_histogram");
  apop_assert(hp, 0, 0, 's', "you sent me an unparametrized model.");
  long double           product = 0;
    gpdf    = hp->pdf;
    if (d->vector)
        product += one_vector(d->vector);
    if (d->matrix){
        gsl_vector *outp = apop_matrix_map(d->matrix, one_vector);
        product += apop_vector_sum(outp);
        gsl_vector_free(outp);
    }
    return product;
}

static void histogram_rng(double *out, gsl_rng *r, apop_model* eps){
  apop_histogram_settings *hp = apop_settings_get_group(eps, "apop_histogram");
  apop_assert_void(hp, 0, 's', "you sent me an unparametrized model.");
    if (!hp->cdf){
        hp->cdf = gsl_histogram_pdf_alloc(hp->pdf->n); //darn it---this produces a CDF!
        gsl_histogram_pdf_init(hp->cdf, hp->pdf);
    }
    do {
        *out  = gsl_histogram_pdf_sample(hp->cdf, gsl_rng_uniform(r));
    } while (!gsl_finite(*out));
}

/** The histogram model.

  This is an empirical distribution. If you have a data set from which you want to make random draws, this is overkill; instead just use something like \code 
  gsl_rng *r = apop_rng_alloc(27);
  gsl_vector *my_data = [gather data here.];
  gsl_vector_get(my_data, gsl_rng_uniform(r)*my_data->size);
  \endcode

  But this can be used anywhere a model is needed, such as the inputs and outputs to \c apop_update.

  The model is unlike most other models in that there are no parameters
  of any sort (beyond the data itself), so there is no \c estimate
  method; instead all the work of producing the histogram is done in \c
  apop_histogram_settings_alloc. [Actually, there is an \c estimate method,
  but it is just an alias for the histogram_alloc function.]

\ingroup models
*/
apop_model apop_histogram = {"Histogram", 0,0,0, .estimate = est, .p = histogram_p, .draw = histogram_rng};
