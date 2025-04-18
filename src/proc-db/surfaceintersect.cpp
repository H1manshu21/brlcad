/*             S U R F A C E I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @{ */
/** @file proc-db/surfaceintersect.cpp
 *
 * This code was originally written by Joe Doliner: jdoliner@gmail.com
 */

#include "surfaceintersect.h"

/* implementation system headers */
#include <assert.h>
#include "bu/app.h"

#define SI_MIN(a, b) (((a) > (b)) ? (a) : (b))


/**
 * Push
 *
 * @brief updates s and t using an X, Y, Z vector
 */
void
Push(const ON_Surface *surf, double *s, double *t, ON_3dVector vec)
{
    int ret;
    ON_3dVector ds, dt;
    ON_3dPoint value;
    ret = surf->Ev1Der(*s, *t, value, ds, dt);
    if (!ret) return;
    double delta_s, delta_t;
    ON_DecomposeVector(vec, ds, dt, &delta_s, &delta_t);
    *s += delta_s;
    *t += delta_t;
}


/**
 * Step
 *
 * @brief advances s1, s2, t1, t2 along the curve of intersection of
 * the two surfaces by a distance of step size.
 */
void
Step(
    const ON_Surface *surf1,
    const ON_Surface *surf2,
    double *s1,
    double *s2,
    double *t1,
    double *t2,
    double stepsize
    )
{
    ON_3dVector Norm1 = surf1->NormalAt(*s1, *t1);
    ON_3dVector Norm2 = surf2->NormalAt(*s2, *t2);
    ON_3dVector step = ON_CrossProduct(Norm1, Norm2);
    double Magnitude = ON_ArrayMagnitude(3, step);
    /* double vec[3] = {0.0, 0.0, 0.0}; */
    ON_3dVector stepscaled;
    ON_ArrayScale(3, stepsize/Magnitude, step, stepscaled);
    Push(surf1, s1, t1, stepscaled);
    Push(surf2, s2, t2, stepscaled);
}


/**
 * Jiggle
 *
 * @brief uses newtonesque method to jiggle the points on the surfaces
 * about and find a closer * point returns the new distance between
 * the points.
 */
double
Jiggle(
    const ON_Surface *surf1,
    const ON_Surface *surf2,
    double *s1,
    double *s2,
    double *t1,
    double *t2
    )
{
    ON_3dPoint p1 = surf1->PointAt(*s1, *t1);
    ON_3dPoint p2 = surf2->PointAt(*s2, *t2);
    ON_3dVector Norm1 = surf1->NormalAt(*s1, *t1);
    ON_3dVector Norm2 = surf2->NormalAt(*s2, *t2);
    ON_3dVector p1p2 = p2 - p1, p2p1 = p1 - p2;
    ON_3dVector p1p2orth, p1p2prl, p2p1orth, p2p1prl;
    VPROJECT(p1p2, Norm1, p1p2prl, p1p2orth);
    VPROJECT(p2p1, Norm2, p2p1prl, p2p1orth);
    Push(surf1, s1, t1, p1p2orth / 2);
    Push(surf2, s2, t2, p2p1orth / 2);
    return surf1->PointAt(*s1, *t1).DistanceTo(surf2->PointAt(*s2, *t2));
}


/**
 * SplitTrim
 *
 * @brief Split's a trim at a point, and replaces the references to
 * that trim with the pieces
 */
void
SplitTrim(ON_BrepTrim *trim, double t)
{
    ON_Curve *left, *right;
    bool rv = trim->Split(t, left, right);

    if (rv != 0) {
	int lefti = trim->Brep()->AddTrimCurve(left);
	int righti = trim->Brep()->AddTrimCurve(right);
	trim->Loop()->m_ti.Remove(trim->m_trim_index);
	trim->Loop()->m_ti.Insert(lefti, trim->m_trim_index);
	trim->Loop()->m_ti.Insert(righti, trim->m_trim_index);
    }
}


/**
 * ShatterLoop
 *
 * @brief after slicing a loop up in to pieces, this destroys the loop
 * itself and drops the pieces into an array
 */
void
ShatterLoop(ON_BrepLoop *loop, ON_ClassArray<ON_Curve*> curves)
{
    int i;
    for (i = 0; i < loop->TrimCount(); i++) {
	curves.Append(loop->Trim(i)->TrimCurveOf()->Duplicate());
    }

    /* deletes the loop as well as curves used only by this loop */
    loop->Brep()->DeleteLoop(*loop, true);
}


/**
 * Compare_X_Parameter
 *
 * @brief Compares two ON_X_EVENTS by the value of the parameter of the
 * first curve.
 */
