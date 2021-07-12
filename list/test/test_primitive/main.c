#include <stdio.h>
#include <stdlib.h>
#include "list.h"

int compareNumbers(void *vn1, void *vn2)
{
    int *pn1 = vn1;
    int *pn2 = vn2;
    return *pn1 - *pn2;
}

void *duplicateNumber(void *vn, int *error)
{
    int *pn = vn;
    int *pnd = malloc(sizeof(*pnd));
    if (pnd == NULL)
    {
        *error = -1;
    }
    else
    {
        *pnd = *pn;
    }

    return pnd;
}

void print(void *vn, int *_)
{
    int *pn = vn;
    printf("%zd\n", *pn);
}

#define DEBUG_NUMBER(N, M) \
    printf("%zd should be %zd\n", N, M);

#define DEBUG_P_NUMBER(P, N) \
    DEBUG_NUMBER(*P, N);

#define DEBUG_POINTER(P, V) \
    printf("%p should be %p\n", P, V);

int findThree(void *vnp)
{
    int *np = vnp;
    return *np == 3;
}

int findSix(void *vnp)
{
    int *np = vnp;
    return *np == 6;
}

int lessThanTen(void *vnp)
{
    int *np = vnp;
    return *np < 10;
}

int isEven(void *vnp)
{
    int *np = vnp;
    return *np % 2 == 0;
}

int isOdd(void *vnp)
{
    return isEven(vnp) == 0;
}

void *times2(void *vnp, int *error)
{
    int *np = vnp;
    int *dp = malloc(sizeof(*dp));
    if (dp == NULL)
    {
        *error = -1;
    }
    else
    {
        *dp = *np * 2;
    }
    return dp;
}

void *minus1(void *vnp, int *error)
{
    int *np = vnp;
    int *mp = malloc(sizeof(*mp));
    if (mp == NULL)
    {
        *error = -1;
    }
    else
    {
        *mp = *np - 1;
    }
    return mp;
}

void *sum(void *acc, void *vnp, int *_)
{
    int *currSum = acc;
    int *np = vnp;
    *currSum += *np;
    return acc;
}

void *product(void *acc, void *vnp, int *_)
{
    int *currProd = acc;
    int *np = vnp;
    *currProd *= *np;
    return acc;
}

