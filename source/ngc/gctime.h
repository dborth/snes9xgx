/****************************************************************************
 * gctime.h
 ****************************************************************************/

#define mftb(rval) ({unsigned long u; do { \
         asm volatile ("mftbu %0" : "=r" (u)); \
         asm volatile ("mftb %0" : "=r" ((rval)->l)); \
         asm volatile ("mftbu %0" : "=r" ((rval)->u)); \
         } while(u != ((rval)->u)); })

typedef struct
{
        unsigned long l, u;
} tb_t;

unsigned long tb_diff_msec(tb_t *end, tb_t *start);
unsigned long tb_diff_usec(tb_t *end, tb_t *start);
void udelay(unsigned int us);
void mdelay(unsigned int ms);


