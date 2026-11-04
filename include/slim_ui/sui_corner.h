/* sui_corner.h -- the continuous rounded corner: prepared path commands and a hit test. All coordinates are logical units, Y down.
 * No allocation: the caller owns the command storage. It needs the C runtime's math, so a SUI_NO_CRT build leaves it out.
 */
#ifndef SUI_CORNER_H
#define SUI_CORNER_H
#include <math.h>
#include <stddef.h>

typedef struct SuiCornerPoint { double x, y; } SuiCornerPoint;
typedef struct SuiCornerRect { double x, y, width, height; } SuiCornerRect;
typedef enum SuiCornerOp { SUI_CORNER_MOVE, SUI_CORNER_LINE, SUI_CORNER_CUBIC, SUI_CORNER_CLOSE } SuiCornerOp;
typedef struct SuiCornerCommand { SuiCornerOp op; SuiCornerPoint p[3]; } SuiCornerCommand;
typedef struct SuiCornerPath {
    SuiCornerCommand* commands;
    size_t count, capacity;
    int failed;
} SuiCornerPath;

static inline SuiCornerPoint sui_corner_point(double x, double y) { SuiCornerPoint p={x,y}; return p; }
static inline void sui_corner_path_init(SuiCornerPath* p, SuiCornerCommand* storage, size_t capacity) {
    p->commands=storage; p->count=0; p->capacity=storage ? capacity : 0; p->failed=0;
}
static inline int sui_corner_path_push(SuiCornerPath* p, SuiCornerOp op, SuiCornerPoint a, SuiCornerPoint b, SuiCornerPoint c) {
    SuiCornerCommand* command;
    if (!p) return 0;
    if (p->failed || p->count>=p->capacity) { p->failed=1; return 0; }
    command=&p->commands[p->count++]; command->op=op;
    command->p[0]=a; command->p[1]=b; command->p[2]=c; return 1;
}
static inline void sui_corner_path_line(SuiCornerPath* p, double x, double y) {
    SuiCornerPoint a=sui_corner_point(x,y), zero={0,0}; sui_corner_path_push(p,SUI_CORNER_LINE,a,zero,zero);
}
/* ---- continuous corners ------------------------------------------------------------------------------------------------------------
 * Every rounded rectangle the renderer fills, strokes and clips has Apple's continuous corner (Core Animation's
 * cornerCurve "continuous"), constructed as Core Graphics constructs it. The radius is first clamped to half the shorter side. A
 * corner of radius r is three cubics reaching SUI_CORNER_REACH x r along each edge, its curvature rising from the straight edge to a
 * circular middle. Along an edge too short for the full reach (shorter than 2 x SUI_CORNER_REACH x r), that half of the corner reaches
 * half the side instead, and its points move linearly toward the circular corner of radius r (the taper, per axis): at a radius of
 * half the side both halves are circular, so a square with a radius of half its side is a circle, and a capsule ends circular across
 * its short axis while its long edges keep the smooth transition (as Core Animation draws it; not a stadium). A corner's points are in
 * its own units of r: u from the edge the contour arrives along, v from the edge it leaves along, the contour running clockwise
 * (Y down) from the top-left corner's left edge. */
#define SUI_CORNER_REACH 1.528665               /* a full corner's reach along each edge, in units of its radius */
#define SUI_CORNER_RECT_PATH_CAPACITY 17         /* sui_corner_path_round_rect: a move, four corners of three cubics, three lines, a close */
#define SUI_CORNER_OFFSET_PATH_CAPACITY 53  /* sui_corner_path_round_rect_offset: each corner a line and twelve cubics */
/* The radius a w x h rounded rect draws: clamped to 0..half the shorter side (0 for a non-finite radius). */
static inline double sui_corner_radius(double w,double h,double radius) {
    double half=0.5*(w<h ? w : h);
    if(!(radius>0) || !(half>0)) return 0;
    return radius<half ? radius : half;
}
/* How far along an edge of length `side` a corner of (clamped) radius r reaches: SUI_CORNER_REACH x r, at most half the side. */
static inline double sui_corner_reach(double side,double r) {
    double reach=SUI_CORNER_REACH*r,half=0.5*side;
    return r>0 ? (reach<half ? reach : half) : 0;
}
/* One half of a corner (the half beside the edge it arrives along, in that edge's frame), its reach in units of r from 1 (circular)
 * to SUI_CORNER_REACH (full): the first cubic's handles, its end (the junction) and the middle cubic's first handle. The circular
 * corner's arcs end where the full corner's junctions point, projected onto the circle of radius 1 about (1, 1); each arc is the cubic
 * with handles 4/3 tan(angle / 4). */
