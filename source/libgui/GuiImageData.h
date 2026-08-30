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
		GuiImageData();
		//!Constructor
		//!Converts the PNG format image data to a platform-native texture,
		//!allocated and owned by this object.
		//!\param i Source image data (PNG)
		//!\param w Max image width (0 = not set)
		//!\param h Max image height (0 = not set)
		GuiImageData(const uint8_t * i, int w=0, int h=0);
		//!Constructor
		//!Converts the PNG format image data to a platform-native texture,
		//!decoded directly into a caller-supplied buffer.
		//!\param i Source image data (PNG)
		//!\param dst Destination texture buffer, owned by the caller
		//!\param maxw Max image width (0 = not set)
		//!\param maxh Max image height (0 = not set)
		GuiImageData(const uint8_t * i, uint8_t * dst, int maxw=0, int maxh=0);
		//!Constructor
		//!Populates an image directly with an already-created platform-native
		//!texture
		//!\param t Platform-native texture
		//!\param w Image width
		//!\param h Image height
		//!\param takeOwnership If true (default), this object will destroy
		//!\c t when it is destroyed or replaced.
		GuiImageData(void * t, int w, int h, bool takeOwnership = true);
		//!Destructor
		~GuiImageData();
		//!Decodes new PNG data into this object's own texture, reusing the
		//!existing allocation whenever it's already large enough for the new
		//!image instead of freeing and reallocating.
		//!\param pngData Source image data (PNG)
		//!\param maxw Max image width (0 = not set)
		//!\param maxh Max image height (0 = not set)
		//!\return true on success
		bool reload(const uint8_t * pngData, int maxw = 0, int maxh = 0);
		//!Gets the attached platform-native texture
		//!\return opaque texture handle
		void * getTexture() { return texture; }
		//!Gets the image width
		//!\return image width
		int getWidth() { return width; }
		//!Gets the image height
		//!\return image height
		int getHeight() { return height; }
		//!Sets the scratch buffer that every decode (the PNG-decoding
		//!constructors, and reload()) will stage its raw, uncompressed pixel
		//!rows into while it runs.
		//!\param buffer Scratch buffer, owned and sized by the caller
		//!\param size Size of buffer, in bytes
		static void setDecodeScratch(void * buffer, unsigned int size);
	protected:
		void * texture; //!< Attached platform-native texture
		int height; //!< Height of image
		int width; //!< Width of image
	private:
		bool ownsTexture; //!< Whether this object may destroy/replace texture
		int capWidth; //!< Width texture was allocated to hold, if ownsTexture (0 otherwise)
		int capHeight; //!< Height texture was allocated to hold, if ownsTexture (0 otherwise)
		//!Decodes a PNG buffer, reusing the existing owned texture if it's
		//!already large enough, otherwise (re)allocating one.
		bool decodeImage(const uint8_t * pngData, int * width, int * height, int maxw, int maxh);
};
