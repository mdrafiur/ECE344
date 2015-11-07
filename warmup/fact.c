#include "common.h"
#include <stdbool.h>
#include <ctype.h>

bool isValidInt(char *str)
{
    // Handle empty string or negative numbers.
    if (str == NULL || *str == '-' || *str == '0')
        return false;

    // Check for non-digit chars in the rest of the stirng.
    while (*str)
    {
        if (!isdigit(*str))
            return false;

        else
            ++str;
    }

    return true;
}

int factorial(int n)
{
    if(n == 1)
        return 1;

    else
        return (n * factorial(n-1));
}

int
main(int argc, char *argv[])
{
    char *arg = argv[1];

    if(isValidInt(arg))
    {
        if(atoi(arg) > 12)
            printf("Overflow\n");

        else
            printf("%d\n", factorial(atoi(arg)));
    }

    else
        printf("Huh?\n");

    return 0;
}

