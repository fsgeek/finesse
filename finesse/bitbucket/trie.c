#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "bitbucket.h"
#include "trie.h"

// Source: https://www.techiedelight.com/trie-implementation-insert-search-delete
// Note that this is a simple, non-synchronized Trie implementation in C.  Sufficient for
// an in-memory file system.
//

// define character size
#define CHAR_SIZE 256 // ASCII minus '\0'

// A Trie node
struct Trie
{
	struct  Trie* character[CHAR_SIZE];
    void   *Object; // this is the object to which this Trie entry points
};

// Function that returns a new Trie node
struct Trie* TrieCreateNode(void)
{
	struct Trie* node = (struct Trie*)malloc(sizeof(struct Trie));
    node->Object = NULL;

	for (int i = 0; i < CHAR_SIZE; i++) {
		node->character[i] = NULL;
	}

	return node;
}

// Iterative function to insert a string in Trie
void TrieInsert(struct Trie *head, const char* str, void *object)
{
	// start from root node
	struct Trie* curr = head;
	while (*str)
	{
		// create a new node if path doesn't exists
		if (curr->character[(uint8_t)*str] == NULL) {
			curr->character[(uint8_t)*str] = TrieCreateNode();
		}

		// go to next node
		curr = curr->character[(uint8_t)*str];

		// move to next character
		str++;
	}

	// mark current node as leaf
    curr->Object = object;
}

// Iterative function to search a string in Trie. It returns 1
// if the string is found in the Trie, else it returns 0
void *TrieSearch(struct Trie* head, const char* str)
{
	// return 0 if Trie is empty
	if (head == NULL){
		return 0;
	}

	struct Trie* curr = head;
	while (*str)
	{
		// go to next node
		curr = curr->character[(uint8_t)*str];

		// if string is invalid (reached end of path in Trie)
		if (curr == NULL)
			return 0;

		// move to next character
		str++;
	}

	// if current node is a leaf and we have reached the
	// end of the string, return 1
    return curr->Object;
}

// returns 1 if given node has any children
static int haveChildren(struct Trie* curr)
{
	for (int i = 0; i < CHAR_SIZE; i++) {
		if (NULL != curr->character[i]) {
			return 1;	// child found
		}
	}
	return 0;
}

int TrieDeletion(struct Trie **curr, const char *str) 
{
	struct Trie *t;
	int status = ENOMEM;
	size_t strlength = strlen(str);
	struct Trie **path = (struct Trie **)malloc(sizeof(struct Trie *) * (strlength + 1));

	while (NULL != path) {	

		if (0 == strlength)
		{
			status = 0;
			if (!haveChildren(*curr)) {
				free(*curr);
				*curr = NULL;
			}
			break;
		}

		memset(path, 0, sizeof(struct Trie *) * (strlen(str) + 1));
		status = 0;
		t = *curr;
		for (unsigned index = 0; index < strlength; index++) {
			path[index] = t;
			t = t->character[(uint8_t)str[index]];

			if (NULL == t) {
				status = ENOENT;
				break;
			}
		}

		// error path - didn't find the entry
		if (0 != status) {
			break;
		}

		// We found an entry - does it have an object?
		if (NULL == t->Object) {
			// no
			status = ENOENT;
			break;
		}

		// yes - in this case we may need to delete back up (which is why we saved the path)
		status = 0;
		t->Object = NULL;
		for (int index = strlen(str)-1; index >= 0; index--) {
			t = path[index];
			uint8_t ch = str[index];
			struct Trie *child = t->character[ch];

			if ((NULL != child->Object) || haveChildren(child)) {
				// we're done - either this is a valid leaf node
				// or it has children (or both)
				break;
			}

			// otherwise, we need to reclaim that space
			free(child);
			t->character[ch] = NULL; // this is the reference to child

		}

		if (t == *curr) {
			// this happens if we get to the end of the loop above
			if ((NULL != t->Object) || haveChildren(t)) {
				break; // can't delete this
			}

			// The trie is empty and we can delete it.
			free(t);
			*curr = NULL;
		}

		// Done.
		break;

	}

	if (NULL != path) {
		free(path);
	}

	return status;
}

#if 0
// Recursive function to delete a string in Trie
int TrieDeletion(struct Trie **curr, const char* str)
{
	int status = -ENOENT;

	while (NULL != *curr) {

		// if we have not reached the end of the string
		if (*str)
		{
			// recur for the node corresponding to next character in
			// the string and if it returns 1, delete current node
			// (if it is non-leaf)
			if ((*curr != NULL) && ((*curr)->character[(uint8_t)*str] != NULL) &&
				(0 <= TrieDeletion(&((*curr)->character[(uint8_t)*str]), str + 1)) &&
				(NULL == (*curr)->Object))
			{
				if (!haveChildren(*curr))
				{
					free(*curr);
					(*curr) = NULL;
					status = 0; // success
					break;
				}
				else {
					status = ENOTEMPTY; // has children
					break;
				}
			}
		}

		// if we have reached the end of the string
		if (*str == '\0')
		{
			// if current node is a leaf node and don't have any children
			if (!haveChildren(*curr))
			{
				free(*curr); // delete current node
				(*curr) = NULL;
				status = 0;
				break;
			}

			// if current node is a leaf node and has children
			else
			{
				// mark current node as non-leaf node (DON'T DELETE IT)
				(*curr)->Object = NULL;
				status = ENOTEMPTY;
				break;
			}
		}
	}

	return status;
}
#endif // 0

#if 0
// Trie Implementation in C - Insertion, Searching and Deletion
int main()
{
	struct Trie* head = getNewTrieNode();

	insert(head, "hello");
	printf("%d ", search(head, "hello"));   	// print 1

	insert(head, "helloworld");
	printf("%d ", search(head, "helloworld"));  // print 1

	printf("%d ", search(head, "helll"));   	// print 0 (Not present)

	insert(head, "hell");
	printf("%d ", search(head, "hell"));		// print 1

	insert(head, "h");
	printf("%d \n", search(head, "h")); 		// print 1 + newline

	deletion(&head, "hello");
	printf("%d ", search(head, "hello"));   	// print 0 (hello deleted)
	printf("%d ", search(head, "helloworld"));  // print 1
	printf("%d \n", search(head, "hell"));  	// print 1 + newline

	deletion(&head, "h");
	printf("%d ", search(head, "h"));   		// print 0 (h deleted)
	printf("%d ", search(head, "hell"));		// print 1
	printf("%d\n", search(head, "helloworld")); // print 1 + newline

	deletion(&head, "helloworld");
	printf("%d ", search(head, "helloworld"));  // print 0
	printf("%d ", search(head, "hell"));		// print 1

	deletion(&head, "hell");
	printf("%d\n", search(head, "hell"));   	// print 0 + newline

	if (head == NULL)
		printf("Trie empty!!\n");   			// Trie is empty now

	printf("%d ", search(head, "hell"));		// print 0

	return 0;
}
#endif // 0
