#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "point.h"
#include "sorted_points.h"

struct sorted_points {
    /* you can define this struct to have whatever fields you want. */
    struct point p;
    struct sorted_points *next;
    struct sorted_points *prev;
};

struct sorted_points *
sp_init()
{
    struct sorted_points *sp;

    sp = (struct sorted_points *)malloc(sizeof(struct sorted_points));
    assert(sp);

    sp->next = NULL;
    sp->prev = NULL;

    return sp;
}

void
sp_destroy(struct sorted_points *sp)
{
    if(sp->next == NULL)
    {
        free(sp);
        return;
    }
    sp_destroy(sp->next);
    free(sp);
}

int
sp_add_point(struct sorted_points *sp, double x, double y)
{
    struct sorted_points *newPoint = sp_init();
    point_set(&(newPoint->p), x, y);

    struct point origin;
    point_set(&origin, 0, 0);

    struct sorted_points *current = sp;

    // The loop traverses the sorted points list to add a new point
    while(current->next && (point_distance(&(current->next->p), &origin) < point_distance(&(newPoint->p), &origin)))
        current = current->next;


    if(!sp->next)
    {
        newPoint->next = newPoint->prev = NULL;
        sp->next = sp->prev = newPoint;
        return 1;
    }

    else if(!current->next)
    {
        current->next = newPoint;
        newPoint->prev = current;
        sp->prev = newPoint;
        return 1;

    }

    else
    {
        newPoint->next = current->next;
        current->next = newPoint;
        newPoint->next->prev = newPoint;

        if(current != sp)
            newPoint->prev = current;

        return 1;
    }

    return 0;
}

int
sp_remove_first(struct sorted_points *sp, struct point *ret)
{
    struct sorted_points *current = sp->next;

    if(!current)
        return 0;

    ret->x = point_X(&(current->p));
    ret->y = point_Y(&(current->p));

    if(!current->next)
        sp->next = sp->prev = NULL;

    else
    {
        sp->next = current->next;
        sp->next->prev = current->prev;
    }

    free(current);
    return 1;
}

int
sp_remove_last(struct sorted_points *sp, struct point *ret)
{
    struct sorted_points *current = sp->prev;

    if(!current)
        return 0;

    ret->x = point_X(&(current->p));
    ret->y = point_Y(&(current->p));

    if(!current->prev)
        sp->prev = sp->next = NULL;

    else
    {
        current->prev->next = current->next;
        sp->prev = current->prev;
    }

    free(current);
    return 1;
}

int
sp_remove_by_index(struct sorted_points *sp, int index, struct point *ret)
{
    struct sorted_points *current = sp->next;
    int i;

    if(!current)
        return 0;

    for(i = 0; i < index && current->next; i++)
        current = current->next;

    if(i != index) return 0;

    if(!current->prev && !current->next) 
        sp->prev = sp->next = NULL;

    else if(!current->prev)
    { 
        sp->next = current->next;
        sp->next->prev = current->prev;			
    }

    else if(!current->next)
    {
        current->prev->next = current->next;
        sp->prev = current->prev;			
    }

    else
    {
        current->prev->next = current->next;
        current->next->prev = current->prev;
    }

    ret->x = point_X(&(current->p));
    ret->y = point_Y(&(current->p));

    free(current);
    return 1;
}

int
sp_delete_duplicates(struct sorted_points *sp)
{
    struct sorted_points *current = sp->next;
    int count = 0;

    if(!current)
        return 0;

    while(current->next)
    {
        if(point_compare(&(current->p), &(current->next->p)) == 0)
        {
            if(!current->prev)
            {
                sp->next = current->next;
                sp->next->prev = current->prev;
            }	

            else
            {
                current->prev->next = current->next;
                current->next->prev = current->prev;
            }	

            free(current);
            count++;
        }
        current = current->next;
    }

    return count;
}