typedef struct SuiCornerHalf { double reach, c1, c2x, c2y, jx, jy, mx, my; } SuiCornerHalf;
static inline SuiCornerHalf sui__corner_half(double reach) {
    static const SuiCornerHalf full={SUI_CORNER_REACH,1.08849,0,0.868407,0.0749114,0.631494,0.16906,0.372824};
    static const SuiCornerHalf round={1,0.8732602355268559,0.02409251987589936,0.7476756885741362,
                                       0.07099462715854121,0.6299337664259244,0.17263992142896187,0.37476586421208935};
    double t=(SUI_CORNER_REACH-reach)/(SUI_CORNER_REACH-1); SuiCornerHalf h;
    t=t>0 ? (t<1 ? t : 1) : 0;
    h.reach=full.reach+(round.reach-full.reach)*t; h.c1=full.c1+(round.c1-full.c1)*t;
    h.c2x=full.c2x+(round.c2x-full.c2x)*t; h.c2y=full.c2y+(round.c2y-full.c2y)*t;
    h.jx=full.jx+(round.jx-full.jx)*t; h.jy=full.jy+(round.jy-full.jy)*t;
    h.mx=full.mx+(round.mx-full.mx)*t; h.my=full.my+(round.my-full.my)*t;
    return h;
}
#define SUI__CORNER_CIRCLE_HANDLE 0.4477152501692064   /* 1 - 4/3 (sqrt 2 - 1): a quarter circle's cubic handle, from the corner */
/* A corner's points in units of r for its reaches (units of r, 1 to SUI_CORNER_REACH) along the edge it arrives along and the one it
 * leaves along: the start (0, reach_in) and three cubics (10 points), or, both halves circular, one quarter circle (4 points). The
 * middle cubic's second handle takes its small coordinate from the arriving half, as Core Graphics builds it. Returns the count. */
static inline unsigned sui_corner_points(double reach_in,double reach_out,SuiCornerPoint p[10]) {
    SuiCornerHalf i,o;
    if(!(reach_in>1) && !(reach_out>1)) {
        p[0]=sui_corner_point(0,1); p[1]=sui_corner_point(0,SUI__CORNER_CIRCLE_HANDLE); p[2]=sui_corner_point(SUI__CORNER_CIRCLE_HANDLE,0); p[3]=sui_corner_point(1,0);
        return 4;
    }
    i=sui__corner_half(reach_in); o=sui__corner_half(reach_out);
    p[0]=sui_corner_point(0,i.reach); p[1]=sui_corner_point(0,i.c1); p[2]=sui_corner_point(i.c2x,i.c2y); p[3]=sui_corner_point(i.jx,i.jy);
    p[4]=sui_corner_point(i.mx,i.my); p[5]=sui_corner_point(o.my,i.mx); p[6]=sui_corner_point(o.jy,o.jx);
    p[7]=sui_corner_point(o.c2y,o.c2x); p[8]=sui_corner_point(o.c1,0); p[9]=sui_corner_point(o.reach,0);
    return 10;
}
/* A corner placed in a rect: its point and the unit directions u and v point into the rect from the edge the contour arrives along
 * and the one it leaves along; the reaches (units of r) along those edges. */
