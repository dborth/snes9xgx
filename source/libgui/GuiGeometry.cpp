/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiGeometry.cpp
 ***************************************************************************/

#include "GuiGeometry.h"
#include <cmath>
#include <algorithm>
#include <functional>

namespace {

const float kPi = 3.14159265358979f;

float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

//A closed outline. Every ring produced for one shape has the same vertex count,
//so two rings (say, offset -0.5 and +0.5) can be joined into a band.
struct Ring {
	std::vector<GuiPoint> pts;
};

typedef std::function<Ring(float)> RingFn; //offset from the shape's edge (+ outward) -> ring

//Number of segments for a quarter circle of radius r within the tolerance
int arcCount(float radius, float tol)
{
	if(radius <= tol)
		return 1;
	float ang = acosf(clampf(1.0f - tol / radius, -1.0f, 1.0f));
	if(ang < 1e-4f)
		return 24;
	return (int)clampf(ceilf((kPi * 0.5f) / ang), 2.0f, 24.0f);
}

//Number of segments for a full circle/ellipse of the given largest radius
int circleCount(float radius, float tol)
{
	if(radius <= tol)
		return 8;
	float ang = acosf(clampf(1.0f - tol / radius, -1.0f, 1.0f));
	if(ang < 1e-4f)
		return 96;
	return (int)clampf(ceilf((2.0f * kPi) / ang), 12.0f, 96.0f);
}

//Evaluates a GuiFill at a point. A gradient is linear in position, so
//evaluating it per vertex and letting the GPU interpolate is exact.
struct FillEval {
	PixelColor from, to;
	bool gradient;
	float ux, uy, t0, invSpan;

	FillEval(const GuiFill& f, const GuiRect& b) :
		from(f.from), to(f.to), gradient(f.gradient), ux(0), uy(0), t0(0), invSpan(0)
	{
		if(!gradient)
			return;
		float rad = f.angle * kPi / 180.0f;
		ux = cosf(rad);
		uy = sinf(rad);
		float d0 = b.x * ux + b.y * uy;
		float d1 = (b.x + b.w) * ux + b.y * uy;
		float d2 = b.x * ux + (b.y + b.h) * uy;
		float d3 = (b.x + b.w) * ux + (b.y + b.h) * uy;
		float lo = std::min(std::min(d0, d1), std::min(d2, d3));
		float hi = std::max(std::max(d0, d1), std::max(d2, d3));
		t0 = lo;
		invSpan = (hi - lo) > 1e-6f ? 1.0f / (hi - lo) : 0.0f;
	}

	static uint8_t mix(uint8_t a, uint8_t b, float t)
	{
		return (uint8_t)clampf(a + (b - a) * t + 0.5f, 0.0f, 255.0f);
	}