int
Compare_X_Parameter(const ON_X_EVENT *a, const ON_X_EVENT *b)
{
    if (a->m_a[0] < b->m_a[0]) {
	return -1;
    } else if (a->m_a[0] > b->m_a[0]) {
	return 1;
    } else {
	return 0;
    }
}


/**
 * Curve_Compare_start
 *
 * @brief Compares the start points of the curve profiles
 */
int
Curve_Compare_start(ON_Curve *const *a, ON_Curve *const *b)
{
    ON_3dVector A = ON_2dVector((*a)->PointAtStart().x, (*a)->PointAtStart().y);
    ON_3dVector B = ON_2dVector((*b)->PointAtStart().x, (*b)->PointAtStart().y);

    if (V2NEAR_EQUAL(A, B, 1e-9)) {
	return 0;
    } else if (A.x < B.x) {
	return -1;
    } else if (A.x > B.x) {
	return 1;
    } else if (A.y < B.y) {
	return -1;
    } else if (A.y > B.y) {
	return 1;
    }

    /* we should have already done this... but just in case */
    return 0;
}


/**
 * Face_X_Event::Face_X_Event
 *
 * @brief create a new uninitialized Face_X_Event
 */
Face_X_Event::Face_X_Event() : face1(NULL), face2(NULL), curve1(NULL), curve2(NULL)
{}


/**
 * Face_X_Event::Face_X_Event
 *
 * @brief create a new Face_X_Event using a set of given values
 */
Face_X_Event::Face_X_Event(ON_BrepFace *f1, ON_BrepFace *f2, ON_Curve *c1, ON_Curve *c2)
{
    this->face1 = f1;
    this->face2 = f2;
    this->curve1 = c1;
    this->curve2 = c2;
    this->loop_flags1.SetCapacity(face1->LoopCount());
    this->loop_flags1.SetCount(face1->LoopCount());
    this->loop_flags2.SetCapacity(face2->LoopCount());
    this->loop_flags2.SetCount(face2->LoopCount());
}


/**
 * Face_X_Event::Render_Curves
 *
 * @brief Renders the Curves in the Face_X_Event as the different
 * curves it is segmented in to This assumes the convention that to
 * the left of a curve is below.
 */
int
Face_X_Event::Render_Curves()
{
    /* the curve can be active or inactive in either face */
    bool active1 = false, active2 = false;
    double last = 0.0;
    int i;

    /* Now we step through the X events activating and deactivating
     * the curve Note the curve is always curve1 in the event, while
     * the trim is always curve 2
     */
    for (i = 0; i < x.Count(); i++) {
	ON_X_EVENT event = x[i];
	if (active1 && active2) {
	    /* to be deactivated the curve must pass from below a curve to above it */
	    if (event.m_dirA[0] == event.from_below_dir && event.m_dirA[1] == event.to_above_dir) {
		ON_Curve *new_curve1 = curve1->Duplicate();
		ON_Curve *new_curve2 = curve2->Duplicate();
		new_curve1->Trim(ON_Interval(last, event.m_a[0]));
		new_curve2->Trim(ON_Interval(last, event.m_a[0]));
		new_curves1.Append(new_curve1);
		new_curves2.Append(new_curve2);
		last = event.m_a[0];
		if (event.m_user.i == 0) { /* event.m_user tells us which of the twins the event happened in */
		    active1 = false;
		} else if (event.m_user.i == 1) {
		    active2 = false;
		} else {
		    assert(0);
		}
	    } else if (event.m_dirA[0] == event.from_above_dir && event.m_dirA[1] == event.to_below_dir) {

		/* this would be an activating event, except that both
		 * curves are already active in this case we assume
		 * that the user forgot to remove the outer curve,
		 * thus we reset last
		 */
		last = event.m_a[0];
	    }
	} else {
	    /* one of the curves is inactive */
	    if (event.m_dirA[0] == event.from_above_dir && event.m_dirA[1] == event.to_below_dir) {
		last = event.m_a[0];
		if (event.m_user.i == 0) {
		    active1 = true;
		} else if (event.m_user.i == 1) {
		    active2 = true;
		} else {
		    assert(0);
		}
	    } else {
		/* do nothing */
	    }
	}
    }

    return 0; /* XXX - unused */
}


/**
 * CurveCurveIntersect
 *
 * @brief Intersect 2 curves appending ON_X_EVENTS to the array x for
 * the intersections returns the number of ON_X_EVENTS appended
 *
 * This is not a great implementation of this function; it's limited in
 * that it will only find point intersections, not overlaps. Overlaps
 * will come out as long strings of points, and will probably take a
 * long time to compute.
 */
