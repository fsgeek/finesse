# Finesse Tests

I've used incremental testing while constructing Finesse in order to ensure that the individual pieces work in what is a fairly complex system.  To that end, there are multiple "levels" of the tests:

* Testing that the basic message packing/unpacking is working correctly.  This is important because I've chosen to use manually constructed calls, rather than using tools to automate building what is, in essence, a shared memory based RPC mechanism.  While not germane to testing, I _did_ try using two different approaches to automated construction, but was disappointed with the resulting performance since they involved adding further CPU overhead.
    - If you look at fincomm_test, you will see that it uses an _in memory_ buffer for testing the packing/unpacking routines.  This avoids some of the complications of shared memory, which is useful, particularly when first getting started up.

    - Once _that_ works, it makes sense to add in the shared memory, but I still run in a single thread, interleaving the client and server interactions:

        * Start server
        * Start client
        * Client does a simple test (the "test" message)
        * Server grabs that specific message
        * Server validates the message
        * Server constructs a response
        * Client waits for the response to arrive (note that it _already has_ though, so verifying block-and-wait can't be done at this point) 
        * Client validates the response
        * Stop client
        * Stop server
    
        Of course you can add multiple operations in the mix, but the first step should be to add _one_ operation and get it working.

        Usually, I'm testing this against the library interface.  Sometimes the code I write inside the test environment ends up moving into the library (or into the server side implementation).  As much as possible, I've tried to put the logic inside the library.

* Testing that the shared memory logic is working.  Instead of using an in-memory buffer, use the shared memory interface; this then splits the addresses so what the _client_ uses and what the _server_ uses aren't the same.  It catches _some_ pointer related issues (like passing the wrong handle) but not all.  To capture more, you can fork a sub-process (I don't do that presently) and have the subprocess act as the server.

* Testing that the FUSE server is working.  In this case the server is now in a separate address space from the client code.  I started by testing the communications libraries, then the actual finesse_ functions (which are logically "drop in" replacements for libc functions).

I'm sure there's more to do, but this has worked surprisingly well.