typedef struct SuiCornerFrame { SuiCornerPoint at, u, v; double reach_in, reach_out; } SuiCornerFrame;
/* Corner k (0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left) of the w x h rect at (x, y), radius r (clamped). */
static inline SuiCornerFrame sui__corner_frame(SuiCornerRect b,double r,unsigned k) {
    SuiCornerFrame f; double rh=r>0 ? sui_corner_reach(b.width,r)/r : 1,rv=r>0 ? sui_corner_reach(b.height,r)/r : 1;
    switch(k&3u) {
    case 0: f.at=sui_corner_point(b.x,b.y); f.u=sui_corner_point(1,0); f.v=sui_corner_point(0,1); f.reach_in=rv; f.reach_out=rh; break;
    case 1: f.at=sui_corner_point(b.x+b.width,b.y); f.u=sui_corner_point(0,1); f.v=sui_corner_point(-1,0); f.reach_in=rh; f.reach_out=rv; break;
    case 2: f.at=sui_corner_point(b.x+b.width,b.y+b.height); f.u=sui_corner_point(-1,0); f.v=sui_corner_point(0,-1); f.reach_in=rv; f.reach_out=rh; break;
    default: f.at=sui_corner_point(b.x,b.y+b.height); f.u=sui_corner_point(0,-1); f.v=sui_corner_point(1,0); f.reach_in=rh; f.reach_out=rv; break;
    }
    return f;
}
/* Local (u, v) lengths of a corner frame to rect coordinates. */
static inline SuiCornerPoint sui__corner_map(const SuiCornerFrame* f,double u,double v) {
    return sui_corner_point(f->at.x+u*f->u.x+v*f->v.x,f->at.y+u*f->u.y+v*f->v.y);
}
/* Appends a corner's cubics (radius r): the contour is at the corner's start, (0, reach_in r). */
static inline void sui__corner_path_corner(SuiCornerPath* p,const SuiCornerFrame* f,double r) {
    SuiCornerPoint q[10]; unsigned n=sui_corner_points(f->reach_in,f->reach_out,q),k;
    for(k=1;k+2<n;k+=3)
        sui_corner_path_push(p,SUI_CORNER_CUBIC,sui__corner_map(f,r*q[k].x,r*q[k].y),sui__corner_map(f,r*q[k+1].x,r*q[k+1].y),
                       sui__corner_map(f,r*q[k+2].x,r*q[k+2].y));
}
static inline int sui__corner_input_ok(SuiCornerRect b,double radius) {
    return isfinite(b.x) && isfinite(b.y) && isfinite(b.width) && isfinite(b.height) && isfinite(b.x+b.width) &&
           isfinite(b.y+b.height) && b.width>=0 && b.height>=0 && isfinite(radius) && radius>=0;
}
/* Atomic append of the continuous rounded rect `b` with corner radius `radius` (clamped; 0: the rectangle), one closed contour
 * clockwise (Y down) from the top-left corner's left edge: SUI_CORNER_RECT_PATH_CAPACITY commands at most. Invalid input or too little
 * capacity appends nothing. */
static inline int sui_corner_path_round_rect(SuiCornerPath* p,SuiCornerRect b,double radius) {
    SuiCornerPoint zero={0,0}; double r; unsigned k;
    if(!p || p->failed || !sui__corner_input_ok(b,radius) || p->count>p->capacity ||
       SUI_CORNER_RECT_PATH_CAPACITY>p->capacity-p->count) { if(p) p->failed=1; return 0; }
    r=sui_corner_radius(b.width,b.height,radius);
    if(!(r>0)) {
        sui_corner_path_push(p,SUI_CORNER_MOVE,sui_corner_point(b.x,b.y),zero,zero); sui_corner_path_line(p,b.x+b.width,b.y);
        sui_corner_path_line(p,b.x+b.width,b.y+b.height); sui_corner_path_line(p,b.x,b.y+b.height);
        sui_corner_path_push(p,SUI_CORNER_CLOSE,zero,zero,zero); return 1;
    }
    for(k=0;k<4;++k) {
        SuiCornerFrame f=sui__corner_frame(b,r,k); SuiCornerPoint start=sui__corner_map(&f,0,r*f.reach_in);
        sui_corner_path_push(p,k ? SUI_CORNER_LINE : SUI_CORNER_MOVE,start,zero,zero);
        sui__corner_path_corner(p,&f,r);
    }
    sui_corner_path_push(p,SUI_CORNER_CLOSE,zero,zero,zero); return 1;
}
/* Cubic q at t offset outward by d along its normal: the offset curve's point and derivative B'(t)(1 + d kappa(t)) (kappa the
 * curvature, positive where a corner turns). Corner-local lengths. */
