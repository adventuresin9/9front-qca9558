# 9front-qca9558
9front kernel for the QCA9558

This kernel has the fully functioning fpuemu code, and requires the hack in ;

https://github.com/adventuresin9/mt7688-fpu-patch

to compile properly.

## ToDo

Unlike previous 24k chips I've done kernels for, the 74k series chips can do write-back caching, and are reported to run far better with it.  However, the current ethernet driver needs to be rewriteen to make sure the cache is flushed when it need to be.

see;  https://forums.freebsd.org/threads/fixing-up-the-qca9558-performance-on-freebsd-or-why-attention-to-detail-matters-when.53828/

Also, the switch is configured through mii, so a switch configuration system needs to be done.

There are plenty of datasheets for these chips here;

https://github.com/Deoptim/atheros

