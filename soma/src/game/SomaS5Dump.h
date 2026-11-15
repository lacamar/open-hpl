#ifndef SOMA_S5_DUMP_H
#define SOMA_S5_DUMP_H

bool OhplS5DumpEnabled();
void OhplS5Log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

unsigned OhplS5Bits(float afX);
void OhplS5SelfTest();

#endif