static inline void sui__corner_offset_at(const SuiCornerPoint q[4],double t,double d,SuiCornerPoint* at,SuiCornerPoint* dt) {
    double m=1-t,l,k; SuiCornerPoint b,d1,d2;
    b=sui_corner_point(m*m*m*q[0].x+3*m*m*t*q[1].x+3*m*t*t*q[2].x+t*t*t*q[3].x,m*m*m*q[0].y+3*m*m*t*q[1].y+3*m*t*t*q[2].y+t*t*t*q[3].y);
    d1=sui_corner_point(3*(m*m*(q[1].x-q[0].x)+2*m*t*(q[2].x-q[1].x)+t*t*(q[3].x-q[2].x)),
                  3*(m*m*(q[1].y-q[0].y)+2*m*t*(q[2].y-q[1].y)+t*t*(q[3].y-q[2].y)));
    d2=sui_corner_point(6*(m*(q[2].x-2*q[1].x+q[0].x)+t*(q[3].x-2*q[2].x+q[1].x)),6*(m*(q[2].y-2*q[1].y+q[0].y)+t*(q[3].y-2*q[2].y+q[1].y)));
    l=sqrt(d1.x*d1.x+d1.y*d1.y);
    if(!(l>1e-12)) { *at=b; *dt=d1; return; }
    k=(d1.x*d2.y-d1.y*d2.x)/(l*l*l);
    *at=sui_corner_point(b.x+d*d1.y/l,b.y-d*d1.x/l);
    *dt=sui_corner_point(d1.x*(1+d*k),d1.y*(1+d*k));
}
/* Atomic append of the outward parallel (offset) curve, at `offset`, of the continuous rounded rect `offset` inside `b` with radius
 * `radius - offset`: a ring drawn around a rounded element keeps the same distance from it all the way round, where a rounded rect of
 * radius `radius` would come closer near each corner's ends. The inner corner's cubics become cubic Hermite pieces (their ends offset
 * along the normals, derivatives B'(1 + d kappa)): the first and last, where the curvature ramps, four each, the middle two; its small
 * turns at the junctions (2.4 degrees) are joined by arcs of radius d about them, as a parallel curve is. A circular corner offsets to
 * the circle of radius `radius`, a square inner corner to the circle of radius `offset`. Within 0.0015 units of the true parallel curve
 * for offsets up to 8 units (a quarter circle's own cubic is 2.7e-4 of its radius off), and within 0.001 of its frame (b). `offset` is clamped to 0..min(radius, half the shorter side); 0 is
 * sui_corner_path_round_rect. SUI_CORNER_OFFSET_PATH_CAPACITY commands at most. */
