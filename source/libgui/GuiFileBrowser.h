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

#define FILE_PAGESIZE 			10

//!Resolves the source PNG path for a preview image, given a browser index.
//!Return false (leaving outPath untouched) if entry index has no preview image.
typedef bool (*GuiPreviewPathResolver)(void * context, int index, char * outPath, size_t outPathSize);

//!Called whenever GuiFileBrowser actually changes what target (the
//!GuiImage passed to setPreviewImage()) is showing.
typedef void (*GuiPreviewImageChangedCB)(void * context, GuiImage * target);

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
		GuiImage * fileListBg[FILE_PAGESIZE];
		GuiImage * fileListIcon[FILE_PAGESIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;
		GuiButton * scrollbarBoxBtn;

		GuiImage * bgFileSelectionImg;
		GuiImage * scrollbarImg;
		GuiImage * arrowDownImg;
		GuiImage * arrowDownOverImg;
		GuiImage * arrowUpImg;
		GuiImage * arrowUpOverImg;
		GuiImage * scrollbarBoxImg;
		GuiImage * scrollbarBoxOverImg;

		GuiImageData * bgFileSelection;
		GuiImageData * bgFileSelectionEntry;
		GuiImageData * iconFolder;
		GuiImageData * iconSD;
		GuiImageData * iconUSB;
		GuiImageData * iconDVD;
		GuiImageData * iconSMB;
		GuiImageData * scrollbar;
		GuiImageData * arrowDown;
		GuiImageData * arrowDownOver;
		GuiImageData * arrowUp;
		GuiImageData * arrowUpOver;
		GuiImageData * scrollbarBox;
		GuiImageData * scrollbarBoxOver;

		GuiSound * btnSoundOver;
		GuiSound * btnSoundClick;
		GuiTrigger * trigA;
		GuiTrigger * trigHeldA;

		int selectedItem;
		int numEntries;
		bool listChanged;

		// preview-image support
		std::unique_ptr<GuiImageAsyncCache> previewCache;
		GuiImage * previewTarget = nullptr;
		GuiImageData * previewLastImage = nullptr; // last thing painted into previewTarget, so we only call setImage() on an actual change
		GuiPreviewPathResolver previewResolver = nullptr;
		void * previewResolverContext = nullptr;
		GuiPreviewImageChangedCB previewChangedCB = nullptr;
		void * previewChangedCBContext = nullptr;
		int previewRequestedIndex = -1;
};
