//
// (C) Copyright 2020 Tony Mason (fsgeek@cs.ubc.ca)
// All Rights Reserved
//

#if !defined(__TRIE_H__)
#define __TRIE_H__

struct Trie;

struct Trie* TrieCreateNode(void);
void TrieInsert(struct Trie *head, const char* str, void *object);
void *TrieSearch(struct Trie* head, const char* str);
int TrieDeletion(struct Trie **curr, const char* str);


#endif // __TRIE_H__
