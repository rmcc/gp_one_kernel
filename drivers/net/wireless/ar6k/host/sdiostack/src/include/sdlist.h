// Copyright (c) 2004-2006 Atheros Communications Inc.
// 
//
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
//
//

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sdlist.h

@abstract: OS independent list functions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDLIST_H___
#define __SDLIST_H___

/* list functions */
/* pointers for the list */
typedef struct _SDLIST {
    struct _SDLIST *pPrev;
    struct _SDLIST *pNext;
}SDLIST, *PSDLIST;
/*
 * SDLIST_INIT , circular list 
*/
#define SDLIST_INIT(pList)\
    {(pList)->pPrev = pList; (pList)->pNext = pList;}
#define SDLIST_INIT_DECLARE(List)\
    SDLIST List =   {&List, &List}


#define SDLIST_IS_EMPTY(pList) (((pList)->pPrev == (pList)) && ((pList)->pNext == (pList)))
#define SDLIST_GET_ITEM_AT_HEAD(pList) (pList)->pNext   
#define SDLIST_GET_ITEM_AT_TAIL(pList) (pList)->pPrev 
/*
 * SDITERATE_OVER_LIST pStart is the list, pTemp is a temp list member
 * NOT: do not use this function if the items in the list are deleted inside the
 * iteration loop
*/
#define SDITERATE_OVER_LIST(pStart, pTemp) \
    for((pTemp) =(pStart)->pNext; pTemp != (pStart); (pTemp) = (pTemp)->pNext)
    

/* safe iterate macro that allows the item to be removed from the list
 * the iteration continues to the next item in the list
 */
#define SDITERATE_OVER_LIST_ALLOW_REMOVE(pStart,pItem,st,offset)  \
{                                                       \
    PSDLIST  pTemp;                                     \
    pTemp = (pStart)->pNext;                            \
    while (pTemp != (pStart)) {                         \
        (pItem) = CONTAINING_STRUCT(pTemp,st,offset);   \
         pTemp = pTemp->pNext;                          \
         
#define SDITERATE_END }}
 
/*
 * SDListInsertTail - insert pAdd to the end of the list
*/
static INLINE PSDLIST SDListInsertTail(PSDLIST pList, PSDLIST pAdd) {
        /* this assert catches when an item is added twice */
    DBG_ASSERT(pAdd->pNext != pList);
        /* insert at tail */ 
    pAdd->pPrev = pList->pPrev;
    pAdd->pNext = pList;
    pList->pPrev->pNext = pAdd;
    pList->pPrev = pAdd;
    return pAdd;
}
    
/*
 * SDListInsertHead - insert pAdd into the head of the list
*/
static INLINE PSDLIST SDListInsertHead(PSDLIST pList, PSDLIST pAdd) {
        /* this assert catches when an item is added twice */
    DBG_ASSERT(pAdd->pPrev != pList);    
        /* insert at head */ 
    pAdd->pPrev = pList;
    pAdd->pNext = pList->pNext;
    pList->pNext->pPrev = pAdd;
    pList->pNext = pAdd;
    return pAdd;
}

#define SDListAdd(pList,pItem) SDListInsertHead((pList),(pItem))
/*
 * SDListRemove - remove pDel from list
*/
static INLINE PSDLIST SDListRemove(PSDLIST pDel) {
    pDel->pNext->pPrev = pDel->pPrev;
    pDel->pPrev->pNext = pDel->pNext;
        /* point back to itself just to be safe, incase remove is called again */
    pDel->pNext = pDel;
    pDel->pPrev = pDel;
    return pDel;
}

/*
 * SDListRemoveItemFromHead - get a list item from the head
*/
static INLINE PSDLIST SDListRemoveItemFromHead(PSDLIST pList) {
    PSDLIST pItem = NULL;
    if (pList->pNext != pList) {
        pItem = pList->pNext;
            /* remove the first item from head */
        SDListRemove(pItem);    
    }   
    return pItem; 
}
#endif /* __SDLIST_H___ */
