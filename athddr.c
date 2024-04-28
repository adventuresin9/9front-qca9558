/*
 *	Atheros DDR controller.
 *	Mostly needed because it has some functions
 *	that flush writes for various periferals.
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "io.h"





static u32int
rdddr(u32int offset)
{
	return *IO(u32int, (DDRBASE | offset));
}


static void
wrddr(u32int offset, u32int val)
{
	*IO(u32int, (DDRBASE | offset)) = val;
	wbarrier();
	*IO(u32int, (DDRBASE | offset));
}


void
ddrflusher(u32int offset)
{
	wrddr(offset, 0x1);
}

