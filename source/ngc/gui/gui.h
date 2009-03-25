/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * Tantric February 2009
 *
 * gui.h
 *
 * GUI class definitions
 ***************************************************************************/

#ifndef GUI_H
#define GUI_H

#include <gccore.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <math.h>
#include <asndlib.h>
#include "pngu/pngu.h"
#include "FreeTypeGX.h"
#include "snes9xGX.h"
#include "video.h"
#include "input.h"
#include "filelist.h"
#include "fileop.h"
#include "menu.h"
#include "oggplayer.h"

#define SCROLL_INITIAL_DELAY 20
#define SCROLL_LOOP_DELAY 3
#define PAGESIZE 8
#define SAVELISTSIZE 6
#define MAX_SAVES 20
#define MAX_OPTIONS 30

typedef void (*UpdateCallback)(void * e);

enum
{
	ALIGN_LEFT,
	ALIGN_RIGHT,
	ALIGN_CENTRE,
	ALIGN_TOP,
	ALIGN_BOTTOM,
	ALIGN_MIDDLE
};

enum
{
	STATE_DEFAULT,
	STATE_SELECTED,
	STATE_CLICKED,
	STATE_DISABLED
};

enum
{
	SOUND_PCM,
	SOUND_OGG
};

enum
{
	IMAGE_TEXTURE,
	IMAGE_COLOR,
	IMAGE_DATA
};

#define EFFECT_SLIDE_TOP			1
#define EFFECT_SLIDE_BOTTOM			2
#define EFFECT_SLIDE_RIGHT			4
#define EFFECT_SLIDE_LEFT			8
#define EFFECT_SLIDE_IN				16
#define EFFECT_SLIDE_OUT			32
#define EFFECT_FADE					64
#define EFFECT_SCALE				128
#define EFFECT_COLOR_TRANSITION		256

class GuiSound
{
	public:
		GuiSound(const u8 * s, int l, int t);
		~GuiSound();
		void Play();
		void Stop();
		void Pause();
		void Resume();
		void SetVolume(int v);
	protected:
		const u8 * sound;
		int type;
		s32 length;
		s32 voice;
		s32 volume;
};

class GuiElement
{
	public:
		GuiElement();
		~GuiElement();
		void SetParent(GuiElement * e);
		int GetLeft();
		int GetTop();
		int GetWidth();
		int GetHeight();
		void SetSize(int w, int h);
		bool IsVisible();
		bool IsSelectable();
		bool IsClickable();
		void SetSelectable(bool s);
		void SetClickable(bool c);
		int GetState();
		void SetAlpha(int a);
		int GetAlpha();
		void SetScale(float s);
		float GetScale();
		void SetTrigger(GuiTrigger * t);
		void SetTrigger(u8 i, GuiTrigger * t);
		bool Rumble();
		void SetRumble(bool r);
		void SetEffect(int e, int a, int t=0);
		void SetEffectOnOver(int e, int a, int t=0);
		void SetEffectGrow();
		int GetEffect();
		bool IsInside(int x, int y);
		void SetPosition(int x, int y);
		void UpdateEffects();
		void SetUpdateCallback(UpdateCallback u);
		int IsFocused();
		virtual void SetVisible(bool v);
		virtual void SetFocus(int f);
		virtual void SetState(int s);
		virtual void ResetState();
		virtual int GetSelected();
		virtual void SetAlignment(int hor, int vert);
		virtual void Update(GuiTrigger * t);
		virtual void Draw();
	protected:
		bool visible;
		int focus; // -1 = cannot focus, 0 = not focused, 1 = focused
		int width;
		int height;
		int xoffset;
		int yoffset;
		int xoffsetDyn;
		int yoffsetDyn;
		int alpha;
		f32 scale;
		int alphaDyn;
		f32 scaleDyn;
		bool rumble;
		int effects;
		int effectAmount;
		int effectTarget;
		int effectsOver;
		int effectAmountOver;
		int effectTargetOver;
		int alignmentHor; // LEFT, RIGHT, CENTRE
		int alignmentVert; // TOP, BOTTOM, MIDDLE
		int state; // DEFAULT, SELECTED, CLICKED, DISABLED
		bool selectable; // is SELECTED a valid state?
		bool clickable; // is CLICKED a valid state?
		GuiTrigger * trigger[2];
		GuiElement * parentElement;
		UpdateCallback updateCB;
};

//!Groups elements into one window in which they can be managed.
class GuiWindow : public GuiElement
{
	public:
		//!Constructor
		GuiWindow();
		GuiWindow(int w, int h);
		//!Destructor.
		~GuiWindow();

		//!Appends a element at the end, thus drawing it at last.
		//!\param element The element to append. If it is already in the list, it gets removed first.
		void Append(GuiElement* e);
		//!Inserts a element into the manager.
		//!\param element The element to insert. If it is already in the list, it gets removed first.
		//!\param index The new index of the element.
		void Insert(GuiElement* e, u32 index);
		//!Removes a element from the list.
		//!\param element A element that is in the list.
		void Remove(GuiElement* e);
		//!Clears the whole GuiWindow from all GuiElement.
		void RemoveAll();

