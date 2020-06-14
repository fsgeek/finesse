# Bitbucket

The purpose of the Bitbucket file system is to provide an efficient in-memory full featured file system that focuses on being fast (and correct) with respect to meta-data management.  It is NOT interested in the actual data storage, so reads return zero filled blobs and writes are discarded.  Not useful as a _file system_ per-se, but it is useful as a tool for looking at ways to improve the performance of a file system _framework_ --- in this case **FUSE**.

Thus, the goals here are:

* Good parallelism/scalability.  To the extent possible this code tries to use the least synchronization required: atomics for increment/decrement, reader/writer locks for data structures that are used for mostly lookup, and short duration holding periods.

* Good internal consistency checking.  The code attempts to quickly identify issues, with an eye towards quickly identifying inconsisent or incorrect behavior.

* Simplicity.  Sometimes parallelism/scalability is an enemy to simplicity, so there is a balancing act: the lookup table is not dynamically scalable (simplicty) but it exists (parallelism/scalability).  I use read locks in decrement paths with atomics: the logic here is not simple, but it is fairly small.  In general, I tend to write more comments when something isn't simple.

## Key Data Structures


### Object

There is a general concept of an "object"; the implementation of this is not entirely transparent (though it probably should be).  The primary goal of objects is to handle the reference counting aspects: when the object ref count drops to zero, the object itself can be deleted.

## Inode

These are fairly traditional UNIX style file system meta-data structures and each specialization represents the _kinds_ of structures one finds in a UNIX or Linux file system: directories, files, symbolic links, devices, sockets, etc.

## Current Issues

The current issue that I'm grappling with (and the motivation for writing up _this_ document) is trying to reconcile the reference counting model that I created versus the reference counting model that FUSE seems to be using.

FUSE provides **two** mechanisms for finding a given inode:

* The **inode number** which is a file system unique 64 bit identifier (there's a separate "generation number" that is also used to uniquify re-used inodes, which I currently capture as the "Epoch" of the given object).

* The **inode handle** which is returned by certain calls (open/create/opendir) and then passed into other specific calls.

These tie into their model of reference counting: the open count represents the number of file handles that have been returned by the kernel to applications for a given inode.  Thus, they have a separate call (`release`) that represents releasing those references.  The lookup count represents the number of references to a given file handle.  A `lookup` operation also returns a reference, which is released via a call to `forget`.  Some operations (e.g., readdirplus) take out this style of reference. The write-up (in the header and code) does not describe this especially well.  What I have ascertained via nature study is:

* Open style operations return both reference types: thus an open gets both the handle type reference (`fi->fh`) and the lookup type reference (based on the `ino_t` value).  These are wired backwards to the way that I've constructed my current reference counting model, because I remove inodes from the Inode Table before the object itself goes away.

Thus, I'm thinking that I need to re-think this; I'd also like to abstract it a bit in trying to address the reference counting model with an eye towards fitting into the FUSE model better as well as potential future direction.  Thus, I'm thinking of having:

* A `node` table.  Nodes are created in that table and returned with a single reference to them.  When the reference count drops to zero, the node is removed from the table.  Nodes can thus be found (in the table) using their node id, which creates a lookup reference to them.  Nodes may themselves reference other nodes: this is the traditional model of a "directory" (where the node points to other nodes).

* A `reference` table.  Each node contains a list of the nodes to which it has references.  Since the nodes themselves are reference counted objects, these references increment the reference count on the node.  This allows detection of a "dead" node (one with a reference count of zero).  The downside to simply having reference counts is that there's really no easy way to delete all the references (e.g., if you want to force a `node` to be really removed).


```
    This has me thinking about using a compressed sparse row (CSR) model.  I've not implemented CSR previously, so this would be a new data structure to build, but might be useful in the future.  For FUSE, I don't need that much generality, since minimally connected graphs have fairly nice properties. I note that hard links undermine this.

    (off doing research...)
    The challenge here is that the size of the graph is potentially quite large, _but_ it is also quite sparse.  Thus, whatever algorithm is used needs to be optimized for exactly this case.

    I'm envisioning this as a "Trie" style structure, just for node numbers. A "leaf" then contains some number of bits, each of which represents the presence/absence of the given entry in the adjacency list.

    CSR is a good persistent storage representation, but it is not particularly dynamic. It seems that Dictionary of Keys or List in List is preferred for in-memory representation.  I think that's what my tree-based structure looks like - something akin to a page table.

    Another approach to consider might be [ANS](https://en.wikipedia.org/wiki/Asymmetric_numeral_systems).

    Since the only thing I want to store is a single bit (there's an edge), I could use even/odd number to encode this information (low order bit) or I could use the high order bit information (so bit 63 in a zero indexed value).

    Combining these approaches might make the greatest sense for me.  I need to think about this more - later.
```

Enough for random thoughts now.  I do need to address this.


