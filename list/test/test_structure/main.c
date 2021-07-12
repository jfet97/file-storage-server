#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "list.h"

typedef struct
{
    int age;
    char *name;
} Person;

void print(void *vp, int *error)
{
    Person *p = vp;

    printf("AGE: %zd --- ", p->age);
    printf("NAME: %s\n", p->name);
}

int compare_person(void *vp1, void *vp2)
{
    Person *p1 = vp1;
    Person *p2 = vp2;

    int cn = strcmp(p1->name, p2->name);

    if (cn == 0)
    {
        return p1->age - p2->age;
    }

    return cn;
}

void *duplicate_person(void *vp, int *error)
{
    Person *p = vp;
    Person *p1 = malloc(sizeof(*p1));

    if (p1 == NULL)
    {
        *error = -1;
    }
    else
    {
        p1->age = p->age;
        p1->name = strndup(p->name, strlen(p->name));
    }

    return p1;
}

void free_person(void *vp)
{
    Person *p = vp;
    free(p->name);
}
#define DEBUG_NUMBER(N, M) \
    printf("%zd should be %zd\n", N, M);

#define DEBUG_P_NUMBER(P, N) \
    DEBUG_NUMBER(*P, N);

#define DEBUG_POINTER(P, V) \
    printf("%p should be %p\n", P, V);

int findAgeThree(void *vp)
{
    Person *p = vp;
    return p->age == 3;
}

int findNameBarbara(void *vp)
{
    Person *p = vp;
    return strcmp(p->name, "Barbara") == 0;
}

int ageLessThanTen(void *vp)
{
    Person *p = vp;
    return p->age < 10;
}

int isAgeEven(void *vp)
{
    Person *p = vp;
    return p->age % 2 == 0;
}

int isAgeOdd(void *vp)
{
    return isAgeEven(vp) == 0;
}

void *times2Age(void *vp, int *error)
{
    Person *p = vp;
    Person *dp = malloc(sizeof(*dp));
    if (dp == NULL)
    {
        *error = -1;
    }
    else
    {
        dp->age = p->age * 2;
        dp->name = strndup(p->name, strlen(p->name));
    }
    return dp;
}

void *minus1Age(void *vp, int *error)
{
    Person *p = vp;
    Person *dp = malloc(sizeof(*dp));
    if (dp == NULL)
    {
        *error = -1;
    }
    else
    {
        dp->age = p->age - 1;
        dp->name = strndup(p->name, strlen(p->name));
    }
    return dp;
}

void *sumAges(void *acc, void *vp, int *error)
{
    int *currSum = acc;
    Person *p = vp;
    *currSum += p->age;
    return acc;
}

void *productAges(void *acc, void *vp, int *error)
{
    int *currProd = acc;
    Person *p = vp;
    *currProd *= p->age;
    return acc;
}

int sameAge(void *x, void *y)
{
    Person *a = x;
    Person *b = y;
    return a->age == b->age;
}
int sameName(void *x, void *y)
{
    Person *a = x;
    Person *b = y;
    return a->name == b->name;
}

