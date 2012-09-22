/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2011 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/
#include "CmHardwarePixelBuffer.h"
#include "CmTexture.h"
#include "CmTextureST.h"
#include "CmTextureData.h"
#include "CmDataStream.h"
#include "CmException.h"
#include "CmDebug.h"

namespace CamelotEngine {
	//--------------------------------------------------------------------------
    Texture::Texture()
        : // init defaults; can be overridden before load()
            mHeight(512),
            mWidth(512),
            mDepth(1),
			mNumRequestedMipmaps(0),
            mNumMipmaps(0),
			mMipmapsHardwareGenerated(false),
            mGamma(1.0f),
			mHwGamma(false),
			mFSAA(0),
            mTextureType(TEX_TYPE_2D),            
            mFormat(PF_UNKNOWN),
            mUsage(TU_DEFAULT),
            mSrcFormat(PF_UNKNOWN),
            mSrcWidth(0),
            mSrcHeight(0), 
            mSrcDepth(0),
            mDesiredFormat(PF_UNKNOWN),
            mDesiredIntegerBitDepth(0),
            mDesiredFloatBitDepth(0),
            mInternalResourcesCreated(false)
    {
        
    }
    //--------------------------------------------------------------------------
    void Texture::setFormat(PixelFormat pf)
    {
        mFormat = pf;
        mDesiredFormat = pf;
        mSrcFormat = pf;
    }
    //--------------------------------------------------------------------------
    bool Texture::hasAlpha(void) const
    {
        return PixelUtil::hasAlpha(mFormat);
    }
    //--------------------------------------------------------------------------
    void Texture::setDesiredIntegerBitDepth(UINT16 bits)
    {
        mDesiredIntegerBitDepth = bits;
    }
    //--------------------------------------------------------------------------
    UINT16 Texture::getDesiredIntegerBitDepth(void) const
    {
        return mDesiredIntegerBitDepth;
    }
    //--------------------------------------------------------------------------
    void Texture::setDesiredFloatBitDepth(UINT16 bits)
    {
        mDesiredFloatBitDepth = bits;
    }
    //--------------------------------------------------------------------------
    UINT16 Texture::getDesiredFloatBitDepth(void) const
    {
        return mDesiredFloatBitDepth;
    }
    //--------------------------------------------------------------------------
    void Texture::setDesiredBitDepths(UINT16 integerBits, UINT16 floatBits)
    {
        mDesiredIntegerBitDepth = integerBits;
        mDesiredFloatBitDepth = floatBits;
    }
    //--------------------------------------------------------------------------
	size_t Texture::calculateSize(void) const
	{
        return getNumFaces() * PixelUtil::getMemorySize(mWidth, mHeight, mDepth, mFormat);
	}
	//--------------------------------------------------------------------------
	size_t Texture::getNumFaces(void) const
	{
		return getTextureType() == TEX_TYPE_CUBE_MAP ? 6 : 1;
	}
	//-----------------------------------------------------------------------------
	void Texture::createInternalResources(void)
	{
		if (!mInternalResourcesCreated)
		{
			createInternalResourcesImpl();
			mInternalResourcesCreated = true;
		}
	}
	//-----------------------------------------------------------------------------
	void Texture::freeInternalResources(void)
	{
		if (mInternalResourcesCreated)
		{
			freeInternalResourcesImpl();
			mInternalResourcesCreated = false;
		}
	}
	//-----------------------------------------------------------------------------
	void Texture::loadFromTextureData(const vector<TextureDataPtr>::type& textureData)
	{
		if(textureData.size() < 1)
			CM_EXCEPT(InvalidParametersException, "Cannot load empty vector of images");

		if(getTextureType() == TEX_TYPE_CUBE_MAP)
		{
			if(textureData.size() != 6)
				CM_EXCEPT(InvalidParametersException, "Cube map textures require six faces.");
		}
		else
		{
			if(textureData.size() > 1)
			{
				gDebug().log("Non-cube textures can only have one face. Loading only first face from the provided array.", "D3D9RenderSystem");
			}
		}

		// Set desired texture size and properties from images[0]
		mSrcWidth = mWidth = textureData[0]->getWidth();
		mSrcHeight = mHeight = textureData[0]->getHeight();
		mSrcDepth = mDepth = textureData[0]->getDepth();

		// Get source image format and adjust if required
		mSrcFormat = textureData[0]->getFormat();

		if (mDesiredFormat != PF_UNKNOWN)
		{
			// If have desired format, use it
			mFormat = mDesiredFormat;
		}
		else
		{
			// Get the format according with desired bit depth
			mFormat = PixelUtil::getFormatForBitDepths(mSrcFormat, mDesiredIntegerBitDepth, mDesiredFloatBitDepth);
		}

		// The custom mipmaps in the image have priority over everything
		size_t imageMips = textureData[0]->getNumMipmaps();

		if(imageMips > 0)
		{
			mNumMipmaps = mNumRequestedMipmaps = textureData[0]->getNumMipmaps();
			// Disable flag for auto mip generation
			mUsage &= ~TU_AUTOMIPMAP;
		}

		// Create the texture
		createInternalResources();
		// Check if we're loading one image with multiple faces
		// or a vector of images representing the faces
		size_t faces;
		if(textureData.size() > 1)
			faces = textureData.size();
		else
			faces = 1;

		// Check wether number of faces in images exceeds number of faces
		// in this texture. If so, clamp it.
		if(faces > getNumFaces())
			faces = getNumFaces();

		// Main loading loop
		// imageMips == 0 if the image has no custom mipmaps, otherwise contains the number of custom mips
		for(size_t mip = 0; mip<=imageMips; ++mip)
		{
			for(size_t i = 0; i < faces; ++i)
			{
				PixelData src = textureData[i]->getPixels(mip);

				// Sets to treated format in case is difference
				src.format = mSrcFormat;

				if(mGamma != 1.0f) {
					// Apply gamma correction
					// Do not overwrite original image but do gamma correction in temporary buffer
					MemoryDataStreamPtr buf(new MemoryDataStream(
						PixelUtil::getMemorySize(
						src.getWidth(), src.getHeight(), src.getDepth(), src.format)));

					PixelData corrected = PixelData(src.getWidth(), src.getHeight(), src.getDepth(), src.format, buf->getPtr());
					PixelUtil::bulkPixelConversion(src, corrected);

					PixelUtil::applyGamma(static_cast<UINT8*>(corrected.data), mGamma, corrected.getConsecutiveSize(), 
						static_cast<UINT8>(PixelUtil::getNumElemBits(src.format)));

					// Destination: entire texture. blitFromMemory does the scaling to
					// a power of two for us when needed
					getBuffer(i, mip)->blitFromMemory(corrected);
				}
				else 
				{
					// Destination: entire texture. blitFromMemory does the scaling to
					// a power of two for us when needed
					getBuffer(i, mip)->blitFromMemory(src);
				}

			}
		}
		// Update size (the final size, not including temp space)
		mSize = getNumFaces() * PixelUtil::getMemorySize(mWidth, mHeight, mDepth, mFormat);
	}
	//-----------------------------------------------------------------------------
	void Texture::unloadImpl(void)
	{
		freeInternalResources();
	}
    //-----------------------------------------------------------------------------   
    void Texture::copyToTexture( TexturePtr& target )
    {
        if(target->getNumFaces() != getNumFaces())
        {
            CM_EXCEPT(InvalidParametersException, 
                "Texture types must match");
        }
        size_t numMips = std::min(getNumMipmaps(), target->getNumMipmaps());
        if((mUsage & TU_AUTOMIPMAP) || (target->getUsage()&TU_AUTOMIPMAP))
            numMips = 0;
        for(unsigned int face=0; face<getNumFaces(); face++)
        {
            for(unsigned int mip=0; mip<=numMips; mip++)
            {
                target->getBuffer(face, mip)->blit(getBuffer(face, mip));
            }
        }
    }

	/************************************************************************/
	/* 								SERIALIZATION                      		*/
	/************************************************************************/

	SerializableType* Texture::getSerializable() const
	{
		static TextureST serializableType;
		return &serializableType;
	}

	Texture* Texture::newObject()
	{
		CM_EXCEPT(InternalErrorException, "Cannot instantiate abstract class!");
	}
}