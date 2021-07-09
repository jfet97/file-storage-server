#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "list.h"

#define T List_T

#define CHECK_POINTER_RETURN(P, R) \
    if (P == NULL)                 \
    {                              \
        return R;                  \
    }

#define CHECK_POINTER_RETURN_NULL(P) \
    CHECK_POINTER_RETURN(P, NULL)

#define CHECK_POINTER_RETURN_VOID(P) \
    if (P == NULL)                   \
    {                                \
        return;                      \
    }

#define IF_NOT_NULL(P, CODE) \
    if (P != NULL)           \
    {                        \
        CODE                 \
    }

#define IF_NULL(P, CODE) \
    if (P == NULL)       \
    {                    \
        CODE             \
    }

#define SET_ERROR error && (*error = errToSet);

struct Node
{
    void *element;
    struct Node *next;
    struct Node *prev;
} Node;

struct T
{
    struct Node *head;
    struct Node *tail;
    size_t length;
    Comparator comparator;
    Duplicator duplicator;
    Deallocator deallocator;
} ListEntity;

// -------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// CREATE OR DELETE A NODE

static struct Node *Node_create(void *element, struct Node *next, struct Node *prev)
{
    struct Node *newNode = malloc(sizeof(*newNode));

    CHECK_POINTER_RETURN_NULL(newNode);

    newNode->element = element;
    newNode->next = next;
    newNode->prev = prev;

    return newNode;
}

static void Node_free(struct Node **nodePtr, int freeElement)
{
    CHECK_POINTER_RETURN_VOID(*nodePtr);

    if (freeElement == 1)
    {
        free((*nodePtr)->element);
    }
    free(*nodePtr);
    *nodePtr = NULL;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// CREATE / DELETE A LIST

static int defaultComparator(void *x, void *y)
{
    return x == y;
}

static void *defaultDuplicator(void *x, int *error)
{
    return x;
}

T List_create(Comparator c, Duplicator du, Deallocator de, int *error)
{
    int errToSet = 0;
    T newList = malloc(sizeof(*newList));

    if (newList == NULL)
    {
        errToSet = E_LIST_MALLOC;
    }

    if (!errToSet)
    {
        newList->length = 0;
        newList->head = NULL;
        newList->tail = NULL;
        newList->comparator = c ? c : defaultComparator;
        newList->duplicator = du ? du : defaultDuplicator;
        newList->deallocator = de;
    }

    SET_ERROR
    return newList;
}

static void List__free(struct Node **nodePtr, Deallocator d, int freeElement)
{
    IF_NOT_NULL(
        *nodePtr,
        List__free(&(*nodePtr)->next, d, freeElement);
        if (freeElement == 1) {
            IF_NOT_NULL(d, d((*nodePtr)->element););
        });
    Node_free(nodePtr, freeElement);
}

void List_free(T *listPtr, int freeElement, int *error)
{
    int errToSet = 0;

    if (listPtr == NULL || *listPtr == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet)
    {
        if ((*listPtr)->length != 0)
        {
            List__free(&(*listPtr)->head, (*listPtr)->deallocator, freeElement);
        }

        free(*listPtr);
        *listPtr = NULL;
    }

    SET_ERROR
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// RETRIEVE COMPARATOR / DEALLOCATOR

Comparator List_getComparator(T list, int *error)
{
    int errToSet = 0;
    Comparator c = NULL;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet)
    {
        c = list->comparator;
    }

    SET_ERROR
    return c;
}

Duplicator List_getDuplicator(T list, int *error)
{
    int errToSet = 0;
    Duplicator d = NULL;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet)
    {
        d = list->duplicator;
    }

    SET_ERROR
    return d;
}

Deallocator List_getDeallocator(T list, int *error)
{
    int errToSet = 0;
    Deallocator d = NULL;
    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet)
    {
        d = list->deallocator;
    }

    SET_ERROR
    return d;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// INSERT REPLACING THE HEAD / TAIL

void List_insertHead(T list, void *element, int *error)
{ // O(1)

    int errToSet = 0;
    struct Node *newNode = NULL;
    size_t listLength = 0;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet && element == NULL)
    {
        errToSet = E_LIST_NULL_ELEMENT;
    }

    if (!errToSet)
    {
        newNode = Node_create(element, list->head, NULL);

        if (newNode == NULL)
        {
            errToSet = E_LIST_MALLOC;
        }
    }

    if (!errToSet)
    {
        listLength = List_length(list, &errToSet);
    }

    if (!errToSet)
    {

        if (listLength == 0)
        {
            list->tail = newNode;
        }
        else
        {
            list->head->prev = newNode;
        }

        list->head = newNode;
        list->length++;
    }

    SET_ERROR
}

