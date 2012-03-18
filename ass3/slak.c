#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <slack/std.h>
#include <slack/list.h>

void main()
{
    List * l = list_create(NULL);
    l = list_append(l, 1);
    l = list_append(l, 2);
    l = list_append(l, 3);
    l = list_append(l, 4);

    while(1)
    {
        int q = list_shift_int(l);
        printf("value = %d\n",q);
        l = list_append(l,q);
    }
}