int
CurveCurveIntersect(
    const ON_Curve *curve1,
    const ON_Curve *curve2,
    ON_SimpleArray<ON_X_EVENT>& x,
    double tol
    )
{
    int rv = 0;
    ON_BoundingBox bbox1, bbox2;
    curve1->GetTightBoundingBox(bbox1);
    curve2->GetTightBoundingBox(bbox2);
    if (bbox1.IsDisjoint(bbox2)) {
	return 0;
    } else {
	ON_Interval domain1 = curve1->Domain(), domain2 = curve2->Domain();
	if (bbox1.Diagonal().Length() + bbox2.Diagonal().Length() > tol) {
	    ON_Curve *left1, *right1, *left2, *right2;
	    curve1->Split(domain1.Mid(), left1, right1), curve2->Split(domain2.Mid(), left2, right2);
	    rv += CurveCurveIntersect(left1, left2, x, tol);
	    rv += CurveCurveIntersect(left1, right2, x, tol);
	    rv += CurveCurveIntersect(right1, left2, x, tol);
	    rv += CurveCurveIntersect(right2, right2, x, tol);
	} else {
	    /* the curves are contained within a bounding box that's smaller than the tolerance */
	    ON_X_EVENT *newx = new ON_X_EVENT();
	    newx->m_type = newx->ccx_point;
	    newx->m_A[0] = curve1->PointAt(domain1.Mid());
	    newx->m_B[0] = curve2->PointAt(domain2.Mid());
	    newx->m_a[0] = domain1.Mid();
	    newx->m_b[0] = domain2.Mid();

	    /* Documentation for these routines being what it is for these intersections
	     * it's not really clear what these should be
	     newx.m_cnodeA[0] = ;
	     newx.m_nodeA_t[0] = ;
	     newx.m_cnodeB[0] = ;
	     newx.m_nodeB_t[0] = ;
	     newx.m_x_eventsn = ; this one we could do, but it doesn't seem worth the trouble.
	    */
	    x.Append(*newx);
	    delete newx;
	    rv++;
	}
    }
    return rv;
}


/**
 * SetCurveSurveIntersectionDir
 *
 * @brief Sets the Dir fields on an intersection event, this function
 * is 'below' a curve refers to the portion to the right of the curve
 * wrt N
 */
bool
SetCurveCurveIntersectionDir(
    ON_3dVector N,
    int xcount,
    ON_X_EVENT* xevent,
    ON_Curve *curve1,
    ON_Curve *curve2
    )
{
    bool rv = true;
    int i;
    for (i = 0; i < xcount; i++) {
	ON_X_EVENT event = xevent[i];
	if (event.m_type != event.ccx_point) {
	    continue;
	}
	ON_3dVector tangent1 = curve1->TangentAt(event.m_a[0]);
	ON_3dVector tangent2 = curve2->TangentAt(event.m_b[0]);
	ON_3dVector cross = ON_CrossProduct(tangent1, tangent2);
	double dot = ON_DotProduct(N, cross);
	if (dot > 0) {
	    event.m_dirA[0] = event.from_above_dir;
	    event.m_dirA[1] = event.to_below_dir;
	    event.m_dirB[0] = event.from_below_dir;
	    event.m_dirB[1] = event.to_above_dir;
	} else if (dot > 0) {
	    event.m_dirA[0] = event.from_below_dir;
	    event.m_dirA[1] = event.to_above_dir;
	    event.m_dirB[0] = event.from_above_dir;
	    event.m_dirB[0] = event.to_below_dir;
	} else {
	    event.m_dirA[0] = event.no_x_dir;
	    event.m_dirA[1] = event.no_x_dir;
	    event.m_dirB[0] = event.no_x_dir;
	    event.m_dirB[1] = event.no_x_dir;
	    rv = false;
	}
    }
    return rv;
}


/**
 * Face_X_Event::Get_ON_X_Events()
 *
 * @brief Gets all of the intersections between either of the new
 * curves and the trims of the faces, stores them in the x field in
 * the class
 */
int
Face_X_Event::Get_ON_X_Events(double tol)
{
    ON_SimpleArray<ON_X_EVENT> out;
    x.Empty();
    ON_BrepFace *faces[2] = {face1, face2};
    ON_Curve *curves[2] = {curve1, curve2};
    ON_ClassArray<bool> *loop_flags[2]= {&loop_flags1, &loop_flags2};

    int i, j, k, l;
    for (i = 0; i < 2; i++) {

	ON_BrepFace *face = faces[i];
	for (j = 0; j < face->LoopCount(); j++) {

	    ON_BrepLoop* loop = face->Loop(j);
	    for (k = 0; k < loop->TrimCount(); k++) {

		ON_BrepTrim *trim = loop->Trim(k);
		out.Empty();

		/* It's worth noting that trims are always curve2 in intersections */
		int new_xs = CurveCurveIntersect(curves[i], trim->TrimCurveOf(), out, tol);
		if (new_xs) {
		    loop_flags[i][j] = true; /* flag loop j for destruction */
		}

		/* record in m_user which curve this intersection came from */
		for (l = 0; l < new_xs; l++) {
		    out[l].m_user.i = i;
		}

		SetCurveCurveIntersectionDir(ON_3dVector(0.0, 0.0, 1.0), new_xs, out.Array(), curves[0], curves[1]);
		x.Append(new_xs, out.Array());
	    }
	}
    }
    if (x.Count()) {
	x.QuickSort(Compare_X_Parameter);
    }
    return x.Count();
}


