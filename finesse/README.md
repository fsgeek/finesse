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

## Adding a new call

Updated May 19, 2020.

In this section I'm going to describe the steps for adding a new call by walking through the addition of one: **access**.

The prototype for `access` is:

`int access(const char *pathname, int mode);`

So, this involves multiple steps.  I usually start by constructing the _message_ that the client and server exchange with each other.

1. I start by reviewing the communications information.  For `access` right now, I have (see finesse/include/fincomm.h):

```C
        struct {
            uuid_t Inode;
            int Mask;
        } Access;
```
        I copied this from the lowlevel FUSE API.  But this doesn't mesh with the _systemcall_ definition so I am going to widen this.  I've done this before (statvfs), so I am going to use the same approach:

```C
        struct {
            enum {
                ACCESS_NAME = 33,
                ACCESS_INODE = 34,
            } AccessType;
            union {
                uuid_t Inode;
                char Name[0];
            } Options;
            int Mask;
        } Access;
```

I tried to pick unique values for ACCESS_NAME and ACCESS_INODE; this helps detect cases where we use the wrong enum values.  I don't have
a good system for keeping track of these things, unfortunately.

2. Add a set of wrappers for this:

    * Create a new file for the call(s) in the communications library (finesse/communications).  I called it `access.c` in this case.
    * Add `access.c` to `meson.build` in the same directory.
    * Add a `FinesseSendAccessRequest` operation; I usually copy an existing implementation that is similar, since it's (more or less) "boiler plate".
    * Add `FinesseSendAccessResponse`, `FinesseGetAccessResponse`, and `FinesseFreeAccessResponse` to `finesse.h`
    * Since I now have a name and a file descriptor (uuid for Finesse) case, I also added `Faccess` versions of these.

    Go through the routines and code them up to fill in the respective fields.  The return here is just a success or failure, so we don't need to use a structure field in the response structure --- just the `Response` field is enough.

3.  Add tests for these (finesse/tests).  I start with the `finesse_test.c` file and add it there.  I use the existing tests as a skeleton and adapt them as necessary.

4. Add a handler for the server side of this.  This goes into `finesse/server/access.c` (in this case).  Then add a call to handle this (e.g., `FinesseServerFuseAccess`). The API for this is not standardized; you have to figure out which FUSE fields the call is using on return.  If this needs to call the FUSE file system, you can do so here and then copy the results into the message and send it.

5. Add support for the operation inside the FUSE server.  This is done in `lib/finesse.c`

6. Add a test for this (`finesse/tests`) in `fuse_test.c`.

    When debugging at this step, I normally run FUSE under gdb in one terminal, and the test program in the other.

Normally, I build the testing up incrementally so at each step I confirm (as much as possible) that I have implemented things properly.


