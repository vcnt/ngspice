/**********
Copyright 2008 Holger Vogt.  All rights reserved.
Author:   2008 Holger Vogt
**********/

/*
 * Code to do fast fourier transform on data.
 */

#define GREEN /* select fast Green's fft */

#include "ngspice/ngspice.h"
#include "ngspice/ftedefs.h"
#include "ngspice/dvec.h"
#include "ngspice/sim.h"

#include "com_fft.h"
#include "variable.h"
#include "parse.h"
#include "../misc/misc_time.h"
#include "ngspice/fftext.h"


void
com_fft(wordlist *wl)
{
    ngcomplex_t **fdvec = NULL;
    double  **tdvec = NULL;
    double  *freq, *win = NULL, *time;
    double  span;
    int     fpts, i, j, tlen, ngood;
    struct dvec  *f, *vlist, *lv = NULL, *vec;
    struct pnode *pn, *names = NULL;
    char   window[BSIZE_SP];
    double maxt;

#ifdef GREEN
    int M;
#endif

    double *reald = NULL, *imagd = NULL;
    int N, order;
    double scale;

    if (!plot_cur || !plot_cur->pl_scale) {
        fprintf(cp_err, "Error: no vectors loaded.\n");
        goto done;
    }
    if (!isreal(plot_cur->pl_scale) ||
        ((plot_cur->pl_scale)->v_type != SV_TIME)) {
        fprintf(cp_err, "Error: fft needs real time scale\n");
        goto done;
    }

    tlen = (plot_cur->pl_scale)->v_length;
    time = (plot_cur->pl_scale)->v_realdata;
    span = time[tlen-1] - time[0];

#ifdef GREEN
    /* size of fft input vector is power of two and larger or equal than spice vector */
    N = 1;
    M = 0;
    while (N < tlen) {
        N <<= 1;
        M++;
    }
#else
    /* size of input vector is power of two and larger than spice vector */
    N = 1;
    while (N < tlen)
        N *= 2;
#endif
    /* output vector has length of N/2 */
    fpts = N/2;

    win = TMALLOC(double, tlen);
    maxt = time[tlen-1];
    if (!cp_getvar("specwindow", CP_STRING, window))
        strcpy(window, "blackman");
    if (!cp_getvar("specwindoworder", CP_NUM, &order))
        order = 2;
    if (order < 2)
        order = 2;

    if (fft_windows(window, win, time, tlen, maxt, span, order) == 0)
        goto done;

    names = ft_getpnames(wl, TRUE);
    vlist = NULL;
    ngood = 0;
    for (pn = names; pn; pn = pn->pn_next) {
        vec = ft_evaluate(pn);
        for (; vec; vec = vec->v_link2) {

            if (vec->v_length != tlen) {
                fprintf(cp_err, "Error: lengths of %s vectors don't match: %d, %d\n",
                        vec->v_name, vec->v_length, tlen);
                continue;
            }

            if (!isreal(vec)) {
                fprintf(cp_err, "Error: %s isn't real!\n", vec->v_name);
                continue;
            }

            if (vec->v_type == SV_TIME) {
                continue;
            }

            if (!vlist)
                vlist = vec;
            else
                lv->v_link2 = vec;

            lv = vec;
            ngood++;
        }
    }

    if (!ngood)
        goto done;

    plot_cur = plot_alloc("spectrum");
    plot_cur->pl_next = plot_list;
    plot_list = plot_cur;
    plot_cur->pl_title = copy((plot_cur->pl_next)->pl_title);
    plot_cur->pl_name = copy("Spectrum");
    plot_cur->pl_date = copy(datestring());

    freq = TMALLOC(double, fpts);
    f = alloc(struct dvec);
    ZERO(f, struct dvec);
    f->v_name = copy("frequency");
    f->v_type = SV_FREQUENCY;
    f->v_flags = (VF_REAL | VF_PERMANENT | VF_PRINT);
    f->v_length = fpts;
    f->v_realdata = freq;
    vec_new(f);

    for (i = 0; i<fpts; i++)
        freq[i] = i*1.0/span*tlen/N;

    tdvec = TMALLOC(double  *, ngood);
    fdvec = TMALLOC(ngcomplex_t *, ngood);
    for (i = 0, vec = vlist; i<ngood; i++) {
        tdvec[i] = vec->v_realdata; /* real input data */
        fdvec[i] = TMALLOC(ngcomplex_t, fpts); /* complex output data */
        f = alloc(struct dvec);
        ZERO(f, struct dvec);
        f->v_name = vec_basename(vec);
        f->v_type = SV_NOTYPE;
        f->v_flags = (VF_COMPLEX | VF_PERMANENT);
        f->v_length = fpts;
        f->v_compdata = fdvec[i];
        vec_new(f);
        vec = vec->v_link2;
    }

    printf("FFT: Time span: %g s, input length: %d, zero padding: %d\n", span, N, N-tlen);
    printf("FFT: Freq. resolution: %g Hz, output length: %d\n", 1.0/span*tlen/N, fpts);

    reald = TMALLOC(double, N);
    imagd = TMALLOC(double, N);
    for (i = 0; i<ngood; i++) {
        for (j = 0; j < tlen; j++) {
            reald[j] = tdvec[i][j]*win[j];
            imagd[j] = 0.0;
        }
        for (j = tlen; j < N; j++) {
            reald[j] = 0.0;
            imagd[j] = 0.0;
        }
#ifdef GREEN
        // Green's FFT
        fftInit(M);
        rffts(reald, M, 1);
        fftFree();

        scale = (double) N;
        /* Re(x[0]), Re(x[N/2]), Re(x[1]), Im(x[1]), Re(x[2]), Im(x[2]), ... Re(x[N/2-1]), Im(x[N/2-1]). */
        for (j = 0; j < fpts; j++) {
            fdvec[i][j].cx_real = reald[2*j]/scale;
            fdvec[i][j].cx_imag = reald[2*j+1]/scale;
        }
        fdvec[i][0].cx_imag = 0;
#else
        fftext(reald, imagd, N, tlen, 1 /* forward */);
        scale = 0.66;

        for (j = 0; j < fpts; j++) {
            fdvec[i][j].cx_real = reald[j]/scale;
            fdvec[i][j].cx_imag = imagd[j]/scale;
        }
#endif
    }

done:
    tfree(reald);
    tfree(imagd);

    tfree(tdvec);
    tfree(fdvec);
    tfree(win);

    free_pnode(names);
}