int main()
{

    int error;

    List_T numbers = List_create(compareNumbers, duplicateNumber, NULL, &error);

    int *n1 = malloc(sizeof(*n1));
    int *n2 = malloc(sizeof(*n2));
    int *n3 = malloc(sizeof(*n3));
    int *n4 = malloc(sizeof(*n4));
    int *n5 = malloc(sizeof(*n5));

    *n1 = 1;
    *n2 = 2;
    *n3 = 3;
    *n4 = 4;
    *n5 = 5;

    // -----------------------------------

    // List_insertHead(numbers, n1, &error);
    // List_insertHead(numbers, n2, &error);
    // List_insertHead(numbers, n3, &error);
    // List_insertHead(numbers, n4, &error);
    // List_insertHead(numbers, n5, &error);

    // -----------------------------------

    // List_insertOrdered(numbers, n1, 0, &error);
    // List_insertOrdered(numbers, n2, 0, &error);
    // List_insertOrdered(numbers, n3, 0, &error);
    // List_insertOrdered(numbers, n4, 0, &error);
    // List_insertOrdered(numbers, n5, 0, &error);

    // -----------------------------------

    // List_insertOrdered(numbers, n1, 1, &error);
    // List_insertOrdered(numbers, n2, 1, &error);
    // List_insertOrdered(numbers, n3, 1, &error);
    // List_insertOrdered(numbers, n4, 1, &error);
    // List_insertOrdered(numbers, n5, 1, &error);

    // -----------------------------------

    List_insertTail(numbers, n1, &error);
    List_insertTail(numbers, n2, &error);
    List_insertTail(numbers, n3, &error);
    List_insertTail(numbers, n4, &error);
    List_insertTail(numbers, n5, &error);

    // -----------------------------------

    // int *n_1 = List_pickHead(numbers, &error);
    // int *n_5 = List_pickTail(numbers, &error);
    // n_1 = List_pickHead(numbers, &error);
    // n_5 = List_pickTail(numbers, &error);

    // DEBUG_P_NUMBER(n_1, 1);
    // DEBUG_P_NUMBER(n_5, 5);

    // int *n_3 = List_findPick(numbers, findThree, &error);
    // n_3 = List_findPick(numbers, findThree, &error);
    // int *n_6 = List_findPick(numbers, findSix, &error);

    // DEBUG_P_NUMBER(n_3, 3);
    // DEBUG_POINTER(n_6, NULL);

    // List_forEach(numbers, print, &error);

    // -----------------------------------

    // int *n_1 = List_extractHead(numbers, &error);
    // int *n_5 = List_extractTail(numbers, &error);

    // DEBUG_P_NUMBER(n_1, 1);
    // DEBUG_P_NUMBER(n_5, 5);

    // int *n_3 = List_findExtract(numbers, findThree, &error);
    // int *n_6 = List_findExtract(numbers, findSix, &error);

    // DEBUG_P_NUMBER(n_3, 3);
    // DEBUG_POINTER(n_6, NULL);

    // DEBUG_NUMBER((int)List_length(numbers, &error), 2);

    // List_forEach(numbers, print, &error);

    // -----------------------------------

    // List_deleteHead(numbers, &error);
    // List_deleteTail(numbers, &error);
    // List_deleteHead(numbers, &error);
    // List_deleteTail(numbers, &error);

    // DEBUG_NUMBER((int)List_length(numbers, &error), 1);

    // List_findDelete(numbers, findThree, &error);
    // List_findDelete(numbers, findSix, &error);

    // DEBUG_NUMBER((int)List_length(numbers, &error), 0);

    // -----------------------------------

    // DEBUG_NUMBER(List_any(numbers, findThree, &error), 1);
    // DEBUG_NUMBER(List_any(numbers, findSix, &error), 0);

    // DEBUG_NUMBER(List_all(numbers, lessThanTen, &error), 1);
    // DEBUG_NUMBER(List_all(numbers, findThree, &error), 0);

    // List_forEach(numbers, print, &error);

    // -----------------------------------

    // List_forEach(numbers, print, &error);
    // List_forEach_right(numbers, print, &error);

    // -----------------------------------

    // List_T oddNumbers = List_filter(numbers, isOdd, 1, &error);
    // List_forEach(oddNumbers, print, &error);
    // List_T evenNumbers = List_filter(numbers, isEven, 1, &error);
    // List_forEach(evenNumbers, print, &error);

    // List_free(&oddNumbers, 1, &error);
    // List_free(&evenNumbers, 1, &error);

    // oddNumbers = List_filter(numbers, isOdd, 0, &error);
    // List_forEach(oddNumbers, print, &error);
    // evenNumbers = List_filter(numbers, isEven, 0, &error);
    // List_forEach(evenNumbers, print, &error);

    // List_free(&oddNumbers, 0, &error);
    // List_free(&evenNumbers, 0, &error);

    // -----------------------------------

    // List_T timesTwoNumbers = List_map(numbers, times2, List_getComparator(numbers, &error), List_getDuplicator(numbers, &error), List_getDeallocator(numbers, &error), &error);
    // List_forEach(timesTwoNumbers, print, &error);
    // List_T minusOneNumbers = List_map(numbers, minus1, List_getComparator(numbers, &error), List_getDuplicator(numbers, &error), List_getDeallocator(numbers, &error), &error);
    // List_forEach(minusOneNumbers, print, &error);

    // List_free(&timesTwoNumbers, 1, &error);
    // List_free(&minusOneNumbers, 1, &error);

    // -----------------------------------
    // int *s = malloc(sizeof(*s));
    // if(s != NULL) {
    //     *s = 0;
    //     List_reduce(numbers, sum, s, &error);
    //     DEBUG_P_NUMBER(s, 15);
    // }

    // int *p = malloc(sizeof(*p));
    // if(p != NULL) {
    //     *p = 1;
    //     List_reduce(numbers, product, p, &error);
    //     DEBUG_P_NUMBER(p, 120);
    // }

    // free(s);
    // free(p);

    // -----------------------------------

    List_forEach(numbers, print, &error);

    List_reverse(numbers, &error);

    List_forEach_right(numbers, print, &error);

    // -----------------------------------

    List_free(&numbers, 1, &error);

    return 0;
}