/**
 * MakeLoops
 *
 * @brief Makes loops out of the new trims and old trims by matching
 * them end to end old trims were previously in trim loops, new trims
 * come from the intersection the difference being that every new_trim
 * must be used, while some old_trims will be discarded.
 *
 * Note: destroys the arrays it's given.
 */
int
MakeLoops(
    ON_BrepFace *face,
    ON_ClassArray<ON_Curve*>& new_trims,
    ON_ClassArray<ON_Curve*>& old_trims,
    double tol
    )
{
    int i;
    ON_ClassArray<ON_Curve*> trims[2] = {new_trims, old_trims};
    for (i = 0; i < 2; i++) {
	trims[i].QuickSort(Curve_Compare_start);
    }

    ON_SimpleArray<ON_Curve*> loop;
    while (new_trims.Count() != 0) {

	loop.Append(*new_trims.First());
	new_trims.Remove(0);
	while (!V2NEAR_EQUAL((*loop.First())->PointAtStart(), (*loop.Last())->PointAtEnd(), tol)) {

	    /* bit hacky, makes a curve we can use to search for the
	     * arrays for matching trims
	     */
	    ON_LineCurve search_line = ON_LineCurve((*loop.Last())->PointAtEnd(), (*loop.First())->PointAtStart());
	    ON_Curve *search_curve = (ON_Curve *) &search_line;
	    int next_curve = -1; /* -1 is the not found value */
	    for (i = 0; i < 2; i++) {
		next_curve = new_trims.BinarySearch(&search_curve, Curve_Compare_start);
		if (next_curve != -1) {
		    loop.Append(trims[i][next_curve]);
		    trims[i].Remove(next_curve);
		    break;
		}
	    }

	    if (next_curve == -1) {
		/* we didn't find anything in either array */
		assert(0);
	    }
	}

	/* When we get here, loop will be a fully closed loop of
	 * trims.
	 */
	ON_Brep *brep = face->Brep();
	ON_BrepLoop *new_loop = &brep->NewLoop(ON_BrepLoop::unknown, *face);
	for (i = 0; i < loop.Count(); i++) {
	    new_loop->m_ti.Append(brep->AddTrimCurve(loop[i]));
	}

	int ldir = brep->LoopDirection(*new_loop);
	switch (ldir) {
	    case 1:
		new_loop->m_type = ON_BrepLoop::outer;
		break;
	    case -1:
		new_loop->m_type = ON_BrepLoop::inner;
		break;
	    default:
		assert(0); /* we should absolutely never get here */
	}
    }

    return 0;/* XXX - unused */
}


/**
 * IsClosed
 *
 * @brief check if a 2dPointarrray is closed. To be closed an array
 * must have >2 points in it, have the first and last points within
 * tol of one another and have at least one point not within tol of
 * either of them.
 */
bool
IsClosed(const ON_2dPointArray l, double tol)
{
    if (l.Count() < 3) {
	return false;
    } else if (V2NEAR_EQUAL(l[0], l[l.Count() - 1], tol)) {
	int i;
	for (i = 1; i < l.Count() - 1; i++) {
	    if (!V2NEAR_EQUAL(l[0], l[i], tol) && !V2NEAR_EQUAL(l[l.Count() - 1], l[i], tol)) {
		return true;
	    }
	}
    } else {
	return false;
    }
    return false;
}


/**
 * WalkIntersection
 *
 * @brief walks the intersection between 2 brepfaces, returns lines
 * segmented by the trimming curves
 */