void
com_psd(wordlist *wl)
{
    ngcomplex_t **fdvec = NULL;
    double  **tdvec = NULL;
    double  *freq, *win = NULL, *time, *ave;
    double  span, noipower;
    int     M;
    int N, ngood, fpts, i, j, tlen, jj, smooth, hsmooth;
    char    *s;
    struct dvec  *f, *vlist, *lv = NULL, *vec;
    struct pnode *pn, *names = NULL;
    char   window[BSIZE_SP];
    double maxt;

    double *reald = NULL, *imagd = NULL;
    double scaling, sum;
    int order;

    if (!plot_cur || !plot_cur->pl_scale) {
        fprintf(cp_err, "Error: no vectors loaded.\n");
        goto done;
    }
    if (!isreal(plot_cur->pl_scale) ||
        ((plot_cur->pl_scale)->v_type != SV_TIME)) {
        fprintf(cp_err, "Error: fft needs real time scale\n");
        goto done;
    }

    tlen = (plot_cur->pl_scale)->v_length;
    time = (plot_cur->pl_scale)->v_realdata;
    span = time[tlen-1] - time[0];

    // get filter length from parameter input
    s = wl->wl_word;
    ave = ft_numparse(&s, FALSE);
    if (!ave || (*ave < 1.0)) {
        fprintf(cp_out, "Number of averaged data points:  %d\n", 1);
        smooth = 1;
    } else {
        smooth = (int)(*ave);
    }

    wl = wl->wl_next;

    /* size of fft input vector is power of two and larger or equal than spice vector */
    N = 1;
    M = 0;
    while (N < tlen) {
        N <<= 1;
        M++;
    }

    // output vector has length of N/2
    fpts = N/2;

    win = TMALLOC(double, tlen);
    maxt = time[tlen-1];
    if (!cp_getvar("specwindow", CP_STRING, window))
        strcpy(window, "blackman");
    if (!cp_getvar("specwindoworder", CP_NUM, &order))
        order = 2;
    if (order < 2)
        order = 2;

    if (fft_windows(window, win, time, tlen, maxt, span, order) == 0)
        goto done;

    names = ft_getpnames(wl, TRUE);
    vlist = NULL;
    ngood = 0;
    for (pn = names; pn; pn = pn->pn_next) {
        vec = ft_evaluate(pn);
        for (; vec; vec = vec->v_link2) {

            if (vec->v_length != (int)tlen) {
                fprintf(cp_err, "Error: lengths of %s vectors don't match: %d, %d\n",
                        vec->v_name, vec->v_length, tlen);
                continue;
            }

            if (!isreal(vec)) {
                fprintf(cp_err, "Error: %s isn't real!\n", vec->v_name);
                continue;
            }

            if (vec->v_type == SV_TIME) {
                continue;
            }

            if (!vlist)
                vlist = vec;
            else
                lv->v_link2 = vec;

            lv = vec;
            ngood++;
        }
    }

    if (!ngood)
        goto done;

    plot_cur = plot_alloc("spectrum");
    plot_cur->pl_next = plot_list;
    plot_list = plot_cur;
    plot_cur->pl_title = copy((plot_cur->pl_next)->pl_title);
    plot_cur->pl_name = copy("PSD");
    plot_cur->pl_date = copy(datestring());

    freq = TMALLOC(double, fpts + 1);
    f = alloc(struct dvec);
    ZERO(f, struct dvec);
    f->v_name = copy("frequency");
    f->v_type = SV_FREQUENCY;
    f->v_flags = (VF_REAL | VF_PERMANENT | VF_PRINT);
    f->v_length = fpts;
    f->v_realdata = freq;
    vec_new(f);

    for (i = 0; i <= fpts; i++)
        freq[i] = i*1./span*tlen/N;

    tdvec = TMALLOC(double*, ngood);
    fdvec = TMALLOC(ngcomplex_t*, ngood);
    for (i = 0, vec = vlist; i<ngood; i++) {
        tdvec[i] = vec->v_realdata; /* real input data */
        fdvec[i] = TMALLOC(ngcomplex_t, fpts + 1); /* complex output data */
        f = alloc(struct dvec);
        ZERO(f, struct dvec);
        f->v_name = vec_basename(vec);
        f->v_type = SV_NOTYPE; //vec->v_type;
        f->v_flags = (VF_COMPLEX | VF_PERMANENT);
        f->v_length = fpts + 1;
        f->v_compdata = fdvec[i];
        vec_new(f);
        vec = vec->v_link2;
    }

    printf("PSD: Time span: %g s, input length: %d, zero padding: %d\n", span, N, N-tlen);
    printf("PSD: Freq. resolution: %g Hz, output length: %d\n", 1.0/span*tlen/N, fpts);

    reald = TMALLOC(double, N);
    imagd = TMALLOC(double, N);

    // scale = 0.66;

    for (i = 0; i<ngood; i++) {
        double intres;
        for (j = 0; j < tlen; j++) {
            reald[j] = (tdvec[i][j]*win[j]);
            imagd[j] = 0.;
        }
        for (j = tlen; j < N; j++) {
            reald[j] = 0.;
            imagd[j] = 0.;
        }

        // Green's FFT
        fftInit(M);
        rffts(reald, M, 1);
        fftFree();

        scaling = (double) N;

        /* Re(x[0]), Re(x[N/2]), Re(x[1]), Im(x[1]), Re(x[2]), Im(x[2]), ... Re(x[N/2-1]), Im(x[N/2-1]). */
        intres = (double)N * (double)N;
        noipower = fdvec[i][0].cx_real = reald[0]*reald[0]/intres;
        fdvec[i][fpts].cx_real = reald[1]*reald[1]/intres;
        noipower += fdvec[i][fpts-1].cx_real;
        for (j = 1; j < fpts; j++) {
            jj = j<<1;
            fdvec[i][j].cx_real = 2.* (reald[jj]*reald[jj] + reald[jj + 1]*reald[jj + 1])/intres;
            fdvec[i][j].cx_imag = 0;
            noipower += fdvec[i][j].cx_real;
            if (!finite(noipower))
                break;
        }

        printf("Total noise power up to Nyquist frequency %5.3e Hz:\n%e V^2 (or A^2), \nnoise voltage or current %e V (or A)\n",
               freq[fpts], noipower, sqrt(noipower));

        /* smoothing with rectangular window of width "smooth",
           plotting V/sqrt(Hz) or I/sqrt(Hz) */
        if (smooth < 1)
            continue;

        hsmooth = smooth>>1;
        for (j = 0; j < hsmooth; j++) {
            sum = 0.;
            for (jj = 0; jj < hsmooth + j; jj++)
                sum += fdvec[i][jj].cx_real;
            sum /= (hsmooth + j);
            reald[j] = (sqrt(sum)/scaling);
        }
        for (j = hsmooth; j < fpts-hsmooth; j++) {
            sum = 0.;
            for (jj = 0; jj < smooth; jj++)
                sum += fdvec[i][j-hsmooth+jj].cx_real;
            sum /= smooth;
            reald[j] = (sqrt(sum)/scaling);
        }
        for (j = fpts-hsmooth; j < fpts; j++) {
            sum = 0.;
            for (jj = 0; jj < smooth; jj++)
                sum += fdvec[i][j-hsmooth+jj].cx_real;
            sum /= (fpts - j + hsmooth - 1);
            reald[j] = (sqrt(sum)/scaling);
        }
        for (j = 0; j < fpts; j++)
            fdvec[i][j].cx_real = reald[j];
    }

done:
    free(reald);
    free(imagd);

    tfree(tdvec);
    tfree(fdvec);
    tfree(win);

    free_pnode(names);
}