		//!Returns a element at a specified index.
		//!\param index The index from where to poll the element.
		//!\return A pointer to the element at the index. NULL if index is out of bounds.
		GuiElement* GetGuiElementAt(u32 index) const;
		//!Returns the size of the list of elements.
		//!\return The size of the current elementlist.
		u32 GetSize();
		void SetVisible(bool v);
		void ResetState();
		void SetState(int s);
		int GetSelected();
		void SetFocus(int f);
		void ChangeFocus(GuiElement * e);
		void ToggleFocus(GuiTrigger * t);
		void MoveSelectionHor(int d);
		void MoveSelectionVert(int d);

		//!Draws all the elements in this GuiWindow.
		void Draw();
		void Update(GuiTrigger * t);

	protected:
		std::vector<GuiElement*> _elements;
};

class GuiImageData
{
	public:
		GuiImageData(const u8 * i);
		~GuiImageData();
		u8 * GetImage();
		int GetWidth();
		int GetHeight();
	protected:
		u8 * data;
		int height;
		int width;
};

class GuiImage : public GuiElement
{
	public:
		GuiImage(GuiImageData * img);
		GuiImage(u8 * img, int w, int h);
		GuiImage(int w, int h, GXColor c);
		~GuiImage();
		void SetAngle(float a);
		void SetTile(int t);
		void Draw();
		u8 * GetImage();
		void SetImage(GuiImageData * img);
		void SetImage(u8 * img, int w, int h);
		GXColor GetPixel(int x, int y);
		void SetPixel(int x, int y, GXColor color);
		void ColorStripe(int s);
		void SetStripe(int s);
	protected:
		int imgType;
		u8 * image;
		f32 imageangle;
		int tile;
		int stripe;
};

class GuiText : public GuiElement
{
	public:
		GuiText(const char * t, int s, GXColor c);
		GuiText(const char * t);
		~GuiText();
		void SetText(const char * t);
		void SetPresets(int sz, GXColor c, int w, u16 s, int h, int v);
		void SetFontSize(int s);
		void SetMaxWidth(int w);
		void SetColor(GXColor c);
		void SetStyle(u16 s);
		void SetAlignment(int hor, int vert);
		void Draw();
	protected:
		wchar_t* text;
		int size;
		int maxWidth;
		u16 style;
		GXColor color;
};

class GuiButton : public GuiElement
{
	public:
		GuiButton(int w, int h);
		~GuiButton();
		void SetImage(GuiImage* i);
		void SetImageOver(GuiImage* i);
		void SetIcon(GuiImage* i);
		void SetIconOver(GuiImage* i);
		void SetLabel(GuiText* t);
		void SetLabelOver(GuiText* t);
		void SetLabel(GuiText* t, int n);
		void SetLabelOver(GuiText* t, int n);
		void SetSoundOver(GuiSound * s);
		void SetSoundClick(GuiSound * s);
		void Draw();
		void Update(GuiTrigger * t);
	protected:
		GuiImage * image;
		GuiImage * imageOver;
		GuiImage * icon;
		GuiImage * iconOver;
		GuiText * label[3];
		GuiText * labelOver[3];
		GuiSound * soundOver;
		GuiSound * soundClick;
};

class GuiFileBrowser : public GuiElement
{
	public:
		GuiFileBrowser(int w, int h);
		~GuiFileBrowser();
		void ResetState();
		void SetFocus(int f);
		void Draw();
		void TriggerUpdate();
		void Update(GuiTrigger * t);
		GuiButton * gameList[PAGESIZE];
	protected:
		int selectedItem;
		bool listChanged;

		GuiText * gameListText[PAGESIZE];
		GuiImage * gameListBg[PAGESIZE];
		GuiImage * gameListFolder[PAGESIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;
		GuiButton * scrollbarBoxBtn;

		GuiImage * bgGameSelectionImg;
		GuiImage * scrollbarImg;
		GuiImage * arrowDownImg;
		GuiImage * arrowDownOverImg;
		GuiImage * arrowUpImg;
		GuiImage * arrowUpOverImg;
		GuiImage * scrollbarBoxImg;
		GuiImage * scrollbarBoxOverImg;

		GuiImageData * bgGameSelection;
		GuiImageData * bgGameSelectionEntry;
		GuiImageData * gameFolder;
		GuiImageData * scrollbar;
		GuiImageData * arrowDown;
		GuiImageData * arrowDownOver;
		GuiImageData * arrowUp;
		GuiImageData * arrowUpOver;
		GuiImageData * scrollbarBox;
		GuiImageData * scrollbarBoxOver;

		GuiTrigger * trigA;
};

typedef struct _optionlist {
	int length;
	char name[MAX_OPTIONS][150];
	char value[MAX_OPTIONS][150];
} OptionList;

class GuiOptionBrowser : public GuiElement
{
	public:
		GuiOptionBrowser(int w, int h, OptionList * l);
		~GuiOptionBrowser();
		void SetCol2Position(int x);
		int FindMenuItem(int c, int d);
		int GetClickedOption();
		void ResetState();
		void SetFocus(int f);
		void Draw();
		void Update(GuiTrigger * t);
		GuiText * optionVal[PAGESIZE];
	protected:
		int selectedItem;
		int listOffset;