void List_insertTail(T list, void *element, int *error)
{ // O(1)

    int errToSet = 0;
    struct Node *newNode = NULL;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet && element == NULL)
    {
        errToSet = E_LIST_NULL_ELEMENT;
    }

    if (!errToSet)
    {
        newNode = Node_create(element, NULL, list->tail);

        if (newNode == NULL)
        {
            errToSet = E_LIST_MALLOC;
        }
    }

    if (!errToSet)
    {
        if (list->tail != NULL)
        {
            // list->tail == NULL <=> list.length == 0
            list->tail->next = newNode;
        }
        else
        {
            list->head = newNode;
        }

        list->tail = newNode;
        list->length++;
    }

    SET_ERROR
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// INSERT MAINTAINING AN ORDER

static void List__insertOrdered(struct Node **nodePtr, void *element, Comparator c, int order)
{
    void *currentElement = (*nodePtr)->next->element;

    int compareResult = c(currentElement, element);

    int shouldIInsertBeforeCurrentElement = (compareResult >= 0 && order == 1) || (compareResult < 0 && order == 0);

    if (shouldIInsertBeforeCurrentElement)
    {
        struct Node *newNode = Node_create(element, (*nodePtr)->next, *nodePtr);
        CHECK_POINTER_RETURN_VOID(newNode);
        (*nodePtr)->next->prev = newNode;
        (*nodePtr)->next = newNode;
    }
    else
    {
        List__insertOrdered(&(*nodePtr)->next, element, c, order);
    }
}

void List_insertOrdered(T list, void *element, int order, int *error)
{
    Comparator c = NULL;
    int errToSet = 0;
    size_t listLength = 0;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet && element == NULL)
    {
        errToSet = E_LIST_NULL_ELEMENT;
    }

    if (!errToSet)
    {
        c = list->comparator;
        int innerError = 0;

        listLength = List_length(list, &innerError);

        errToSet = innerError;
    }

    if (!errToSet && listLength == 0)
    {
        int innerError = 0;

        List_insertHead(list, element, &innerError);

        errToSet = innerError;
    }

    if (!errToSet && listLength != 0)
    {
        int innerError = 0;

        int compareResultH = c(list->head->element, element);
        // the list has at least one element
        if ((compareResultH < 0 && order == 0) || (compareResultH >= 0 && order == 1))
        {
            List_insertHead(list, element, &innerError);
            errToSet = innerError;
            SET_ERROR;
            return;
        }

        int compareResultT = c(list->tail->element, element);
        if ((compareResultT >= 0 && order == 0) || (compareResultT < 0 && order == 1))
        {
            List_insertTail(list, element, &innerError);
            errToSet = innerError;
            SET_ERROR;
            return;
        }

        // the list has at least two elements
        // and the new one hasn't to be inserted as the first one nor as the last one
        struct Node **firstNodePtr = &(list->head);
        List__insertOrdered(firstNodePtr, element, c, order);
        list->length++;
    }

    SET_ERROR
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// PICK AN ELEMENT FROM THE HEAD / TAIL

void *List_pickHead(T list, int *error)
{
    int errToSet = 0;
    void *toRet = NULL;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }
    else if (list->head == NULL)
    {
        errToSet = E_LIST_EMPTY_LIST;
    }
    else
    {
        toRet = list->head->element;
    }

    SET_ERROR
    return toRet;
}

void *List_pickTail(T list, int *error)
{
    int errToSet = 0;
    void *toRet = NULL;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }
    else if (list->head == NULL)
    {
        errToSet = E_LIST_EMPTY_LIST;
    }
    else
    {
        toRet = list->tail->element;
    }

    SET_ERROR
    return toRet;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// FIND AND PICK AN ELEMENT

static struct Node *List__findNode(struct Node *node, Predicate f)
{
    CHECK_POINTER_RETURN_NULL(node);

    if (f(node->element) == 1)
    {
        return node;
    }
    else
    {
        return List__findNode(node->next, f);
    }
}

static void *List__find(struct Node *node, Predicate f)
{
    struct Node *foundNode = List__findNode(node, f);

    CHECK_POINTER_RETURN_NULL(foundNode);

    return foundNode->element;
}

