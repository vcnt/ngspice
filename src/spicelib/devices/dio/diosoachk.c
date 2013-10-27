/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1985 Thomas L. Quarles
**********/

#include "ngspice/ngspice.h"
#include "ngspice/cktdefs.h"
#include "diodefs.h"
#include "ngspice/trandefs.h"
#include "ngspice/sperror.h"
#include "ngspice/suffix.h"
#include "ngspice/cpdefs.h"

void
soa_printf(GENinstance *, GENmodel *, CKTcircuit *, const char *, ...);

/* make SOA checks after NR has finished */

int
DIOsoaCheck(CKTcircuit *ckt, GENmodel *inModel)
{
    DIOmodel *model = (DIOmodel *) inModel;
    DIOinstance *here;
    double vd;  /* current diode voltage */
    int maxwarns_fv = 0, maxwarns_bv = 0;
    static int warns_fv = 0, warns_bv = 0;

    if(!(ckt->CKTmode & (MODEDC | MODEDCOP | MODEDCTRANCURVE | MODETRAN | MODETRANOP)))
        return OK;

    for(; model; model = model->DIOnextModel) {

        maxwarns_fv = maxwarns_bv = ckt->CKTsoaMaxWarns;

        for (here = model->DIOinstances; here; here = here->DIOnextInstance) {

            vd = ckt->CKTrhsOld [here->DIOposPrimeNode] -
                 ckt->CKTrhsOld [here->DIOnegNode];

            if (vd > model->DIOfv_max)
                if (warns_fv < maxwarns_fv) {
                    soa_printf((GENinstance*) here, (GENmodel*) model, ckt,
                               "Vf=%g has exceeded Fv_max=%g\n",
                               vd, model->DIOfv_max);
                    warns_fv++;
                }

            if (-vd > model->DIObv_max)
                if (warns_bv < maxwarns_bv) {
                    soa_printf((GENinstance*) here, (GENmodel*) model, ckt,
                               "|Vj|=%g has exceeded Bv_max=%g\n",
                               -vd, model->DIObv_max);
                    warns_bv++;
                }

        }

    }

    return OK;
}