static inline int sui_corner_path_round_rect_offset(SuiCornerPath* p,SuiCornerRect b,double radius,double offset) {
    SuiCornerPoint zero={0,0}; SuiCornerRect in; double R,d,rb; unsigned k;
    if(!p || p->failed || !sui__corner_input_ok(b,radius) || !isfinite(offset) || offset<0 || p->count>p->capacity ||
       SUI_CORNER_OFFSET_PATH_CAPACITY>p->capacity-p->count) { if(p) p->failed=1; return 0; }
    R=sui_corner_radius(b.width,b.height,radius); d=offset<R ? offset : R;
    if(!(d>0)) return sui_corner_path_round_rect(p,b,R);
    in.x=b.x+d; in.y=b.y+d; in.width=b.width-2*d; in.height=b.height-2*d; rb=R-d;
    for(k=0;k<4;++k) {
        SuiCornerFrame f=sui__corner_frame(b,R,k),g=sui__corner_frame(in,rb,k); SuiCornerPoint q[10],last={0,0},last_d={0,0}; unsigned n,j;
        if(!(rb>0)) { g.reach_in=g.reach_out=1; }
        n=sui_corner_points(g.reach_in,g.reach_out,q);
        if(n==4) {                                                     /* circular (or a square inner corner): the circle of radius R */
            sui_corner_path_push(p,k ? SUI_CORNER_LINE : SUI_CORNER_MOVE,sui__corner_map(&f,0,R),zero,zero);
            sui_corner_path_push(p,SUI_CORNER_CUBIC,sui__corner_map(&f,0,R*SUI__CORNER_CIRCLE_HANDLE),sui__corner_map(&f,R*SUI__CORNER_CIRCLE_HANDLE,0),
                           sui__corner_map(&f,R,0));
            continue;
        }
        for(j=0;j<10;++j) q[j]=sui_corner_point(d+rb*q[j].x,d+rb*q[j].y);   /* the inner corner in this corner's lengths */
        for(j=0;j<3;++j) {
            SuiCornerPoint a,da,c,dc; const SuiCornerPoint* s=&q[3*j]; unsigned h,pieces;
            sui__corner_offset_at(s,0,d,&a,&da);
            if(!j) sui_corner_path_push(p,k ? SUI_CORNER_LINE : SUI_CORNER_MOVE,sui__corner_map(&f,a.x,a.y),zero,zero);
            else {                                                    /* the junction's small turn: the arc of radius d about it */
                double lt=sqrt(last_d.x*last_d.x+last_d.y*last_d.y),ln=sqrt(da.x*da.x+da.y*da.y),turn=0,hh;
                if(lt>1e-12 && ln>1e-12) turn=atan2(last_d.x*da.y-last_d.y*da.x,last_d.x*da.x+last_d.y*da.y);
                hh=4.0/3.0*tan(0.25*turn)*d;
                sui_corner_path_push(p,SUI_CORNER_CUBIC,
                               sui__corner_map(&f,last.x+(lt>1e-12 ? hh*last_d.x/lt : 0),last.y+(lt>1e-12 ? hh*last_d.y/lt : 0)),
                               sui__corner_map(&f,a.x-(ln>1e-12 ? hh*da.x/ln : 0),a.y-(ln>1e-12 ? hh*da.y/ln : 0)),sui__corner_map(&f,a.x,a.y));
            }
            pieces=j==1 ? 2u : 4u;                                    /* the ramps (first and last cubics) in four, the middle in two */
            for(h=1;h<=pieces;++h) {
                const double step=1.0/pieces;
                sui__corner_offset_at(s,step*h,d,&c,&dc);
                sui_corner_path_push(p,SUI_CORNER_CUBIC,sui__corner_map(&f,a.x+da.x*step/3,a.y+da.y*step/3),
                               sui__corner_map(&f,c.x-dc.x*step/3,c.y-dc.y*step/3),sui__corner_map(&f,c.x,c.y));
                a=c; da=dc;
            }
            last=a; last_d=da;
        }
    }
    sui_corner_path_push(p,SUI_CORNER_CLOSE,zero,zero,zero); return 1;
}
/* Whether (x, y) lies inside the continuous rounded rect `b` of corner radius `radius` (clamped), its edge included. O(1): in a corner's
 * zone the curve's height at the point is found by bisection on the cubic that spans it (each is monotone in both directions). */
static inline int sui_corner_round_rect_contains(SuiCornerRect b,double radius,double x,double y) {
    SuiCornerFrame f; SuiCornerPoint q[10]; double r,u,v,dx,dy; unsigned k,n,j;
    if(!(x>=b.x && y>=b.y && x<=b.x+b.width && y<=b.y+b.height)) return 0;
    r=sui_corner_radius(b.width,b.height,radius);
    if(!(r>0)) return 1;
    k=x<b.x+0.5*b.width ? (y<b.y+0.5*b.height ? 0u : 3u) : (y<b.y+0.5*b.height ? 1u : 2u);
    f=sui__corner_frame(b,r,k); dx=x-f.at.x; dy=y-f.at.y;
    u=(dx*f.u.x+dy*f.u.y)/r; v=(dx*f.v.x+dy*f.v.y)/r;            /* the corner's units */
    if(u>=f.reach_out || v>=f.reach_in) return 1;
    n=sui_corner_points(f.reach_in,f.reach_out,q);
    for(j=0;j+3<n;j+=3) {
        double lo=0,hi=1,t=0.5,m,w; unsigned it;
        if(q[j+3].x<u) continue;
        for(it=0;it<32;++it) {                                    /* t to 2e-10: the point to 1e-9 of the radius */
            t=0.5*(lo+hi); m=1-t;
            w=m*m*m*q[j].x+3*m*m*t*q[j+1].x+3*m*t*t*q[j+2].x+t*t*t*q[j+3].x;
            if(w<u) lo=t; else hi=t;
        }
        m=1-t;
        return v>=m*m*m*q[j].y+3*m*m*t*q[j+1].y+3*m*t*t*q[j+2].y+t*t*t*q[j+3].y;
    }
    return 1;
}
#endif