void *List_findPick(T list, Predicate f, int *error)
{
    int errToSet = 0;
    void *toRet = NULL;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }
    else
    {
        toRet = List__find(list->head, f);
    }

    SET_ERROR;
    return toRet;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// EXTRACT AN ELEMENT FROM THE HEAD / TAIL

void *List_extractHead(T list, int *error)
{
    int errToSet = 0;
    size_t listLength = 0;
    void *elementToRet = NULL;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet && list->head == NULL)
    {
        errToSet = E_LIST_EMPTY_LIST;
    }

    if (!errToSet)
    {
        int innerError = 0;

        listLength = List_length(list, &innerError);

        errToSet = innerError;
    }

    if (!errToSet)
    {
        struct Node *nodeToExtract = list->head;
        elementToRet = nodeToExtract->element;

        if (listLength == 1)
        {
            list->head = NULL;
            list->tail = NULL;
        }

        if (listLength > 1)
        {
            list->head = list->head->next;
            list->head->prev = NULL;
        }

        Node_free(&nodeToExtract, 0);

        list->length--;
    }

    SET_ERROR;
    return elementToRet;
}

void *List_extractTail(T list, int *error)
{
    int errToSet = 0;
    size_t listLength = 0;
    void *elementToRet = NULL;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet && list->tail == NULL)
    {
        errToSet = E_LIST_EMPTY_LIST;
    }

    if (!errToSet)
    {
        int innerError = 0;

        listLength = List_length(list, &innerError);

        errToSet = innerError;
    }

    if (!errToSet)
    {
        struct Node *nodeToExtract = list->tail;
        elementToRet = nodeToExtract->element;

        if (listLength == 1)
        {
            list->head = NULL;
            list->tail = NULL;
        }
        if (listLength > 1)
        {
            list->tail = list->tail->prev;
            list->tail->next = NULL;
        }

        Node_free(&nodeToExtract, 0);

        list->length--;
    }

    SET_ERROR;
    return elementToRet;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// FIND AND EXTRACT AN ELEMENT

void *List_findExtract(T list, Predicate f, int *error)
{
    int errToSet = 0;
    void *elementToRet = NULL;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet)
    {
        struct Node *nodeToExtract = List__findNode(list->head, f);

        if (nodeToExtract)
        {
            if (nodeToExtract == list->head)
            {
                return List_extractHead(list, error);
            }
            else if (nodeToExtract == list->tail)
            {
                return List_extractTail(list, error);
            }
            else
            {
                elementToRet = nodeToExtract->element;

                nodeToExtract->next->prev = nodeToExtract->prev;
                nodeToExtract->prev->next = nodeToExtract->next;

                Node_free(&nodeToExtract, 0);

                list->length--;
            }
        }
    }

    SET_ERROR;
    return elementToRet;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// DELETE HEAD / TAIL
void List_deleteHead(T list, int *error)
{

    int errToSet = 0;
    size_t listLength = 0;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    // empty list
    if (!errToSet && list->head == NULL)
    {
        errToSet = E_LIST_EMPTY_LIST;
    }

    if (!errToSet)
    {
        int innerError = 0;

        listLength = List_length(list, &innerError);

        errToSet = innerError;
    }

    if (!errToSet)
    {
        struct Node *nodeToRemove = list->head;

        if (listLength == 1)
        {
            list->head = NULL;
            list->tail = NULL;
        }

        if (listLength > 1)
        {
            list->head = list->head->next;
            list->head->prev = NULL;
        }

        IF_NOT_NULL(
            list->deallocator,
            list->deallocator(nodeToRemove->element););

        Node_free(&nodeToRemove, 1);

        list->length--;
    }

    SET_ERROR;
}