	PixelColor at(GuiPoint p, float alphaScale) const
	{
		PixelColor c = from;
		if(gradient)
		{
			float t = clampf((p.x * ux + p.y * uy - t0) * invSpan, 0.0f, 1.0f);
			c.r = mix(from.r, to.r, t);
			c.g = mix(from.g, to.g, t);
			c.b = mix(from.b, to.b, t);
			c.a = mix(from.a, to.a, t);
		}
		c.a = (uint8_t)clampf(c.a * alphaScale + 0.5f, 0.0f, 255.0f);
		return c;
	}
};

void pushVertex(std::vector<MeshVertex>& out, GuiPoint p, const PixelColor& c)
{
	MeshVertex v;
	v.x = p.x;
	v.y = p.y;
	v.c = c;
	out.push_back(v);
}

//Triangles centre -> ring[i] -> ring[i+1] (closed), all at one alpha scale
void emitFan(std::vector<MeshVertex>& out, GuiPoint centre, const Ring& r, const FillEval& e, float a)
{
	size_t n = r.pts.size();
	if(n < 3)
		return;
	PixelColor cc = e.at(centre, a);
	for(size_t i = 0; i < n; i++)
	{
		const GuiPoint& p0 = r.pts[i];
		const GuiPoint& p1 = r.pts[(i + 1) % n];
		pushVertex(out, centre, cc);
		pushVertex(out, p0, e.at(p0, a));
		pushVertex(out, p1, e.at(p1, a));
	}
}

//Quads between ring A and the (outer) ring B: one alpha scale per ring
void emitBand(std::vector<MeshVertex>& out, const Ring& A, float aA, const Ring& B, float aB, const FillEval& e)
{
	size_t n = A.pts.size();
	if(n < 3 || B.pts.size() != n)
		return;
	for(size_t i = 0; i < n; i++)
	{
		size_t j = (i + 1) % n;
		PixelColor ca0 = e.at(A.pts[i], aA), ca1 = e.at(A.pts[j], aA);
		PixelColor cb0 = e.at(B.pts[i], aB), cb1 = e.at(B.pts[j], aB);
		pushVertex(out, A.pts[i], ca0);
		pushVertex(out, B.pts[i], cb0);
		pushVertex(out, B.pts[j], cb1);
		pushVertex(out, A.pts[i], ca0);
		pushVertex(out, B.pts[j], cb1);
		pushVertex(out, A.pts[j], ca1);
	}
}

Ring roundRectRing(const GuiRect& r, const GuiCorners& c, float d, int n)
{
	Ring ring;
	float half = 0.5f * std::min(r.w, r.h);
	d = std::max(d, -half); //never invert the rectangle
	float rad[4] = {
		clampf(c.tl, 0.0f, half), clampf(c.tr, 0.0f, half),
		clampf(c.br, 0.0f, half), clampf(c.bl, 0.0f, half)
	};
	GuiPoint centre[4] = {
		{ r.x + rad[0], r.y + rad[0] },
		{ r.x + r.w - rad[1], r.y + rad[1] },
		{ r.x + r.w - rad[2], r.y + r.h - rad[2] },
		{ r.x + rad[3], r.y + r.h - rad[3] }
	};
	//The corner of the offset rectangle, used where the offset radius would go negative
	GuiPoint sharp[4] = {
		{ r.x - d, r.y - d },
		{ r.x + r.w + d, r.y - d },
		{ r.x + r.w + d, r.y + r.h + d },
		{ r.x - d, r.y + r.h + d }
	};
	const float start[4] = { 180.0f, 270.0f, 0.0f, 90.0f };

	ring.pts.reserve(4 * (n + 1));
	for(int k = 0; k < 4; k++)
	{
		float rr = rad[k] + d;
		for(int i = 0; i <= n; i++)
		{
			if(rr >= 0.0f)
			{
				float th = (start[k] + 90.0f * (float)i / (float)n) * kPi / 180.0f;
				GuiPoint p = { centre[k].x + rr * cosf(th), centre[k].y + rr * sinf(th) };
				ring.pts.push_back(p);
			}
			else
				ring.pts.push_back(sharp[k]);
		}
	}
	return ring;
}

Ring ellipseRing(const GuiRect& r, float d, int count)
{
	Ring ring;
	float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
	float rx = std::max(0.0f, r.w * 0.5f + d), ry = std::max(0.0f, r.h * 0.5f + d);
	ring.pts.reserve(count);
	for(int i = 0; i < count; i++)
	{
		float t = 2.0f * kPi * (float)i / (float)count;
		GuiPoint p = { cx + rx * cosf(t), cy + ry * sinf(t) };
		ring.pts.push_back(p);
	}
	return ring;
}

//A convex polygon with a miter vector per corner, so any offset ring is p + d*m
struct Poly {
	std::vector<GuiPoint> p;
	std::vector<GuiPoint> m;
	GuiRect bounds;
	GuiPoint centre;
};

bool buildPoly(const std::vector<GuiPoint>& in, Poly& out)
{
	out.p.clear();
	out.m.clear();
	for(size_t i = 0; i < in.size(); i++)
	{
		const GuiPoint& q = in[i];
		if(!out.p.empty() && fabsf(q.x - out.p.back().x) < 1e-4f && fabsf(q.y - out.p.back().y) < 1e-4f)
			continue; //repeated point
		out.p.push_back(q);
	}
	while(out.p.size() > 1 && fabsf(out.p.front().x - out.p.back().x) < 1e-4f && fabsf(out.p.front().y - out.p.back().y) < 1e-4f)
		out.p.pop_back();
	size_t n = out.p.size();
	if(n < 3)
		return false;

	float area = 0.0f; //positive = clockwise on screen (+Y down)
	for(size_t i = 0; i < n; i++)
	{
		const GuiPoint& a = out.p[i];
		const GuiPoint& b = out.p[(i + 1) % n];
		area += a.x * b.y - b.x * a.y;
	}
	if(fabsf(area) < 1e-6f)
		return false;
	if(area < 0.0f)
		std::reverse(out.p.begin(), out.p.end());

	float minx = out.p[0].x, maxx = minx, miny = out.p[0].y, maxy = miny, sx = 0, sy = 0;
	for(size_t i = 0; i < n; i++)
	{
		minx = std::min(minx, out.p[i].x);
		maxx = std::max(maxx, out.p[i].x);
		miny = std::min(miny, out.p[i].y);
		maxy = std::max(maxy, out.p[i].y);
		sx += out.p[i].x;
		sy += out.p[i].y;
	}
	out.bounds.x = minx;
	out.bounds.y = miny;
	out.bounds.w = maxx - minx;
	out.bounds.h = maxy - miny;
	out.centre.x = sx / (float)n;
	out.centre.y = sy / (float)n;

	for(size_t i = 0; i < n; i++)
	{
		const GuiPoint& a = out.p[(i + n - 1) % n];
		const GuiPoint& b = out.p[i];
		const GuiPoint& c = out.p[(i + 1) % n];
		float e1x = b.x - a.x, e1y = b.y - a.y, l1 = sqrtf(e1x * e1x + e1y * e1y);
		float e2x = c.x - b.x, e2y = c.y - b.y, l2 = sqrtf(e2x * e2x + e2y * e2y);
		float n1x = e1y / l1, n1y = -e1x / l1; //outward normal of a clockwise-on-screen edge
		float n2x = e2y / l2, n2y = -e2x / l2;
		float denom = 1.0f + n1x * n2x + n1y * n2y;
		if(denom < 0.25f)
			denom = 0.25f; //limit the miter on very sharp corners
		GuiPoint m = { (n1x + n2x) / denom, (n1y + n2y) / denom };
		out.m.push_back(m);
	}
	return true;
}

Ring polyRing(const Poly& poly, float d)
{
	Ring ring;
	ring.pts.reserve(poly.p.size());
	for(size_t i = 0; i < poly.p.size(); i++)
	{
		GuiPoint q = { poly.p[i].x + d * poly.m[i].x, poly.p[i].y + d * poly.m[i].y };
		ring.pts.push_back(q);
	}
	return ring;
}

float smoothstep01(float t)
{
	t = clampf(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

} // namespace

GuiFill GuiFill::solid(PixelColor c)
{
	GuiFill f;
	f.from = c;
	f.to = c;
	f.angle = 0.0f;
	f.gradient = false;
	return f;
}

GuiFill GuiFill::linear(PixelColor a, PixelColor b, float angleDeg)
{
	GuiFill f;
	f.from = a;
	f.to = b;
	f.angle = angleDeg;
	f.gradient = true;
	return f;
}

GuiMesh::GuiMesh(float featherWidth, float curveTolerance) :
	feather(featherWidth < 0.0f ? 0.0f : featherWidth),
	tolerance(curveTolerance < 0.01f ? 0.01f : curveTolerance)
{}

void GuiMesh::clear()
{
	verts.clear();
}

//Solid fill with an anti-aliasing fringe centered on the edge: opaque out to
//-feather/2, fading to alpha 0 at +feather/2.
static void fillShape(std::vector<MeshVertex>& out, float feather, const RingFn& ring, GuiPoint centre, const GuiRect& bounds, const GuiFill& fill, bool edgeFringe)
{
	FillEval ev(fill, bounds);
	float h = feather * 0.5f;
	Ring inner = ring(-h);
	emitFan(out, centre, inner, ev, 1.0f);
	if(feather > 0.0f && edgeFringe)
		emitBand(out, inner, 1.0f, ring(h), 0.0f, ev);
}

//A stroke between two offsets from the edge, feathered on both sides. A stroke
//thinner than the feather is drawn as a fainter, wider line, so its total
//coverage stays equal to its width.
static void strokeShape(std::vector<MeshVertex>& out, float feather, const RingFn& ring, const GuiRect& bounds, float width, STROKE align, const GuiFill& fill)
{
	if(width <= 0.0f)
		return;
	float d1, d2;
	switch(align)
	{
		case STROKE::INSIDE:  d1 = -width; d2 = 0.0f; break;
		case STROKE::OUTSIDE: d1 = 0.0f;   d2 = width; break;
		default:              d1 = -width * 0.5f; d2 = width * 0.5f; break;
	}
	FillEval ev(fill, bounds);
	float h = feather * 0.5f;
	if(feather <= 0.0f)
	{
		emitBand(out, ring(d1), 1.0f, ring(d2), 1.0f, ev);
		return;
	}
	if(width < feather)
	{
		//Triangle profile whose area equals the width: peak alpha 2w/f over a feather-wide base,
		//or full alpha over a base of 2w once that would exceed 1
		float mid = (d1 + d2) * 0.5f;
		float a = std::min(1.0f, 2.0f * width / feather);
		float hb = width / a; //half of the base width
		Ring rm = ring(mid);
		emitBand(out, ring(mid - hb), 0.0f, rm, a, ev);
		emitBand(out, rm, a, ring(mid + hb), 0.0f, ev);
		return;
	}
	Ring a0 = ring(d1 - h), a1 = ring(d1 + h), b1 = ring(d2 - h), b0 = ring(d2 + h);
	emitBand(out, a0, 0.0f, a1, 1.0f, ev);
	emitBand(out, a1, 1.0f, b1, 1.0f, ev);
	emitBand(out, b1, 1.0f, b0, 0.0f, ev);
}

void GuiMesh::addRect(const GuiRect& r, const GuiFill& fill, bool edgeFringe)
{
	std::vector<GuiPoint> pts(4);
	pts[0].x = r.x;       pts[0].y = r.y;
	pts[1].x = r.x + r.w; pts[1].y = r.y;
	pts[2].x = r.x + r.w; pts[2].y = r.y + r.h;
	pts[3].x = r.x;       pts[3].y = r.y + r.h;
	addPolygon(pts, fill, edgeFringe);
}

void GuiMesh::addRoundRect(const GuiRect& r, const GuiCorners& c, const GuiFill& fill, bool edgeFringe)
{
	if(r.w <= 0.0f || r.h <= 0.0f)
		return;
	if(c.tl <= 0.0f && c.tr <= 0.0f && c.br <= 0.0f && c.bl <= 0.0f)
	{
		addRect(r, fill, edgeFringe);
		return;
	}
	float maxR = std::max(std::max(c.tl, c.tr), std::max(c.br, c.bl));
	maxR = std::min(maxR, 0.5f * std::min(r.w, r.h));
	int n = arcCount(maxR + feather, tolerance);
	GuiPoint centre = { r.x + r.w * 0.5f, r.y + r.h * 0.5f };
	RingFn ring = [=](float d) { return roundRectRing(r, c, d, n); };
	fillShape(verts, feather, ring, centre, r, fill, edgeFringe);
}

void GuiMesh::addEllipse(const GuiRect& r, const GuiFill& fill, bool edgeFringe)
{
	if(r.w <= 0.0f || r.h <= 0.0f)
		return;
	int n = circleCount(0.5f * std::max(r.w, r.h) + feather, tolerance);
	GuiPoint centre = { r.x + r.w * 0.5f, r.y + r.h * 0.5f };
	RingFn ring = [=](float d) { return ellipseRing(r, d, n); };
	fillShape(verts, feather, ring, centre, r, fill, edgeFringe);
}

void GuiMesh::addPolygon(const std::vector<GuiPoint>& pts, const GuiFill& fill, bool edgeFringe)
{
	Poly poly;
	if(!buildPoly(pts, poly))
		return;
	RingFn ring = [&poly](float d) { return polyRing(poly, d); };
	fillShape(verts, feather, ring, poly.centre, poly.bounds, fill, edgeFringe);
}

void GuiMesh::addLine(GuiPoint a, GuiPoint b, float width, const GuiFill& fill)
{
	float dx = b.x - a.x, dy = b.y - a.y;
	float len = sqrtf(dx * dx + dy * dy);
	if(len < 1e-4f || width <= 0.0f)
		return;
	float nx = dy / len * width * 0.5f, ny = -dx / len * width * 0.5f;
	std::vector<GuiPoint> pts(4);
	pts[0].x = a.x + nx; pts[0].y = a.y + ny;
	pts[1].x = b.x + nx; pts[1].y = b.y + ny;
	pts[2].x = b.x - nx; pts[2].y = b.y - ny;
	pts[3].x = a.x - nx; pts[3].y = a.y - ny;
	addPolygon(pts, fill);
}

void GuiMesh::strokeRoundRect(const GuiRect& r, const GuiCorners& c, float width, STROKE align, const GuiFill& fill)
{
	if(r.w <= 0.0f || r.h <= 0.0f || width <= 0.0f)
		return;
	float half = 0.5f * std::min(r.w, r.h);
	if(align == STROKE::INSIDE)
		width = std::min(width, half);
	else if(align == STROKE::CENTRE)
		width = std::min(width, 2.0f * half);
	float maxR = std::min(std::max(std::max(c.tl, c.tr), std::max(c.br, c.bl)), half);
	int n = arcCount(maxR + width + feather, tolerance);
	RingFn ring = [=](float d) { return roundRectRing(r, c, d, n); };
	strokeShape(verts, feather, ring, r, width, align, fill);
}

void GuiMesh::strokeEllipse(const GuiRect& r, float width, STROKE align, const GuiFill& fill)
{
	if(r.w <= 0.0f || r.h <= 0.0f || width <= 0.0f)
		return;
	float half = 0.5f * std::min(r.w, r.h);
	if(align == STROKE::INSIDE)
		width = std::min(width, half);
	else if(align == STROKE::CENTRE)
		width = std::min(width, 2.0f * half);
	int n = circleCount(0.5f * std::max(r.w, r.h) + width + feather, tolerance);
	RingFn ring = [=](float d) { return ellipseRing(r, d, n); };
	strokeShape(verts, feather, ring, r, width, align, fill);
}

void GuiMesh::strokePolygon(const std::vector<GuiPoint>& pts, float width, STROKE align, const GuiFill& fill)
{
	Poly poly;
	if(width <= 0.0f || !buildPoly(pts, poly))
		return;
	RingFn ring = [&poly](float d) { return polyRing(poly, d); };
	strokeShape(verts, feather, ring, poly.bounds, width, align, fill);
}

void GuiMesh::strokeArc(GuiPoint centre, float radius, float startDeg, float sweepDeg, float width, PixelColor color)
{
	if(radius <= 0.0f || width <= 0.0f || sweepDeg == 0.0f)
		return;
	if(sweepDeg < 0.0f)
	{
		startDeg += sweepDeg;
		sweepDeg = -sweepDeg;
	}
	sweepDeg = std::min(sweepDeg, 360.0f);

	float h = feather * 0.5f;
	float radii[4];
	float alphas[4];
	int rings;
	if(feather > 0.0f && width < feather)
	{
		float a = std::min(1.0f, 2.0f * width / feather);
		float hb = width / a;
		radii[0] = radius - hb; alphas[0] = 0.0f;
		radii[1] = radius;      alphas[1] = a;
		radii[2] = radius + hb; alphas[2] = 0.0f;
		rings = 3;
	}
	else if(feather > 0.0f)
	{
		radii[0] = radius - width * 0.5f - h; alphas[0] = 0.0f;
		radii[1] = radius - width * 0.5f + h; alphas[1] = 1.0f;
		radii[2] = radius + width * 0.5f - h; alphas[2] = 1.0f;
		radii[3] = radius + width * 0.5f + h; alphas[3] = 0.0f;
		rings = 4;
	}
	else
	{
		radii[0] = radius - width * 0.5f; alphas[0] = 1.0f;
		radii[1] = radius + width * 0.5f; alphas[1] = 1.0f;
		rings = 2;
	}

	float outer = radius + width * 0.5f + h;
	float step = acosf(clampf(1.0f - tolerance / outer, -1.0f, 1.0f));
	if(step < 1e-4f)
		step = 1e-4f;
	int segs = (int)clampf(ceilf((sweepDeg * kPi / 180.0f) / step), 1.0f, 128.0f);

	GuiRect bounds = { centre.x - outer, centre.y - outer, 2.0f * outer, 2.0f * outer };
	FillEval ev(GuiFill::solid(color), bounds);

	std::vector<Ring> samples((size_t)rings);
	for(int k = 0; k < rings; k++)
	{
		for(int i = 0; i <= segs; i++)
		{
			float th = (startDeg + sweepDeg * (float)i / (float)segs) * kPi / 180.0f;
			GuiPoint p = { centre.x + radii[k] * cosf(th), centre.y + radii[k] * sinf(th) };
			samples[(size_t)k].pts.push_back(p);
		}
	}
	for(int k = 0; k + 1 < rings; k++)
	{
		const Ring& A = samples[(size_t)k];
		const Ring& B = samples[(size_t)k + 1];
		for(int i = 0; i < segs; i++)
		{
			PixelColor ca0 = ev.at(A.pts[i], alphas[k]), ca1 = ev.at(A.pts[i + 1], alphas[k]);
			PixelColor cb0 = ev.at(B.pts[i], alphas[k + 1]), cb1 = ev.at(B.pts[i + 1], alphas[k + 1]);
			pushVertex(verts, A.pts[i], ca0);
			pushVertex(verts, B.pts[i], cb0);
			pushVertex(verts, B.pts[i + 1], cb1);
			pushVertex(verts, A.pts[i], ca0);
			pushVertex(verts, B.pts[i + 1], cb1);
			pushVertex(verts, A.pts[i + 1], ca1);
		}
	}
}

void GuiMesh::addShadow(const GuiRect& r, const GuiCorners& c, float blur, float spread, float dx, float dy, PixelColor color)
{
	if(r.w <= 0.0f || r.h <= 0.0f || color.a == 0)
		return;
	blur = std::max(blur, 0.0f);
	GuiRect s = { r.x + dx, r.y + dy, r.w, r.h };
	// One band per ~1.5px of blur is enough to follow the smoothstep falloff; a fixed six
	// bands made a 4px key shadow cost as many triangles as a 26px dialog shadow
	const int maxSteps = 6;
	const int steps = (int)clampf(ceilf(blur / 1.5f), 2.0f, (float)maxSteps);
	float maxR = std::min(std::max(std::max(c.tl, c.tr), std::max(c.br, c.bl)), 0.5f * std::min(r.w, r.h));
	int n = arcCount(maxR + std::max(spread, 0.0f) + blur, tolerance * 2.0f);
	FillEval ev(GuiFill::solid(color), s);
	GuiPoint centre = { s.x + s.w * 0.5f, s.y + s.h * 0.5f };

	std::vector<Ring> rings;
	float alpha[maxSteps + 1];
	for(int k = 0; k <= steps; k++)
	{
		float t = (float)k / (float)steps;
		rings.push_back(roundRectRing(s, c, spread - blur * 0.5f + blur * t, n));
		alpha[k] = 1.0f - smoothstep01(t);
	}
	emitFan(verts, centre, rings[0], ev, alpha[0]);
	for(int k = 0; k < steps; k++)
		emitBand(verts, rings[(size_t)k], alpha[k], rings[(size_t)k + 1], alpha[k + 1], ev);
}

std::vector<GuiPoint> GuiRoundRectPolygon(const GuiRect& r, float radius, float angleDeg, float tolerance)
{
	std::vector<GuiPoint> out;
	if(r.w <= 0.0f || r.h <= 0.0f)
		return out;

	const float half = 0.5f * std::min(r.w, r.h);
	const float rad = clampf(radius, 0.0f, half);
	const int n = arcCount(rad, tolerance < 0.01f ? 0.01f : tolerance);
	Ring ring = roundRectRing(r, GuiCorners(rad), 0.0f, n);

	const float a = angleDeg * kPi / 180.0f;
	const float c = cosf(a), s = sinf(a);
	const float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
	out.reserve(ring.pts.size());
	for(size_t i = 0; i < ring.pts.size(); i++)
	{
		const float dx = ring.pts[i].x - cx, dy = ring.pts[i].y - cy;
		GuiPoint p = { cx + dx * c - dy * s, cy + dx * s + dy * c };
		out.push_back(p);
	}
	return out;
}