int main()
{

    int error = 0;

    List_T people = List_create(compare_person, duplicate_person, free_person, &error);

    Person *p1 = malloc(sizeof(*p1));
    Person *p2 = malloc(sizeof(*p2));
    Person *p3 = malloc(sizeof(*p3));
    Person *p4 = malloc(sizeof(*p4));
    Person *p5 = malloc(sizeof(*p5));

    p1->age = 1;
    p1->name = malloc(1000);
    scanf("%s", p1->name);

    p2->age = 2;
    p2->name = malloc(1000);
    scanf("%s", p2->name);

    p3->age = 3;
    p3->name = malloc(1000);
    scanf("%s", p3->name);

    p4->age = 4;
    p4->name = malloc(1000);
    scanf("%s", p4->name);

    p5->age = 5;
    p5->name = malloc(1000);
    scanf("%s", p5->name);

    // -----------------------------------

    // List_insertHead(people, p1, &error);
    // List_insertHead(people, p2, &error);
    // List_insertHead(people, p3, &error);
    // List_insertHead(people, p4, &error);
    // List_insertHead(people, p5, &error);

    // -----------------------------------

    // List_insertOrdered(people, p1, 0, &error);
    // List_insertOrdered(people, p2, 0, &error);
    // List_insertOrdered(people, p3, 0, &error);
    // List_insertOrdered(people, p4, 0, &error);
    // List_insertOrdered(people, p5, 0, &error);

    // -----------------------------------

    // List_insertOrdered(people, p1, 1, &error);
    // List_insertOrdered(people, p2, 1, &error);
    // List_insertOrdered(people, p3, 1, &error);
    // List_insertOrdered(people, p4, 1, &error);
    // List_insertOrdered(people, p5, 1, &error);

    // -----------------------------------

    // List_insertTail(people, p1, &error);
    // List_insertTail(people, p2, &error);
    // List_insertTail(people, p3, &error);
    // List_insertTail(people, p4, &error);
    // List_insertTail(people, p5, &error);

    // -----------------------------------

    // puts("--------------------------------------------");

    // Person *p_1 = List_pickHead(people, &error);
    // print(p_1, &error);
    // Person *p_5 = List_pickTail(people, &error);
    // print(p_5, &error);
    // p_1 = List_pickHead(people, &error);
    // p_5 = List_pickTail(people, &error);
    // print(p_1, &error);
    // print(p_5, &error);

    // Person *p_3 = List_findPick(people, findAgeThree, &error);
    // print(p_3, &error);
    // p_3 = List_findPick(people, findAgeThree, &error);
    // print(p_3, &error);
    // Person *p_B = List_findPick(people, findNameBarbara, &error);
    // DEBUG_POINTER(p_B, NULL);
    // puts("--------------------------------------------");

    // List_forEach(people, print);

    // -----------------------------------

    // puts("--------------------------------------------");

    // Person *p_1 = List_extractHead(people, &error);
    // print(p_1, &error);
    // Person *p_5 = List_extractTail(people, &error);
    // print(p_5, &error);

    // DEBUG_NUMBER((int)List_length(people, &error), 3)

    // Person *p_2 = List_extractHead(people, &error);
    // Person *p_4 = List_extractTail(people, &error);
    // print(p_2, &error);
    // print(p_4, &error);

    // DEBUG_NUMBER((int)List_length(people, &error), 1)

    // Person *p_3 = List_findExtract(people, findAgeThree, &error);
    // print(p_3, &error);
    // p_3 = List_findExtract(people, findAgeThree, &error);
    // DEBUG_POINTER(p_3, NULL);

    // Person *p_B = List_findExtract(people, findNameBarbara, &error);
    // DEBUG_POINTER(p_B, NULL);

    // DEBUG_NUMBER((int)List_length(people, &error), 0)
    // puts("--------------------------------------------");

    // -----------------------------------
    // List_extractHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 4)
    // List_extractHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 3)
    // List_extractHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 2)
    // List_extractHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 1)
    // List_extractHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 0)
    // List_extractHead(people, &error);
    // List_extractHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 0)
    // -----------------------------------

    // List_extractTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 4)
    // List_extractTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 3)
    // List_extractTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 2)
    // List_extractTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 1)
    // List_extractTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 0)
    // List_extractTail(people, &error);
    // List_extractTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 0)

    // -----------------------------------

    // puts("--------------------------------------------");

    // List_deleteHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 4)
    // List_deleteTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 3)
    // List_deleteHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 2)
    // List_deleteTail(people, &error);

    // DEBUG_NUMBER((int)List_length(people, &error), 1)

    // List_findDelete(people, findAgeThree, &error);
    // List_findDelete(people, findNameBarbara, &error);

    // List_deleteTail(people, &error);
    // List_deleteTail(people, &error);
    // List_deleteTail(people, &error);
    // List_deleteHead(people, &error);
    // List_deleteHead(people, &error);
    // List_deleteHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 0);

    // puts("--------------------------------------------");

    // -----------------------------------
    // List_deleteTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 4)
    // List_deleteTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 3)
    // List_deleteTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 2)
    // List_deleteTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 1)
    // List_deleteTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 0)
    // List_deleteTail(people, &error);
    // List_deleteTail(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 0)
    // -----------------------------------

    // List_deleteHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 4)
    // List_deleteHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 3)
    // List_deleteHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 2)
    // List_deleteHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 1)
    // List_deleteHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 0)
    // List_deleteHead(people, &error);
    // List_deleteHead(people, &error);
    // DEBUG_NUMBER((int)List_length(people, &error), 0)

    // -----------------------------------

    // DEBUG_NUMBER(List_any(people, findAgeThree, &error), 1);
    // DEBUG_NUMBER(List_any(people, findNameBarbara, &error), 0);

    // DEBUG_NUMBER(List_all(people, ageLessThanTen, &error), 1);
    // DEBUG_NUMBER(List_all(people, findAgeThree, &error), 0);

    // List_forEach(people, print, &error);

    // -----------------------------------

    // List_forEach(people, print, &error);
    // List_forEach_right(people, print, &error);

    // -----------------------------------

    // List_T oddPeople = List_filter(people, isAgeOdd, 0, &error);
    // List_forEach(oddPeople, print, &error);
    // puts("--------------------------------------------");
    // List_T evenPeople = List_filter(people, isAgeEven, 0, &error);
    // List_forEach(evenPeople, print, &error);
    // puts("--------------------------------------------");

    // List_free(&oddPeople, 0, &error);
    // List_free(&evenPeople, 0, &error);

    // oddPeople = List_filter(people, isAgeOdd, 1, &error);
    // List_forEach(oddPeople, print, &error);
    // puts("--------------------------------------------");
    // evenPeople = List_filter(people, isAgeEven, 1, &error);
    // List_forEach(evenPeople, print, &error);
    // puts("--------------------------------------------");

    // List_free(&oddPeople, 1, &error);
    // List_free(&evenPeople, 1, &error);

    // -----------------------------------

    // List_T timesTwoPeople = List_map(people, times2Age, List_getComparator(people, &error), List_getDuplicator(people, &error), List_getDeallocator(people, &error), &error);
    // List_forEach(timesTwoPeople, print, &error);
    // List_T minusOnePeople = List_map(people, minus1Age, List_getComparator(people, &error), List_getDuplicator(people, &error), List_getDeallocator(people, &error), &error);
    // List_forEach(minusOnePeople, print, &error);

    // List_free(&timesTwoPeople, 1, &error);
    // List_free(&minusOnePeople, 1, &error);

    // -----------------------------------
    // int *s = malloc(sizeof(*s));
    // if(s != NULL) {
    //     *s = 0;
    //     List_reduce(people, sumAges, s, &error);
    //     DEBUG_P_NUMBER(s, 15);
    // }

    // int *p = malloc(sizeof(*p));
    // if(p != NULL) {
    //     *p = 1;
    //     List_reduce(people, productAges, p, &error);
    //     DEBUG_P_NUMBER(p, 120);
    // }

    // free(s);
    // free(p);

    // -----------------------------------

    // Person f1;
    // f1.age = 4;
    // f1.name = "test";

    // DEBUG_NUMBER(List_search(people, sameAge, &f1, &error), 1);
    // DEBUG_NUMBER(List_search(people, sameName, &f1, &error), 0);

    // -----------------------------------
    Person f1;
    f1.age = 4;
    f1.name = "test";

    List_insertHead(people, p4, &error);

    Person *pr1 = List_searchExtract(people, sameAge, &f1, &error);
    printf("%zd should be 4\n", pr1->age);
    DEBUG_NUMBER(List_length(people, &error), 0);
    List_forEach(people, print, &error);

    puts("----------------------");

    List_insertHead(people, p4, &error);
    List_insertHead(people, p5, &error);
    pr1 = List_searchExtract(people, sameAge, &f1, &error);
    printf("%zd should be 4\n", pr1->age);
    DEBUG_NUMBER(List_length(people, &error), 1);
    List_forEach(people, print, &error);

    puts("----------------------");

    List_insertHead(people, p4, &error);
    pr1 = List_searchExtract(people, sameAge, &f1, &error);
    printf("%zd should be 4\n", pr1->age);
    DEBUG_NUMBER(List_length(people, &error), 1);
    List_forEach(people, print, &error);

    puts("----------------------");

    List_insertHead(people, p4, &error);
    List_insertHead(people, p3, &error);
    pr1 = List_searchExtract(people, sameAge, &f1, &error);
    printf("%zd should be 4\n", pr1->age);
    DEBUG_NUMBER(List_length(people, &error), 2);
    List_forEach(people, print, &error);

    puts("----------------------");

    // -----------------------------------

    // List_forEach(people, print, &error);

    // List_reverse(people, &error);

    // List_forEach_right(people, print, &error);

    // -----------------------------------

    List_free(&people, 1, &error);

    return 0;
}