#include <assert.h>
#include <math.h>
#include <ctype.h>
#include "common.h"
#include "point.h"


void
point_translate(struct point *p, double x, double y)
{
    p->x += x;
    p->y += y;
}

double
point_distance(const struct point *p1, const struct point *p2)
{
    double dx = (p2->x - p1->x);
    double dy = (p2->y - p1->y);
    double dist = sqrt(dx*dx + dy*dy);;
    return dist;
}

int
point_compare(const struct point *p1, const struct point *p2)
{
    struct point origin;
    point_set(&origin, 0, 0);

    if(point_distance(p1, &origin) < point_distance(p2 ,&origin))
        return -1;

    else if(point_distance(p1, &origin) == point_distance(p2 ,&origin))
        return 0;

    else
        return 1;
}
