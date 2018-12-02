#include "net/rime/priority-list.h"
#include "lib/memb.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINTFUNC(...) uiwpplist_print()
#else 
#define PRINTF(...)
#define PRINTFUNC(...)
#endif 

#ifndef PLIST_NUM
#define PLIST_NUM 16
#endif

MEMB(plist_memb, struct uiwppriority_list, PLIST_NUM);
LIST(plist);

#if DEBUG
static void
uiwpplist_print()
{
    struct uiwppriority_list *list_handle = list_head(plist);
    int count = 0;

    while (list_handle != NULL) {
        count += 1;
        printf("p-list: no. %d element in plist, priority: %d, rssi: %hd, no. of retx: %d, address: %d.%d\n", 
            // count, list_handle->uiwpacklist->priority, list_handle->rssi, list_handle->re_tx,
            count, list_handle->uiwpacklist.priority, list_handle->rssi, list_handle->re_tx,
            list_handle->addr.u8[0], list_handle->addr.u8[1]);

        list_handle = list_item_next(list_handle);
    }
}
#endif

static int
update_uiwpplist(struct uiwppriority_list *uiwpplist)
{
    struct uiwppriority_list *list_handle = list_head(plist);

    if (list_handle == NULL) {
        // no priority list, create one
        list_init(plist);
        list_add(plist, uiwpplist);

        PRINTF("p-list: first element in plist created\n");
        PRINTFUNC();
        return 1;

    } else {
        if (list_length(plist) >= PLIST_NUM) {
            // maximum plist reaches, return error
            PRINTF("p-list: maximum plist reaches\n");
            return 0;
        }
        if (uiwpplist->re_tx == 1) {
            // if (list_handle->uiwpacklist->priority > uiwpplist->uiwpacklist->priority) {
            if (list_handle->uiwpacklist.priority > uiwpplist->uiwpacklist.priority) {
                    
                list_push(plist, uiwpplist);
                PRINTF("p-list: retx push to the beginning\n");
                PRINTFUNC();
                return 1;

            } else {

                struct uiwppriority_list *list_previous = list_handle;

                while (list_handle != NULL) {

                    // if (list_handle->uiwpacklist->priority > uiwpplist->uiwpacklist->priority) {
                    if (list_handle->uiwpacklist.priority > uiwpplist->uiwpacklist.priority) {


                        list_insert(plist, list_previous, uiwpplist);
                        PRINTF("p-list: retx element inserted to a specific place\n");
                        PRINTFUNC();
                        return 1;
                    }

                    list_previous = list_handle;
                    list_handle = list_item_next(list_handle);
                }

                list_add(plist, uiwpplist);
                PRINTF("p-list: retx element add to the tail\n");
                PRINTFUNC();
                return 1;
            }
        } else {
            // have list, but current struct has highest priority, or priority corresponded highest rssi
            // if ((list_handle->uiwpacklist->priority > uiwpplist->uiwpacklist->priority) ||
            //     ((list_handle->uiwpacklist->priority == uiwpplist->uiwpacklist->priority) && 
            //     (list_handle->rssi <= uiwpplist->rssi))) {
            if ((list_handle->uiwpacklist.priority > uiwpplist->uiwpacklist.priority) ||
                ((list_handle->uiwpacklist.priority == uiwpplist->uiwpacklist.priority) && 
                (list_handle->rssi <= uiwpplist->rssi))) {

                list_push(plist, uiwpplist);
                PRINTF("p-list: push to the beginning\n");
                PRINTFUNC();
                return 1;
            } else {
                // neither highest priority priority, nor lowest rssi
                struct uiwppriority_list *list_previous = list_handle;

                while (list_handle != NULL) {
                    
                    // if (list_handle->uiwpacklist->priority == uiwpplist->uiwpacklist->priority) {
                    if (list_handle->uiwpacklist.priority == uiwpplist->uiwpacklist.priority) {
                        if (list_handle->rssi <= uiwpplist->rssi) {
                            list_insert(plist, list_previous, uiwpplist);
                            PRINTF("p-list: element inserted to a specific place\n");
                            PRINTFUNC();
                            return 1;
                        }
                    // } else if (list_handle->uiwpacklist->priority > uiwpplist->uiwpacklist->priority) {
                    } else if (list_handle->uiwpacklist.priority > uiwpplist->uiwpacklist.priority) {                        
                        list_insert(plist, list_previous, uiwpplist);
                        PRINTF("p-list: element inserted to a specific place\n");
                        PRINTFUNC();
                        return 1;
                        
                    } else {
                        // do nothing
                    }

                    list_previous = list_handle;
                    list_handle = list_item_next(list_handle);
                }

                // current struct has lowest priority with this priority's lowest rssi
                list_add(plist, uiwpplist);
                PRINTF("p-list: element add to the tail\n");
                PRINTFUNC();
                return 1;
            } 
        }
        
    }

    return 0;
}

int
uiwpplist_create(int rssi, const linkaddr_t *addr, struct uiwpack_list *uiwpacklist)
{
    struct uiwppriority_list *list_handle = list_head(plist);

    while (list_handle != NULL) {
        if (linkaddr_cmp(&list_handle->addr, addr)) {
            PRINTF("p-list: this sensor node is already in the list\n");
            return 0;
        }

        list_handle = list_item_next(list_handle);
    }

    struct uiwppriority_list *uiwpplist;

    uiwpplist = memb_alloc(&plist_memb);

    if (uiwpplist != NULL) {
        uiwpplist->rssi = rssi;
        uiwpplist->re_tx = 0;
        linkaddr_copy(&uiwpplist->addr, addr);
        // uiwpplist->uiwpacklist = uiwpacklist;
        memcpy(&(uiwpplist->uiwpacklist), uiwpacklist, sizeof(*uiwpacklist));

        PRINTF("p-list: plist struct created, rssi: %hd, addr: %d.%d\n", uiwpplist->rssi, 
        uiwpplist->addr.u8[0], uiwpplist->addr.u8[1]);
    } else {
        PRINTF("p-list: list create error!\n");
        return 0;
    }
    
    update_uiwpplist(uiwpplist);

    return 1;
}

void *
uiwpplist_get()
{
    return list_pop(plist);
}

int 
uiwpplist_retx_update(struct uiwppriority_list *uiwpplist)
{
    if (++uiwpplist->re_tx == 2) {
        // if (++uiwpplist->uiwpacklist->priority > UIWP_PRIORITY_LEVEL_MAX) {
        if (++uiwpplist->uiwpacklist.priority > UIWP_PRIORITY_LEVEL_MAX) {
            return 0;
        }
    } else if (uiwpplist->re_tx == 3) {
        return 0;
    } else {
        // do nothing here
    }

    PRINTF("p-list: updated priority: %d, updated re_tx: %d\n", 
        // uiwpplist->uiwpacklist->priority, uiwpplist->re_tx);
        uiwpplist->uiwpacklist.priority, uiwpplist->re_tx);

    return update_uiwpplist(uiwpplist);
}

int 
uiwpplist_free()
{
    // free the list and its corresponding memory
    while (list_head(plist) != NULL) {
        struct uiwppriority_list *uiwpplist = list_chop(plist);
        // free(uiwpplist->uiwpacklist);
        memb_free(&plist_memb, uiwpplist);
    }
    memb_init(&plist_memb);

    PRINTF("p-list: plist is freed\n");

    return 1;
}