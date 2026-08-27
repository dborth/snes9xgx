/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiWindow.h
 ***************************************************************************/
#pragma once

//!Allows GuiElements to be grouped together into a "window"
class GuiWindow : public GuiElement
{
	public:
		//!Constructor
		GuiWindow();
		//!\overload
		//!\param w Width of window
		//!\param h Height of window
		GuiWindow(int w, int h);
		//!Destructor
		~GuiWindow();
		//!Appends a GuiElement to the GuiWindow
		//!\param e The GuiElement to append. If it is already in the GuiWindow, it is removed first
		void append(GuiElement* e);
		//!Appends a GuiElement to the GuiWindow, and instructs the GuiElement to inform its parent on destruction
		//!\param e The GuiElement to append. If it is already in the GuiWindow, it is removed first
		void appendWithAutoRemove(GuiElement* e);
		//!Inserts a GuiElement into the GuiWindow at the specified index
		//!\param e The GuiElement to insert. If it is already in the GuiWindow, it is removed first
		//!\param i Index in which to insert the element
		void insert(GuiElement* e, uint32_t i);
		//!Removes the specified GuiElement from the GuiWindow
		//!\param e GuiElement to be removed
		void remove(GuiElement* e);
		//!Removes all GuiElements
		void removeAll();
		//!Looks for the specified GuiElement
		//!\param e The GuiElement to find
		//!\return true if found, false otherwise
		bool find(GuiElement* e);
		//!Returns the GuiElement at the specified index
		//!\param index The index of the element
		//!\return A pointer to the element at the index, nullptr on error (eg: out of bounds)
		GuiElement* getGuiElementAt(uint32_t index) const;
		//!Returns the size of the list of elements
		//!\return The size of the current element list
		uint32_t getSize();
		//!Sets the visibility of the window
		//!\param v visibility (true = visible)
		void setVisible(bool v);
		//!Resets the window's state to STATE::DEFAULT
		void resetState();
		//!Sets the window's state
		//!\param s State
		void setState(STATE s, int c = -1);
		//!Gets the index of the GuiElement inside the window that is currently selected
		//!\return index of selected GuiElement
		int getSelected();
		//!Sets the window focus
		//!\param f Focus
		void setFocus(int f);
		//!Change the focus to the specified element
		//!This is intended for the primary GuiWindow only
		//!\param e GuiElement that should have focus
		void changeFocus(GuiElement * e);
		//!Changes window focus to the next focusable window or element
		//!If no element is in focus, changes focus to the first available element
		//!If B or 1 button is pressed, changes focus to the next available element
		//!This is intended for the primary GuiWindow only
		//!\param c Pointer to a GuiInputController, containing the current input data
		void toggleFocus(GuiInputController * c);
		//!Moves the selected element to the element to the left or right
		//!\param d Direction to move (-1 = left, 1 = right)
		void moveSelectionHor(int d);
		//!Moves the selected element to the element above or below
		//!\param d Direction to move (-1 = up, 1 = down)
		void moveSelectionVert(int d);
		//!Resets the text for all contained elements
		void resetText();
		//!Draws all the elements in this GuiWindow
		void draw();
		//!Updates the window and all elements contains within
		//!Allows the GuiWindow and all elements to respond to the input data specified
		//!\param c Pointer to a GuiInputController, containing the current input data
		void update(GuiInputController * c);
	protected:
		std::vector<GuiElement*> _elements; //!< Contains all elements within the GuiWindow
};
