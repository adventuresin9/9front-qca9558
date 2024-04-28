#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "io.h"

enum {
	Qdir = 0,
	Qbase,

	Qmax = 16,
};

typedef long Rdwrfn(Chan*, void*, long, vlong);

static Rdwrfn *readfn[Qmax];
static Rdwrfn *writefn[Qmax];

static Dirtab archdir[Qmax] = {
	".",		{ Qdir, 0, QTDIR },	0,	0555,
};

Lock archwlock;	/* the lock is only for changing archdir */
QLock plock;
int narchdir = Qbase;

/*
 * Add a file to the #P listing.  Once added, you can't delete it.
 * You can't add a file with the same name as one already there,
 * and you get a pointer to the Dirtab entry so you can do things
 * like change the Qid version.  Changing the Qid path is disallowed.
 */
Dirtab*
addarchfile(char *name, int perm, Rdwrfn *rdfn, Rdwrfn *wrfn)
{
	int i;
	Dirtab d;
	Dirtab *dp;

	memset(&d, 0, sizeof d);
	strcpy(d.name, name);
	d.perm = perm;

	lock(&archwlock);
	if(narchdir >= Qmax){
		unlock(&archwlock);
		return nil;
	}

	for(i=0; i<narchdir; i++)
		if(strcmp(archdir[i].name, name) == 0){
			unlock(&archwlock);
			return nil;
		}

	d.qid.path = narchdir;
	archdir[narchdir] = d;
	readfn[narchdir] = rdfn;
	writefn[narchdir] = wrfn;
	dp = &archdir[narchdir++];
	unlock(&archwlock);

	return dp;
}

static Chan*
archattach(char* spec)
{
	return devattach('P', spec);
}

Walkqid*
archwalk(Chan* c, Chan *nc, char** name, int nname)
{
	return devwalk(c, nc, name, nname, archdir, narchdir, devgen);
}

static int
archstat(Chan* c, uchar* dp, int n)
{
	return devstat(c, dp, n, archdir, narchdir, devgen);
}

static Chan*
archopen(Chan* c, int omode)
{
	return devopen(c, omode, archdir, narchdir, devgen);
}

static void
archclose(Chan*)
{
}

static long
archread(Chan *c, void *a, long n, vlong offset)
{
	Rdwrfn *fn;

	switch((ulong)c->qid.path){
	case Qdir:
		return devdirread(c, a, n, archdir, narchdir, devgen);

	default:
		if(c->qid.path < narchdir && (fn = readfn[c->qid.path]))
			return fn(c, a, n, offset);
		error(Eperm);
		break;
	}

	return 0;
}

static long
archwrite(Chan *c, void *a, long n, vlong offset)
{
	Rdwrfn *fn;

	if(c->qid.path < narchdir && (fn = writefn[c->qid.path]))
		return fn(c, a, n, offset);
	error(Eperm);

	return 0;
}

void archinit(void);

Dev archdevtab = {
	'P',
	"arch",

	devreset,
	archinit,
	devshutdown,
	archattach,
	archwalk,
	archstat,
	archopen,
	devcreate,
	archclose,
	archread,
	devbread,
	archwrite,
	devbwrite,
	devremove,
	devwstat,
};

//cputype "mips 24k" fpu etc from cop0
// get chip id ascii from sysctl regs
static long
cputyperead(Chan*, void *a, long n, vlong offset)
{
	u32int pridval, conf0val, conf1val;
	u8int compop, compid, procid, revision;
	u8int major, minor, patch;
	u8int fpu, cop2, tlbs, coher;
	int icache, dcache;
	char *procidstr, *endian, *writetype;

	char *p;
	int l;

	pridval = prid();
	conf0val = getconfig();
	conf1val = getconfig1();

	compop = (pridval >> 24) & 0xFF;
	compid = (pridval >> 16) & 0xFF;
	procid = (pridval >> 8) & 0xFF;
	revision = pridval & 0xFF;

	patch = revision & 0x3;
	minor = (revision >> 2) & 0x7;
	major = (revision >> 5) & 0x7;

	switch(procid){
	case 0x93:
		procidstr = "24K";
		break;
	case 0x96:
		procidstr = "24KE";
		break;
	case 0x97:
		procidstr = "74K";
		break;
	case 0x99:
		procidstr = "1004K";
		break;
	default:
		procidstr = "missingno";
		break;
	}

	coher = conf0val & 0x7;

	switch(coher){
	case 0:
		writetype = "write-through";
		break;
	case 2:
		writetype = "uncached";
		break;
	case 3:
		writetype = "write-back";
		break;
	case 7:
		writetype = "uncached accelerated";
		break;
	default:
		writetype = "unknown";
		break;
	}

	endian = conf0val & (1<<15)? "mips" : "spim";
	fpu = conf1val & 0x1? 1 : 0;
	cop2 = conf1val & 0x40? 1 : 0;
	tlbs = ((conf1val >> 25) & 0x3F) + 1;
	icache = (64 << ((conf1val >> 22) & 0x7)) / 8;
	dcache = (64 << ((conf1val >> 13) & 0x7)) / 8;

	p = smalloc(READSTR);
	l = 0;

	l += snprint(p+l, READSTR-l, "%s %s\n", endian, procidstr);
	l += snprint(p+l, READSTR-l, "%uX %uX %d.%d.%d\n",
		compid, compop, major, minor, patch);
	l += snprint(p+l, READSTR-l, "%dkB icache, %dkB dcache, %s\n",
		icache, dcache, writetype);
	l += snprint(p+l, READSTR-l, "%d TLB entries\n", tlbs);
	l += snprint(p+l, READSTR-l, "fpu: %s\n", fpu ? "yes" : "no");
	l += snprint(p+l, READSTR-l, "cop2: %s\n", cop2 ? "yes" : "no");

	n = readstr(offset, a, n, p);
	free(p);
	USED(l);

	return n;
}



//irqalloc, might be best to add from irq.c, like pc does

// clock enable readings from sysctl regs

void
archinit(void)
{

	addarchfile("cputype", 0444, cputyperead, nil);

}
