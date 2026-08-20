/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiImage.h
 ***************************************************************************/
#pragma once

enum class IMAGE {
	TEXTURE,
	COLOR,
	DATA
};

//!Display, manage, and manipulate images in the GUI
class GuiImage : public GuiElement
{
	public:
		//!Constructor
		GuiImage();
		//!\overload
		//!\param img Pointer to GuiImageData element
		GuiImage(GuiImageData * img);
		//!\overload
		//!Sets up a new image from the image data specified
		//!\param img
		//!\param w Image width
		//!\param h Image height
		GuiImage(uint8_t * img, int w, int h);
		//!\overload
		//!Creates an image filled with the specified color
		//!\param w Image width
		//!\param h Image height
		//!\param c Image color
		GuiImage(int w, int h, GuiColor c);
		//!Destructor
		~GuiImage();
		//!Sets the image rotation angle for drawing
		//!\param a Angle (in degrees)
		void setAngle(float a);
		//!Sets the number of times to draw the image horizontally
		//!\param t Number of times to draw the image
		void setTile(int t);
		//!Constantly called to draw the image
		void draw() override;
		//!Gets the image data
		//!\return pointer to image data
		uint8_t * getImage();
		//!Sets up a new image using the GuiImageData object specified
		//!\param img Pointer to GuiImageData object
		void setImage(GuiImageData * img);
		//!\overload
		//!\param img Pointer to image data
		//!\param w Width
		//!\param h Height
		void setImage(uint8_t * img, int w, int h);
		//!Gets the pixel color at the specified coordinates of the image
		//!\param x X coordinate
		//!\param y Y coordinate
		GuiColor getPixel(int x, int y);
		//!Sets the pixel color at the specified coordinates of the image
		//!\param x X coordinate
		//!\param y Y coordinate
		//!\param color Pixel color
		void setPixel(int x, int y, GuiColor color);
		//!Directly modifies the image data to create a color-striped effect
		//!Alters the RGB values by the specified amount
		//!\param s Amount to increment/decrement the RGB values in the image
		void colorStripe(int s);
		//!Sets a stripe effect on the image, overlaying alpha blended rectangles
		//!Does not alter the image data
		//!\param s Alpha amount to draw over the image
		void setStripe(int s);
	protected:
		IMAGE imgType; //!< Type of image data (TEXTURE, COLOR, DATA)
		uint8_t * image; //!< Poiner to image data. May be shared with GuiImageData data
		float imageangle; //!< Angle to draw the image
		int tile; //!< Number of times to draw (tile) the image horizontally
		int stripe; //!< Alpha value (0-255) to apply a stripe effect to the texture
};
