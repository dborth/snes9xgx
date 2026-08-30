/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiImage.h
 ***************************************************************************/
#pragma once

enum class IMAGE {
	TEXTURE,
	COLOR
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
		//!Sets up a new image from the texture data specified
		//!\param t Texture data
		//!\param w Image width
		//!\param h Image height
		GuiImage(uint8_t * tex, int w, int h);
		//!\overload
		//!Creates an image with the specified color
		//!\param w Image width
		//!\param h Image height
		//!\param c Image color
		GuiImage(int w, int h, PixelColor c);
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
		//!Sets up a new image using the GuiImageData object specified
		//!\param img Pointer to GuiImageData object
		void setImage(GuiImageData * img);
		//!\overload
		//!\param img Pointer to (generic row-major RGBA8) image data
		//!\param w Width
		//!\param h Height
		void setImage(uint8_t * img, int w, int h);
		//!\overload
		//!\param tex Pointer to platform-native texture
		//!\param w Width
		//!\param h Height
		void setTexture(uint8_t * tex, int w, int h);
		//!Sets a stripe effect on the image, overlaying alpha blended rectangles
		//!Does not alter the image data
		//!\param s Alpha amount to draw over the image
		void setStripe(int s);
	protected:
		IMAGE imgType; //!< Type of image data (TEXTURE, COLOR)
		void * texture; //!< Attached platform-native texture
		bool ownsTexture; //!< Whether this object created `texture` itself (TEXTURE/COLOR) or borrowed it from a GuiImageData
		float imageangle; //!< Angle to draw the image
		int tile; //!< Number of times to draw (tile) the image horizontally
		int stripe; //!< Alpha value (0-255) to apply a stripe effect to the texture
		PixelColor baseColor; //!< Stored color for IMAGE::COLOR types
};