void List_deleteTail(T list, int *error)
{
    int errToSet = 0;
    size_t listLength = 0;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    // empty list
    if (!errToSet && list->tail == NULL)
    {
        errToSet = E_LIST_EMPTY_LIST;
    }

    if (!errToSet)
    {
        int innerError = 0;

        listLength = List_length(list, &innerError);

        errToSet = innerError;
    }

    if (!errToSet)
    {
        struct Node *nodeToRemove = list->tail;

        if (listLength == 1)
        {
            list->head = NULL;
            list->tail = NULL;
        }
        if (listLength > 1)
        {
            list->tail = list->tail->prev;
            list->tail->next = NULL;
        }

        IF_NOT_NULL(
            list->deallocator,
            list->deallocator(nodeToRemove->element););

        Node_free(&nodeToRemove, 1);

        list->length--;
    }

    SET_ERROR;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// FIND AND DELETE AN ELEMENT

void List_findDelete(T list, Predicate f, int *error)
{
    int errToSet = 0;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet)
    {
        struct Node *nodeToRemove = List__findNode(list->head, f);

        if (nodeToRemove)
        {
            if (nodeToRemove == list->head)
            {
                List_deleteHead(list, error);
                return;
            }

            if (nodeToRemove == list->tail)
            {
                List_deleteTail(list, error);
                return;
            }
            else
            {
                nodeToRemove->next->prev = nodeToRemove->prev;
                nodeToRemove->prev->next = nodeToRemove->next;

                IF_NOT_NULL(
                    list->deallocator,
                    list->deallocator(nodeToRemove->element););

                Node_free(&nodeToRemove, 1);

                list->length--;
            }
        }
    }

    SET_ERROR;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// CHECK IF AN ELEMENT SATISFIES A PREDICATE

int List_any(T list, Predicate f, int *error)
{
    int errToSet = 0;
    int toRet = 0;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet)
    {
        toRet = list->head ? List__find(list->head, f) != NULL : 0;
    }

    SET_ERROR;
    return toRet;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// CHECK IF ALL ELEMENTS SATISFY A PREDICATE

static int List__all(struct Node *node, Predicate f)
{
    CHECK_POINTER_RETURN(node, 1);

    if (f(node->element) == 1)
    {
        return List__all(node->next, f);
    }
    else
    {
        return 0;
    }
}

int List_all(T list, Predicate f, int *error)
{
    int errToSet = 0;
    int toRet = 1;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet)
    {
        toRet = List__all(list->head, f);
    }

    SET_ERROR;
    return toRet;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// GET THE LENGTH

size_t List_length(T list, int *error)
{
    int errToSet = 0;
    size_t toRet = 0;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }
    else
    {
        toRet = list->length;
    }

    SET_ERROR;
    return toRet;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// FIND SOMETHING KNOWN

extern int List_search(T list, Comparator c, void *toFind, int *error)
{
    int errToSet = 0;
    size_t found = 0;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }
    else
    {
        struct Node *runner = list->head;
        while (runner != NULL && !found)
        {
            found = c(toFind, runner->element);
            runner = runner->next;
        }
    }

    SET_ERROR;
    return found;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// FIND SOMETHING KNOWN

extern void* List_searchExtract(T list, Comparator c, void *toFind, int *error)
{
    int errToSet = 0;
    struct Node *nodeToExtract = NULL;
    void *toRet = NULL;
    size_t found = 0;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }
    else
    {
        struct Node *runner = list->head;
        while (runner != NULL && !found)
        {
            found = c(toFind, runner->element);
            nodeToExtract = runner;
            runner = runner->next;
        }
    }

    if (found)
    {
        if (nodeToExtract == list->head)
        {
            toRet = List_extractHead(list, &errToSet);
        }
        else if (nodeToExtract == list->tail)
        {
            toRet = List_extractTail(list, &errToSet);
        }
        else
        {
            toRet = nodeToExtract->element;
            nodeToExtract->next->prev = nodeToExtract->prev;
            nodeToExtract->prev->next = nodeToExtract->next;

            Node_free(&nodeToExtract, 0);
            list->length--;
        }
    }

    SET_ERROR;
    return toRet;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// REVERSE THE LIST

static struct Node *List__reverse(struct Node *node)
{
    // READ node == read current node address

    struct Node *self = node;

    struct Node *futureHead = NULL;
    struct Node *futureSelfNext = NULL;
    struct Node *futureSelfPrev = NULL;

    if (self->next == NULL)
    {
        // in this case self is the address of
        // the old last element aka the new head
        futureHead = self;

        futureSelfNext = self->prev;
        futureSelfPrev = NULL;
    }
    else
    {
        // reverse the remaining list
        futureHead = List__reverse(self->next);

        // self->next is the address of
        // the old next element that is going
        // to be the previous element

        futureSelfNext = self->prev;
        futureSelfPrev = self->next;
    }

    // rotate pointers
    self->next = futureSelfNext;
    self->prev = futureSelfPrev;

    // propagate back the address of the new head
    return futureHead;
}

void List_reverse(T list, int *error)
{
    int errToSet = 0;
    size_t listLength = 0;
    int alreadyReversed = 0;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet)
    {
        int innerError = 0;

        listLength = List_length(list, &innerError);

        errToSet = innerError;
    }

    if (!errToSet && listLength <= 1)
    {
        alreadyReversed = 1;
    }

    if (!errToSet && !alreadyReversed)
    {
        // there are at least 2 elements
        struct Node *newTail = list->head;
        list->head = List__reverse(list->head);
        list->tail = newTail;
    }

    SET_ERROR
    return;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// REDUCE - FILTER - MAP

void *List_reduce(T list, Reducer reducer, void *initialValue, int *error)
{
    void *accumulator = initialValue;
    int errToSet = 0;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet && (initialValue == NULL || reducer == NULL))
    {
        errToSet = E_LIST_INVALID_ARGUMENTS;
    }

    if (!errToSet)
    {
        struct Node *runner = list->head;
        while (runner != NULL && !errToSet)
        {
            accumulator = reducer(accumulator, runner->element, &errToSet);
            runner = !errToSet ? runner->next : NULL;
        }
    }

    SET_ERROR
    return accumulator;
}

