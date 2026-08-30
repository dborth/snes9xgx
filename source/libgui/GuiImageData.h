/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * GuiImageData.h
 ***************************************************************************/
#pragma once

//!Decodes compressed image data (PNG) into a platform-native texture created
//!from it. Currently designed for use only with PNG files.
class GuiImageData
{
	public:
		//!Constructor
		//!Converts the PNG format image data to a platform-native texture
		//!\param i Source image data (PNG)
		//!\param d Destination texture buffer
		//!\param w Max image width (0 = not set)
		//!\param h Max image height (0 = not set)
		GuiImageData(const uint8_t * i, int w=0, int h=0);
		//!Constructor
		//!Converts the PNG format image data to a platform-native texture
		//!\param i Source image data (PNG)
		//!\param d Destination texture buffer
		//!\param w Max image width (0 = not set)
		//!\param h Max image height (0 = not set)
		GuiImageData(const uint8_t * i, uint8_t * dst, int maxw=0, int maxh=0);
		//!Constructor
		//!Populates a an image directly with a platform-native texture
		//!\param t Destination texture buffer
		//!\param w Image width
		//!\param h Image height
		GuiImageData(void * t, int w, int h);
		//!Destructor
		~GuiImageData();
		//!Gets the attached platform-native texture
		//!\return opaque texture handle
		void * getTexture() { return texture; }
		//!Gets the image width
		//!\return image width
		int getWidth() { return width; }
		//!Gets the image height
		//!\return image height
		int getHeight() { return height; }
	protected:
		void * texture; //!< Attached platform-native texture, owned by this object
		int height; //!< Height of image
		int width; //!< Width of image
	private:
		// !Decodes a PNG buffer into a platform-native texture
		void decodeImage(const uint8_t * pngData, int * width, int * height, int maxw = 0, int maxh = 0);
};