void
WalkIntersection(
    const ON_Surface *surf1,
    const ON_Surface *surf2,
    double s1,
    double s2,
    double t1,
    double t2,
    double stepsize,
    double tol,
    ON_Curve *&out1,
    ON_Curve *&out2
    )
{
    ON_2dPointArray intersectionPoints1, intersectionPoints2;
    double inits1 = s1, inits2 = s2, initt1 = t1, initt2 = t2;
    double distance;
    int passes;

    /* this function is meant to be called with an arbitrary point on
     * the curve and return the entire curve, regardless of which
     * point was passed in, this means that if the intersection
     * doesn't happen to be a loop, we need to walk both directions
     * from the initial point.
     */
    for (passes = 0; passes < 2; passes++) {
	while (surf1->Domain(0).Includes(s1, true) && surf1->Domain(1).Includes(t1, true) && surf2->Domain(0).Includes(s2, true) && surf2->Domain(1).Includes(t2, true) && !(IsClosed(intersectionPoints1, stepsize) && IsClosed(intersectionPoints2, stepsize))) {
	    do {
		distance = Jiggle(surf1, surf2, &s1, &s2, &t1, &t2);
	    } while (distance > tol);

	    intersectionPoints1.Append(ON_2dPoint(s1, t1));
	    intersectionPoints2.Append(ON_2dPoint(s2, t2));
	    if (passes == 0) {
		Step(surf1, surf2, &s1, &s2, &t1, &t2, stepsize);
	    } else {
		Step(surf1, surf2, &s1, &s2, &t1, &t2, -stepsize);
	    }
	}

	if (IsClosed(intersectionPoints1, stepsize) && IsClosed(intersectionPoints2, stepsize)) {
	    break; /* we're done, no second pass required */
	} else {
	    /* this is a bit cute, we are assured to hit this part on
	     * the second pass, so we either reverse 0 or 2 times.
	     */
	    intersectionPoints1.Reverse();
	    intersectionPoints2.Reverse();
	    s1 = inits1;
	    s2 = inits2;
	    t1 = initt1;
	    t2 = initt2;

	    if (passes == 0) {
		/* to avoid duplicates of the starting point */
		Step(surf1, surf2, &s1, &s2, &t1, &t2, -stepsize);
	    }
	}
    }
    ON_BezierCurve *bezier1 = new ON_BezierCurve((ON_2dPointArray) intersectionPoints1);
    ON_BezierCurve *bezier2 = new ON_BezierCurve((ON_2dPointArray) intersectionPoints2);
    out1 = ON_NurbsCurve::New(*bezier1);
    out2 = ON_NurbsCurve::New(*bezier2);
}


/**
 * GetStartPointsInternal
 *
 * @brief Subdivides the surface recursively to zoom in on
 * intersection points internal to the surfaces.
 */
bool
GetStartPointsInternal(
    const ON_Surface *surf1,
    const ON_Surface *surf2,
    ON_2dPointArray& start_points1,
    ON_2dPointArray& start_points2,
    double tol
    )
{
    bool return_value;
    if (surf1->BoundingBox().IsDisjoint(surf2->BoundingBox())) {
	return_value = false;
    } else if (surf1->IsPlanar(NULL, tol) && surf2->IsPlanar(NULL, tol)) {
	if (!surf1->BoundingBox().IsDisjoint(surf2->BoundingBox())) {
	    double distance, s1 = surf1->Domain(0).Mid(), s2 = surf2->Domain(0).Mid(), t1 = surf1->Domain(1).Mid(), t2 = surf2->Domain(1).Mid();
	    do {
		distance = Jiggle(surf1, surf2, &s1, &s2, &t1, &t2);
	    } while (distance > tol);

	    start_points1.Append(ON_2dPoint(s1, t1));
	    start_points2.Append(ON_2dPoint(s2, t2));
	    return_value = true;
	} else {
	    return_value = false;
	}
    } else {
	ON_Surface *N1, *S1, *N2, *S2, *Parts1[4], *Parts2[4]; /* = {SW, SE, NW, NE} */

	surf1->Split(0, surf1->Domain(0).Mid(), S1, N1);
	surf2->Split(0, surf2->Domain(0).Mid(), S2, N2);
	S1->Split(1, S1->Domain(1).Mid(), Parts1[0], Parts1[1]);
	N1->Split(1, N1->Domain(1).Mid(), Parts1[2], Parts1[3]);
	S2->Split(1, S2->Domain(1).Mid(), Parts2[0], Parts2[1]);
	N2->Split(1, N2->Domain(1).Mid(), Parts2[2], Parts2[3]);

	int i, j;
	return_value = false;
	for (i = 0; i < 4; i++) {
	    for (j = 0; j < 4; j++) {
		return_value &= GetStartPointsInternal(Parts1[i], Parts2[j], start_points1, start_points2, tol);
	    }
	}
    }
    return return_value;
}


/**
 * GetStartPointsEdges
 *
 * @brief Find starting points that are on the edges of the surfaces
 */
