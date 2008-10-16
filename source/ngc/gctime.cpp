/****************************************************************************
 * gctime.cpp
 ****************************************************************************/
#include "gctime.h"

#define TB_CLOCK  40500000


unsigned long tb_diff_msec(tb_t *end, tb_t *start)
{
        unsigned long upper, lower;
        upper = end->u - start->u;
        if (start->l > end->l)
                upper--;
        lower = end->l - start->l;
        return ((upper*((unsigned long)0x80000000/(TB_CLOCK/2000))) + (lower/(TB_CLOCK/1000)));
}

unsigned long tb_diff_usec(tb_t *end, tb_t *start)
{
        unsigned long upper, lower;
        upper = end->u - start->u;
        if (start->l > end->l)
            upper--;
        lower = end->l - start->l;
        return ((upper*((unsigned long)0x80000000/(TB_CLOCK/2000000))) + (lower/(TB_CLOCK/1000000)));
}

void udelay(unsigned int us)
{
        tb_t start, end;
        mftb(&start);
        while (1)
        {
                mftb(&end);
                if (tb_diff_usec(&end, &start) >= us)
                        break;
        }
}

void mdelay(unsigned int ms)
{
        tb_t start, end;
        mftb(&start);
        while (1)
        {
                mftb(&end);
                if (tb_diff_msec(&end, &start) >= ms)
                        break;
        }
}
