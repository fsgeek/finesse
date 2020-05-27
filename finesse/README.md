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

## Adding a new LD_PRELOAD entry

There's a boilerplate part to this, and then there's the part about wiring in Finesse to it.

### Call boilerplate

There are _three_ things required to add an LD_PRELOAD call:

* The LD_PRELOAD wrapper itself.  This goes in finesse/preload.  It can either be an existing file, or it can be a new file.  I try not to overload the files to make it easier to find them.  The prototype for these functions matches something already exported by a library, so it is usually found in the documentation (e.g., `freopen` which I'm working on as I do this, is documented at [`freopen`](https://www.man7.org/linux/man-pages/man3/fopen.3.html)).  Be careful about the header files that you include; I have found issues with some calls being overridden (and thus not working).  Look at `open` (in open.c) for an example (this has minimal header files to avoid some of these issues).

* The Finesse library API.  Note that this **matches** the LD_PRELOAD prototype.  The reason we have this is to permit an application program (or test) to use the API directly (by invoking the finesse_XXX version).

* The finesse implementation routine.  These are **static** local functions; I try to pair them up with the finesse_XXX function.  So for finesse_XXX there is a fin_XXX function that implements the mechanics of invoking Finesse and then invoking the underlying shared library call.

### LD_PRELOAD

This is a thin shim - usually I just copy the prototype and then add a call to the finesse wrapper version.  For example, I just added `freopen` and it is one line long.  That is the norm.  For variable argument functions (e.g., open), there's a bit more work that needs to be done.

### Finesse Library API

This is where the primary work to invoke Finesse is done.  For a name based call we look to see if the path prefix is of interest to us.  If it is not, we pass it along to the `fin_XXX` variant.  If it is of interest, then we have to perform some operation(s) to communicate with Finesse.  There are basically three possible outcomes for these library API calls:

- We conclude that we aren't interested; we just call the `fin_XXX` wrapper to pass things along to the underlying shared library implementation.

- We decide we _are_ interested and need to do some more processing, but we also need to invoke the original implementation.  An example of this is `open` where we pass the open to the library (and then to the kernel) but we initiate a message to Finesse to obtain a handle for us.  When the kernel returns, we match it up with the Finesse handle in a handle tracking table.

- We decide we can do this entirely with Finesse.  In this case we make the call, get the response, and return the result to the application.  This is what we are trying to achieve: the _kernel bypass_.

As we further develop Finesse, these library API calls are going to become more feature full.  For example, we may change how `readdir` works by replacing the `glibc` implementation with one that exploits a shared memory buffer that we pass to the FUSE server to populate.  In that way, we can transparently give the application access to the directory contents without further interaction with the kernel or Finesse.

For the initial implementation, these often turn into just transparent pass-through calls.

### Finesse call to original shared library

This is usually pretty straight-forward: keep a copy of the original call, look it up the first time we try to use it, pass the call along.  There are some cases where this can become more complicated, such as when there are _version specfic_ calls that have to be looked up (e.g., `pthread_cond_wait` - which we dont' care about here, which requires versioning.  See [Versioning](http://blog.fesnel.com/blog/2009/08/25/preloading-with-multiple-symbol-versions/
)).

Normally, I write these by copying an existing `fin_XXX` operation that is similar.  I change the typedef to match the operation name (`orig_XXX_t`).  It's used in two other places.  I change the name of the function (`orig_XXX`), which is used five times usually, then I change the symbol being looked up (`dlsym(RTLD_NEXT,"XXX")`).  Finally, fix up the parameters at the end to match the parameters of the function and you're done.




