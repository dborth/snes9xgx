/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiFileBrowser.h
 ***************************************************************************/
#pragma once

#include <cstddef>
#include <memory>

#include "GuiImageAsyncCache.h"

class GuiImage;
class GuiShape;
class GuiArrow;

#define FILE_PAGESIZE 			9

//!Resolves the source PNG path for a preview image, given a browser index.
//!Return false (leaving outPath untouched) if entry index has no preview image.
typedef bool (*GuiPreviewPathResolver)(void * context, int index, char * outPath, size_t outPathSize);

//!Called whenever GuiFileBrowser actually changes what target (the
//!GuiImage passed to setPreviewImage()) is showing. isDefault is true when it's the
//!built-in default image rather than the selected entry's own preview.
typedef void (*GuiPreviewImageChangedCB)(void * context, GuiImage * target, bool isDefault);

//!Display a list of files
class GuiFileBrowser : public GuiElement
{
	public:
		GuiFileBrowser(int w, int h);
		~GuiFileBrowser();
		void resetState();
		void setFocus(int f);
		void draw() override;
		void triggerUpdate();
		void update(InputController * c);
		GuiButton * fileList[FILE_PAGESIZE];

		//!Enables an automatically-managed preview image for the currently selected/highlighted entry.
		//!Each frame, GuiFileBrowser asks resolver for the source PNG path of the selected entry
		void setPreviewImage(GuiImage * target, GuiPreviewPathResolver resolver, void * resolverContext,
		                     int capacity, int prefetchRadius, int maxImageWidth = 0, int maxImageHeight = 0,
		                     GuiPreviewImageChangedCB changedCB = nullptr, void * changedCBContext = nullptr);

		//!Forces the next update() to re-resolve and re-request the current selection's preview,
		//!and drops every cached/in-flight entry.
		void refreshPreview();
	protected:
		GuiText * fileListText[FILE_PAGESIZE];
		GuiShape * fileListBg[FILE_PAGESIZE];
		GuiImage * fileListIcon[FILE_PAGESIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;
		GuiButton * scrollbarBoxBtn;

		GuiShape * cardShape;
		GuiShape * trackShape;
		GuiShape * arrowUpShape;
		GuiShape * arrowDownShape;
		GuiArrow * arrowUpGlyph;
		GuiArrow * arrowDownGlyph;
		GuiShape * thumbShape;

		GuiImageData * iconFolder;
		GuiImageData * iconSD;
		GuiImageData * iconUSB;
		GuiImageData * iconDVD;
		GuiImageData * iconSMB;

		GuiTrigger * trigA;
		GuiTrigger * trigHeldA;

		int scrollTravel;		// how far the scrollbar thumb can move, in pixels
		int selectedItem;
		int numEntries;
		bool listChanged;

		// How long the on-screen scrollbar up/down arrow has been continuously held
		// Drives the fast-scroll step size in update(). Reset to 0 whenever
		// neither arrow is held.
		float arrowHoldDuration = 0.0f;

		// preview-image support
		std::unique_ptr<GuiImageAsyncCache> previewCache;
		GuiImage * previewTarget = nullptr;
		GuiImageData * previewLastImage = nullptr; // last thing painted into previewTarget, so we only call setImage() on an actual change
		GuiPreviewPathResolver previewResolver = nullptr;
		void * previewResolverContext = nullptr;
		GuiPreviewImageChangedCB previewChangedCB = nullptr;
		void * previewChangedCBContext = nullptr;
		int previewRequestedIndex = -1;
		bool previewSelHasPath = false; // whether the selected entry has a preview file to load at all
		GuiImageData previewDefaultImage; // shown for entries with no preview; the panel is never left empty
		void showPreview(GuiImageData * img, bool isDefault);
};
