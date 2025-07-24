#include <stdio.h>

int endian() {
    unsigned int x = 1;
    char *c = (char*)&x;
    if (*c)
        return 0;
    return 1;
}

unsigned int IntegerFromBytes(const char* bytes)
{
    if (endian() == 1)
        return *(unsigned int*)(bytes);

    // bytes must be in big endian order
    unsigned int ures = 0;

    for (int i = 0; i < sizeof(unsigned int); i++)
    {
        ures <<= sizeof(unsigned char)*8;
        ures |= (unsigned char)(bytes[i]);
    }

    return ures;
}

const char* const Print(const char* const params)
{
    unsigned int size = IntegerFromBytes(params);
    putchar('\n');
    for (int i = 0; i < size; i++)
        putchar(params[4+i]);
    printf(", printed in C!\n");
    return NULL;
}

// Function pointer types
typedef char (*binder_t)(void*, unsigned int, const char* const(*)(const char* const));
typedef char (*unbinder_t)(void*, unsigned int);

char InitExtender(void* sch, binder_t binder, unbinder_t unbinder)
{
    binder(sch, 13, &Print);
    return 0;
}
