#ifndef STRINGPOOL_H
#define STRINGPOOL_H

extern char g_txtHeapBuffer[0x1002];
extern short g_strOffsetTbl[128];
extern short g_stringPoolLens[128];

int ParseFileForSection(const char *path, const char *section);

void StringReplace(const char *find, const char *repl);

void ClearStringPool();

int StoreStringInPool(int slot);

int AppendPooledString(int slot);

#endif