bool
GetStartPointsEdges(
    const ON_Surface *surf1,
    const ON_Surface *surf2,
    ON_2dPointArray& start_points1,
    ON_2dPointArray& start_points2,
    double UNUSED(tol)
    )
{
    bool rv = false;
    ON_Interval intervals[4]; /* {s1, t1, s2, t2} */
    intervals[0] = surf1->Domain(0);
    intervals[1] = surf1->Domain(1);
    intervals[2] = surf2->Domain(0);
    intervals[3] = surf2->Domain(1);

    ON_2dPointArray out[2] = {start_points1, start_points2};
    ON_SimpleArray<ON_X_EVENT> x;

#if 0
    // TODO - as far as I can tell, these methods were never implemented in
    // openNURBS - they were part of the RhinoSDK and just stubs.  Newer
    // openNURBS has removed them completely, so this code no longer compiles.
    // Commenting out, but leaving to document the intent of the logic in case
    // someone wants to adapt this to other BRL-CAD logic in the future.
    surf1->IsoCurve(1, intervals[0].Min())->IntersectSurface(surf2, x, tol, tol);
    surf1->IsoCurve(1, intervals[0].Max())->IntersectSurface(surf2, x, tol, tol);
    surf1->IsoCurve(0, intervals[1].Min())->IntersectSurface(surf2, x, tol, tol);
    surf1->IsoCurve(0, intervals[1].Max())->IntersectSurface(surf2, x, tol, tol);
    surf2->IsoCurve(1, intervals[2].Min())->IntersectSurface(surf1, x, tol, tol);
    surf2->IsoCurve(1, intervals[2].Max())->IntersectSurface(surf1, x, tol, tol);
    surf2->IsoCurve(0, intervals[3].Min())->IntersectSurface(surf1, x, tol, tol);
    surf2->IsoCurve(0, intervals[3].Max())->IntersectSurface(surf1, x, tol, tol);
#endif

    int curve; /* the surface the curves come from */
    int dir; /* the parameter that varies in the iso curve */
    int min_or_max; /* 0 = min, 1 = max */

    for (curve = 0; curve < 2; curve++) {
	for (dir = 0; dir < 2; dir++) {
	    for (min_or_max = 0; min_or_max < 2; min_or_max++) {
#if 0
		// TODO - as far as I can tell, these methods were never implemented in
		// openNURBS - they were part of the RhinoSDK and just stubs.  Newer
		// openNURBS has removed them completely, so this code no longer compiles.
		// Commenting out, but leaving to document the intent of the logic in case
		// someone wants to adapt this to other BRL-CAD logic in the future.
		const ON_Surface *surfaces[2] = {surf1, surf2};
		if (min_or_max == 0) {
		    surfaces[curve]->IsoCurve(1 - dir, intervals[dir + (2 * curve)].Min())->IntersectSurface(surfaces[curve], x, tol, tol);
		} else {
		    surfaces[curve]->IsoCurve(1 - dir, intervals[dir + (2 * curve)].Max())->IntersectSurface(surfaces[curve], x, tol, tol);
		}
#endif
		int i;
		for (i = 0; i < x.Count(); i++) {
		    rv = true; /* if we get here it means we've found a point */
		    if (dir == 0) {
			if (min_or_max == 0) {
			    out[curve].Append(ON_2dPoint(x[i].m_a[0], intervals[dir + (2 * curve)].Min()));
			} else {
			    out[curve].Append(ON_2dPoint(x[i].m_a[0], intervals[dir + (2 * curve)].Min()));
			}
		    } else {
			if (min_or_max == 0) {
			    out[curve].Append(ON_2dPoint(x[i].m_a[0], intervals[(1 - dir) + (2 * curve)].Min()));
			} else {
			    out[curve].Append(ON_2dPoint(x[i].m_a[0], intervals[(1 - dir) + (2 * curve)].Min()));
			}
		    }
		    out[1 - curve].Append(ON_2dPoint(x[i].m_b[0], x[i].m_b[1]));
		}
	    }
	}
    }
    return rv;
}


/**
 * FaceFaceIntersect
 *
 * @brief finds the intersection curves of two faces and returns them
 * as Face_X_Events.
 */
int
FaceFaceIntersect(
    ON_BrepFace *face1,
    ON_BrepFace *face2,
    double stepsize,
    double tol,
    ON_ClassArray<Face_X_Event>& x
    )
{
    int init_count = x.Count();
    const ON_Surface *surf1 = face1->SurfaceOf(), *surf2 = face2->SurfaceOf();
    ON_2dPointArray start_points1, start_points2;

    bool rv_edges = GetStartPointsEdges(surf1, surf2, start_points1, start_points2, tol);
    bool rv_internal = GetStartPointsInternal(surf1, surf2, start_points1, start_points2, tol);

    if (!(rv_edges || rv_internal)) {
	return false;
    }

    int i, j;
    ON_Curve *out1 = NULL;
    ON_Curve *out2 = NULL;
    ON_2dPoint start1, start2;

    for (i = 0; i < start_points1.Count(); i++) {
	start1 = *start_points1.First();
	start2 = *start_points2.First();
	WalkIntersection(surf1, surf2, start1[0], start2[0], start1[1], start2[1], stepsize, tol, out1, out2);

	start_points1.Remove(0);
	start_points2.Remove(0);

	for (j = 0; j < start_points1.Count(); j++) {
#if 0
	    // TODO - as far as I can tell, these methods were never implemented in
	    // openNURBS - they were part of the RhinoSDK and just stubs.  Newer
	    // openNURBS has removed them completely, so this code no longer compiles.
	    // Commenting out, but leaving to document the intent of the logic in case
	    // someone wants to adapt this to other BRL-CAD logic in the future.
	    double dummy;
	    if (out1->GetClosestPoint(ON_3dPoint(start_points1[j]), &dummy, stepsize) && out2->GetClosestPoint(ON_3dPoint(start_points2[j]), &dummy, stepsize)) {
		/* start points j lie on the curve so we delete them */
		start_points1.Remove(j);
		start_points2.Remove(j);
		j--;
	    }
#endif
	}
	Face_X_Event newevent = Face_X_Event(face1, face2, out1, out2);
	x.Append(newevent);
    }
    return x.Count() - init_count;
}