		OptionList * options;
		int optionIndex[PAGESIZE];
		GuiButton * optionBtn[PAGESIZE];
		GuiText * optionTxt[PAGESIZE];
		GuiImage * optionBg[PAGESIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;
		GuiButton * scrollbarBoxBtn;

		GuiImage * bgOptionsImg;
		GuiImage * scrollbarImg;
		GuiImage * arrowDownImg;
		GuiImage * arrowDownOverImg;
		GuiImage * arrowUpImg;
		GuiImage * arrowUpOverImg;
		GuiImage * scrollbarBoxImg;
		GuiImage * scrollbarBoxOverImg;

		GuiImageData * bgOptions;
		GuiImageData * bgOptionsEntry;
		GuiImageData * scrollbar;
		GuiImageData * arrowDown;
		GuiImageData * arrowDownOver;
		GuiImageData * arrowUp;
		GuiImageData * arrowUpOver;
		GuiImageData * scrollbarBox;
		GuiImageData * scrollbarBoxOver;

		GuiTrigger * trigA;
};

typedef struct _savelist {
	int length;
	char filename[MAX_SAVES][256];
	GuiImageData * previewImg[MAX_SAVES];
	char date[MAX_SAVES][20];
	char time[MAX_SAVES][10];
	int type[MAX_SAVES];
	int files[2][100];
} SaveList;

class GuiSaveBrowser : public GuiElement
{
	public:
		GuiSaveBrowser(int w, int h, SaveList * l, int a);
		~GuiSaveBrowser();
		int GetClickedSave();
		void ResetState();
		void SetFocus(int f);
		void Draw();
		void Update(GuiTrigger * t);
	protected:
		int selectedItem;
		int listOffset;
		int action;

		SaveList * saves;
		GuiButton * saveBtn[SAVELISTSIZE];
		GuiText * saveDate[SAVELISTSIZE];
		GuiText * saveTime[SAVELISTSIZE];
		GuiText * saveType[SAVELISTSIZE];

		GuiImage * saveBgImg[SAVELISTSIZE];
		GuiImage * saveBgOverImg[SAVELISTSIZE];
		GuiImage * savePreviewImg[SAVELISTSIZE];

		GuiButton * arrowUpBtn;
		GuiButton * arrowDownBtn;
		GuiButton * scrollbarBoxBtn;

		GuiImage * scrollbarImg;
		GuiImage * arrowDownImg;
		GuiImage * arrowDownOverImg;
		GuiImage * arrowUpImg;
		GuiImage * arrowUpOverImg;
		GuiImage * scrollbarBoxImg;
		GuiImage * scrollbarBoxOverImg;

		GuiImageData * gameSave;
		GuiImageData * gameSaveOver;
		GuiImageData * gameSaveBlank;
		GuiImageData * scrollbar;
		GuiImageData * arrowDown;
		GuiImageData * arrowDownOver;
		GuiImageData * arrowUp;
		GuiImageData * arrowUpOver;
		GuiImageData * scrollbarBox;
		GuiImageData * scrollbarBoxOver;

		GuiTrigger * trigA;
};

typedef struct _keytype {
	char ch, chShift;
} Key;

class GuiKeyboard : public GuiWindow
{
	public:
		GuiKeyboard(char * t);
		~GuiKeyboard();
		void Update(GuiTrigger * t);
		char kbtextstr[100];
	protected:
		Key keys[4][10];
		int shift;
		int caps;
		GuiText * kbText;
		GuiImage * keyTextboxImg;
		GuiText * keyCapsText;
		GuiImage * keyCapsImg;
		GuiImage * keyCapsOverImg;
		GuiButton * keyCaps;
		GuiText * keyShiftText;
		GuiImage * keyShiftImg;
		GuiImage * keyShiftOverImg;
		GuiButton * keyShift;
		GuiText * keyBackText;
		GuiImage * keyBackImg;
		GuiImage * keyBackOverImg;
		GuiButton * keyBack;
		GuiImage * keySpaceImg;
		GuiImage * keySpaceOverImg;
		GuiButton * keySpace;
		GuiButton * keyBtn[4][10];
		GuiImage * keyImg[4][10];
		GuiImage * keyImgOver[4][10];
		GuiText * keyTxt[4][10];
		GuiImageData * keyTextbox;
		GuiImageData * key;
		GuiImageData * keyOver;
		GuiImageData * keyMedium;
		GuiImageData * keyMediumOver;
		GuiImageData * keyLarge;
		GuiImageData * keyLargeOver;
		GuiSound * keySoundOver;
		GuiTrigger * trigA;
};

#endif
