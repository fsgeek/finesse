Finesse
=======

About
-----

Finesse is a hybrid implementation of FUSE (Filesystem in Userspace)
that combines traditional upcall kernel support (via VFS) with a
separate message passing/shared memory model.

The basic premise of Finesse is to permit extension of the underlying
VFS interface _without_ breaking backwards compatibility.  This is achieved
by adding an interposition layer between the application and the system
call interface, as well as a layer between the FUSE library and the
FUSE file system.

Behavior can be fallback (existing FUSE) behavior via the system call
interface, optimized behavior via the Finesse message passing layer,
or enhanced behavior via the Finesse message passing layer.  Only the
latter requires changing applications explicitly.

TBD: describe building and installing as well as a basic description of
how Finesse functions.

fsgeek@cs.ubc.ca