/**
 * BrepBrepIntersect
 *
 * @brief calls SurfaceSurfaceIntersect on the {m_S}X{m_S} then
 * intersects the results with the trim curves of the brepfaces.
 */
bool
BrepBrepIntersect(
    ON_Brep *brep1,
    ON_Brep *brep2,
    double stepsize,
    double tol
    )
{
    int i, j, k, l;

    /* the new curves we get from the actual intersection */
    ON_ClassArray<ON_ClassArray<ON_Curve*> > intersection_curves1, intersection_curves2;

    /* the new curves we get from destroying the old trim_loops */
    ON_ClassArray<ON_ClassArray<ON_Curve*> > trim_curves1, trim_curves2;

    /* initialization for brep1's arrays */
    intersection_curves1.SetCapacity(brep1->m_F.Count());
    intersection_curves1.SetCount(brep1->m_F.Count());
    trim_curves1.SetCapacity(brep1->m_F.Count());
    trim_curves1.SetCount(brep1->m_F.Count());

    /* initialization for brep2's arrays */
    intersection_curves2.SetCapacity(brep2->m_F.Count());
    intersection_curves2.SetCount(brep2->m_F.Count());
    trim_curves2.SetCapacity(brep2->m_F.Count());
    trim_curves2.SetCount(brep2->m_F.Count());

    /* flags for which trims need to be shattered */
    ON_ClassArray<ON_SimpleArray<bool> > loop_flags1, loop_flags2;
    loop_flags1.SetCapacity(brep1->m_F.Count());
    loop_flags1.SetCount(brep1->m_F.Count());
    loop_flags2.SetCapacity(brep2->m_F.Count());
    loop_flags2.SetCount(brep2->m_F.Count());

    /* initialize the loop_flag arrays */
    for (i = 0; i < brep1->m_F.Count(); i++) {
	ON_BrepFace *face1 = &brep1->m_F[i];
	loop_flags1[i].SetCapacity(face1->LoopCount());
	loop_flags1[i].SetCount(face1->LoopCount());
	loop_flags1[i].MemSet(false);
    }

    for (j = 0; j < brep2->m_F.Count(); j++) {
	ON_BrepFace *face2 = &brep2->m_F[j];
	loop_flags2[j].SetCapacity(face2->LoopCount());
	loop_flags2[j].SetCount(face2->LoopCount());
	loop_flags2[j].MemSet(false);
    }

    /* first we intersect all of the Faces and record the
     * intersections in Face_X_Events.
     */
    ON_ClassArray<Face_X_Event> x;
    for (i = 0; i < brep1->m_F.Count(); i++) {

	ON_BrepFace *face1 = &brep1->m_F[i];
	for (j = 0; j < brep2->m_F.Count(); j++) {

	    ON_BrepFace *face2 = &brep2->m_F[j];
	    x.Empty();
	    FaceFaceIntersect(face1, face2, stepsize, tol, x);

	    for (k = 0; k < x.Count(); k++) {

		Face_X_Event event_ij = x[k];
		event_ij.Get_ON_X_Events(tol);
		int new_curves = event_ij.Render_Curves();

		for (l = 0; l < new_curves; l++) {
		    intersection_curves1[i].Append(event_ij.new_curves1[l]);
		    intersection_curves2[j].Append(event_ij.new_curves2[l]);
		}
		for (l = 0; l < event_ij.loop_flags1.Count(); l++) {
		    loop_flags1[i][l] = loop_flags1[i][l] && event_ij.loop_flags1[l];
		}
		for (l = 0; l < event_ij.loop_flags2.Count(); l++) {
		    loop_flags2[j][l] = loop_flags2[j][l] && event_ij.loop_flags2[l];
		}
	    }
	}
    }

    /* Now we shatter the loops */
    for (i = 0; i < brep1->m_F.Count(); i++) {
	ON_BrepFace *face1 = &brep1->m_F[i];
	for (j = 0; j < loop_flags1[i].Count(); j++) {
	    if (loop_flags1[i][j]) {
		/* time to destroy a loop */
		ShatterLoop(face1->Loop(j), trim_curves1[i]);
	    }
	}
    }

    for (i = 0; i < brep2->m_F.Count(); i++) {
	ON_BrepFace *face2 = &brep2->m_F[i];
	for (j = 0; j < loop_flags2[i].Count(); j++) {
	    if (loop_flags2[i][j]) {
		/* time to destroy a loop */
		ShatterLoop(face2->Loop(j), trim_curves2[i]);
	    }
	}
    }

    /* We now have all of the trims we need to reconstruct the loops */
    for (i = 0; i < brep1->m_F.Count(); i++) {
	MakeLoops(&brep1->m_F[i], intersection_curves1[i], trim_curves1[i], tol);
    }
    for (i = 0; i < brep2->m_F.Count(); i++) {
	MakeLoops(&brep2->m_F[i], intersection_curves2[i], trim_curves2[i], tol);
    }

    /* FIXME: we allocated a lot of entities and stashed them in our
     * 'x' Face_X_Face array, but never delete them (and there's no
     * destructor).  The memory needs to be released before returning.
     */

    /* XXX - unused */
    return false;
}


