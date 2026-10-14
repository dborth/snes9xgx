/****************************************************************************
 * libgui
 * Daryl Borth 2026
 * GuiMeshCache.cpp
 ***************************************************************************/

#include "Gui.h"
#include "../drivers/Mutex.h"

namespace {

//Meshes no shape holds are kept for the next shape that wants one, the longest idle going first
//once they add up to this many vertices (16 bytes each, plus the backend's copy). A keyboard
//uses about nine distinct meshes of a thousand or so vertices
const size_t kMaxIdleVertices = 32768;

//A mesh is handed to the backend the second time it is drawn, so a shape that changes every
//frame (a progress bar filling up) never pays for a backend copy it won't reuse
const uint32_t kUsesBeforeHandle = 2;

std::vector<GuiSharedMesh *> entries;
size_t idleVertices = 0;
uint32_t clockTick = 0;
Mutex * lockPtr = nullptr;

Mutex& cacheLock()
{
	// Created on first use, which is the first shape the menu draws, on the draw thread
	if(!lockPtr)
		lockPtr = new Mutex();
	return *lockPtr;
}

void destroyEntry(GuiSharedMesh * m, void * handle)
{
	if(handle)
	{
		VideoDriver * video = platform ? platform->getVideo() : nullptr;
		if(video && video->getImageRenderer())
			video->getImageRenderer()->destroyMesh(handle);
	}
	delete m;
}

} // namespace

bool GuiMeshKey::operator==(const GuiMeshKey& o) const
{
	return kind == o.kind && width == o.width && height == o.height && feather == o.feather &&
		tolerance == o.tolerance && corners.tl == o.corners.tl && corners.tr == o.corners.tr &&
		corners.br == o.corners.br && corners.bl == o.corners.bl && paint == o.paint;
}

GuiSharedMesh::GuiSharedMesh(const GuiMeshKey& k) :
	key(k), mesh(k.feather, k.tolerance), handle(nullptr), noHandle(false), uses(0), refs(0), idleStamp(0)
{
	GuiRect r = { 0.0f, 0.0f, (float)k.width, (float)k.height };
	GuiAddPaintedShape(mesh, k.kind, r, k.corners, k.paint);
}

GuiSharedMesh * GuiMeshCache::acquire(const GuiMeshKey& key)
{
	MutexLock lock(cacheLock());

	for(size_t i = 0; i < entries.size(); i++)
	{
		GuiSharedMesh * m = entries[i];
		if(m->key == key)
		{
			if(m->refs == 0)
				idleVertices -= m->mesh.size();
			m->refs++;
			return m;
		}
	}

	GuiSharedMesh * m = new GuiSharedMesh(key);
	m->refs = 1;
	entries.push_back(m);
	return m;
}

void GuiMeshCache::release(GuiSharedMesh * m)
{
	if(!m)
		return;

	std::vector<std::pair<GuiSharedMesh *, void *> > evicted;
	{
		MutexLock lock(cacheLock());

		if(--m->refs > 0)
			return;

		m->idleStamp = ++clockTick;
		idleVertices += m->mesh.size();

		// over budget: drop the meshes that have been idle longest
		while(idleVertices > kMaxIdleVertices)
		{
			size_t oldest = entries.size();
			for(size_t i = 0; i < entries.size(); i++)
			{
				if(entries[i]->refs == 0 && (oldest == entries.size() || entries[i]->idleStamp < entries[oldest]->idleStamp))
					oldest = i;
			}
			if(oldest == entries.size())
				break;
			idleVertices -= entries[oldest]->mesh.size();
			evicted.push_back(std::make_pair(entries[oldest], entries[oldest]->handle));
			entries.erase(entries.begin() + oldest);
		}
	}

	// the backend is asked to free its copies outside the lock
	for(size_t i = 0; i < evicted.size(); i++)
		destroyEntry(evicted[i].first, evicted[i].second);
}

void GuiMeshCache::flush()
{
	std::vector<std::pair<GuiSharedMesh *, void *> > freed;
	{
		MutexLock lock(cacheLock());

		for(size_t i = 0; i < entries.size();)
		{
			if(entries[i]->refs == 0)
			{
				freed.push_back(std::make_pair(entries[i], entries[i]->handle));
				entries.erase(entries.begin() + i);
			}
			else
				i++;
		}
		idleVertices = 0;
	}

	for(size_t i = 0; i < freed.size(); i++)
		destroyEntry(freed[i].first, freed[i].second);
}

void GuiMeshCache::draw(GuiSharedMesh * m, float x, float y, float scaleX, float scaleY, uint8_t alpha, std::vector<MeshVertex>& scratch)
{
	if(!m)
		return;
	const std::vector<MeshVertex>& src = m->mesh.vertices();
	if(src.empty())
		return;

	ImageRenderer * images = platform->getVideo()->getImageRenderer();

	m->uses++;
	if(!m->handle && !m->noHandle && m->uses >= kUsesBeforeHandle)
	{
		m->handle = images->createMesh(src.data(), src.size());
		if(!m->handle)
			m->noHandle = true;
	}

	// The backend's copy has its vertex colors baked in, so only a fully opaque draw can use it.
	// A shape that is fading (an effect, a window sliding in) takes the path that rewrites alpha.
	if(m->handle && alpha == 255)
	{
		images->drawMeshInstance(m->handle, x, y, scaleX, scaleY);
		return;
	}

	scratch.resize(src.size());
	for(size_t i = 0; i < src.size(); i++)
	{
		scratch[i].x = x + src[i].x * scaleX;
		scratch[i].y = y + src[i].y * scaleY;
		scratch[i].c = src[i].c;
		if(alpha < 255)
			scratch[i].c.a = (uint8_t)((src[i].c.a * alpha + 127) / 255);
	}
	images->drawMesh(scratch.data(), scratch.size());
}
