# Finesse Subcomponents for FUSE

This directory contains the general purpose pieces of the Finesse extensions for FUSE.  Note that there are some changes in the FUSE library itself, as well as one additional file (finesse.c) that links directly with the FUSE library.  Libraries here are intented to (1) support the FUSE library extensions; (2) permit a direct-call API for Finesse; and (3) provide a shared library that can intercept I/O calls via LD_PRELOAD in existing applications, converting those to use Finesse.

The directories each have a distinct functional role:

* api - this is the Finesse API layer. It can be used directly by applications for FUSE access or it can provide support for the LD_PRELOAD library.  Thus, the interface for calls is generally compatible between the two.
* communications - this captures the communications API implementation between the Finesse/FUSE server and any clients using the Finesse API (directly or indirectly).
* include - contains the common header files for using Finesse
* preload - this is where the LD_PRELOAD library should go
* tests - these are a set of unit tests.
* utils - these are utility functions, primarily for use by the Finesse+FUSE server layer.

While there's actually a fair amount of code here, much of it is boilerplate.  This makes adding new calls fairly easy, as well as adding additional unit tests.  Despite that, keep in mind that each unique function seems to differ slightly from all the others, so it is easy to get things wrong.  Fortunately, doing so will manifest as an error fairly quickly.