namespace si {

    enum {
	A, B, C, D, E, F, G, H
    };

    enum {
	AB, BC, CD, AD, EF, FG, GH, EH
    };

    enum {
	ABCD, FEHG
    };
} /* end namespace */
using namespace si;


void
MakeTwistedCubeEdges1(ON_Brep& brep)
{
    MakeTwistedCubeEdge(brep, A, B, AB);
    MakeTwistedCubeEdge(brep, B, C, BC);
    MakeTwistedCubeEdge(brep, C, D, CD);
    MakeTwistedCubeEdge(brep, A, D, AD);
}


void
MakeTwistedCubeEdges2(ON_Brep& brep)
{
    MakeTwistedCubeEdge(brep, E, F, EF);
    MakeTwistedCubeEdge(brep, F, G, FG);
    MakeTwistedCubeEdge(brep, G, si::H, GH);
    MakeTwistedCubeEdge(brep, E, si::H, EH);
}


void
MakeTwistedCubeFaces1(ON_Brep& brep)
{
    MakeTwistedCubeFace(brep,
			ABCD, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			A, B, C, D, // indices of vertices listed in order
			AB, +1, // south edge, orientation w.r.t. trimming curve?
			BC, +1, // east edge, orientation w.r.t. trimming curve?
			CD, +1,
			AD, -1);
}


void
MakeTwistedCubeFaces2(ON_Brep& brep)
{
    MakeTwistedCubeFace(brep,
			FEHG, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			F, E, si::H, G, // indices of vertices listed in order
			EF, -1, // south edge, orientation w.r.t. trimming curve?
			EH, +1, // east edge, orientation w.r.t. trimming curve?
			GH, -1,
			FG, -1);
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    ON_3dPointArray pts1, pts2;
    pts1.Append(ON_3dPoint(1.0, 1.0, 0.0));
    pts1.Append(ON_3dPoint(-1.0, -1.0, 0.0));
    pts2.Append(ON_3dPoint(-1.0, 1.0, 0.0));
    pts2.Append(ON_3dPoint(1.0, -1.0, 0.0));
    ON_BezierCurve *bezier1 = new ON_BezierCurve(pts1);
    ON_BezierCurve *bezier2 = new ON_BezierCurve(pts2);
    ON_Curve *curve1 = ON_NurbsCurve::New(*bezier1);
    ON_Curve *curve2 = ON_NurbsCurve::New(*bezier2);
    ON_SimpleArray<ON_X_EVENT> x;
    CurveCurveIntersect(curve1, curve2, x, 1e-9);
    ON_Brep brep1 = ON_Brep(), brep2 = ON_Brep();
    ON_Surface *surf1 = TwistedCubeSideSurface(ON_3dPoint(1, 1, 1), ON_3dPoint(-1, -1, 1), ON_3dPoint(-1, -1, -1), ON_3dPoint(1, 1, -1));
    ON_Surface *surf2 = TwistedCubeSideSurface(ON_3dPoint(1, -1, 1), ON_3dPoint(-1, 1, 1), ON_3dPoint(-1, 1, -1), ON_3dPoint(1, -1, -1));

    brep1.Create(surf1);
    brep2.Create(surf2);
    BrepBrepIntersect(&brep1, &brep2, 1e-3, 1e-9);

    delete curve1;
    delete curve2;
    delete bezier1;
    delete bezier2;

    return 0;
}


/** @} */
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

