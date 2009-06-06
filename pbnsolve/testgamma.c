#include "pbnsolve.h"

main()
{
    init_factln();
    printf("1 0 %d\n",bico(1,0));
    printf("1 1 %d\n",bico(1,1));
    printf("2 1 %d\n",bico(2,1));
    printf("3 1 %d\n",bico(3,1));
    printf("4 2 %d\n",bico(4,2));
    printf("5 2 %d\n",bico(5,2));
    printf("6 2 %d\n",bico(6,2));
    printf("6 3 %d\n",bico(6,3));
}