T List_map(T list, Projection projection, Comparator c, Duplicator du, Deallocator de, int *error)
{
    int errToSet = 0;
    T newList = NULL;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet && projection == NULL)
    {
        errToSet = E_LIST_INVALID_ARGUMENTS;
    }

    if (!errToSet)
    {
        newList = List_create(c, du, de, &errToSet);
    }

    if (!errToSet)
    {
        struct Node *runner = list->head;
        while (runner != NULL && !errToSet)
        {
            void *newValue = projection(runner->element, &errToSet);

            if (!errToSet)
            {
                List_insertTail(newList, newValue, &errToSet);
            }

            runner = !errToSet ? runner->next : NULL;
        }
    }

    SET_ERROR
    return newList;
}

T List_filter(T list, Predicate predicate, int useDuplicator, int *error)
{

    int errToSet = 0;
    T newList = NULL;

    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet && predicate == NULL)
    {
        errToSet = E_LIST_INVALID_ARGUMENTS;
    }

    if (!errToSet)
    {
        newList = List_create(list->comparator, list->duplicator, list->deallocator, &errToSet);
    }

    if (!errToSet)
    {
        struct Node *runner = list->head;
        while (runner != NULL && !errToSet)
        {
            int res = predicate(runner->element);
            void *toInsert = NULL;

            if (res == 1 && useDuplicator)
            {
                toInsert = list->duplicator(runner->element, &errToSet);
            }
            else if (res == 1 && !useDuplicator)
            {
                toInsert = runner->element;
            }

            if (!errToSet && toInsert != NULL)
            {
                List_insertTail(newList, toInsert, &errToSet);
            }

            runner = !errToSet ? runner->next : NULL;
        }
    }

    SET_ERROR
    return newList;
}

void List_forEach(T list, ForEachCallback callback, int *error)
{
    int errToSet = 0;
    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet && callback == NULL)
    {
        errToSet = E_LIST_INVALID_ARGUMENTS;
    }

    if (!errToSet)
    {
        struct Node *runner = list->head;
        while (runner != NULL && !errToSet)
        {
            callback(runner->element, &errToSet);
            runner = !errToSet ? runner->next : NULL;
        }
    }

    SET_ERROR
}

void List_forEachWithContext(T list, ForEachCallbackWithContext callback, void* context, int *error)
{
    int errToSet = 0;
    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet && (callback == NULL || context == NULL))
    {
        errToSet = E_LIST_INVALID_ARGUMENTS;
    }

    if (!errToSet)
    {
        struct Node *runner = list->head;
        while (runner != NULL && !errToSet)
        {
            callback(context, runner->element, &errToSet);
            runner = !errToSet ? runner->next : NULL;
        }
    }

    SET_ERROR
}

void List_forEach_right(T list, ForEachCallback callback, int *error)
{
    int errToSet = 0;
    if (list == NULL)
    {
        errToSet = E_LIST_NULL;
    }

    if (!errToSet && callback == NULL)
    {
        errToSet = E_LIST_INVALID_ARGUMENTS;
    }

    if (!errToSet)
    {
        struct Node *runner = list->tail;
        while (runner != NULL && !errToSet)
        {
            callback(runner->element, &errToSet);
            runner = !errToSet ? runner->prev : NULL;
        }
    }

    SET_ERROR
}

const char *list_error_messages[] = {
    "",
    "list internal malloc error",
    "list is null",
    "list cannot contain NULL elements",
    "list is empty",
    "one or more arguments are invalid"};

const char *List_getErrorMessage(int errorCode)
{
    return list_error_messages[errorCode];
}