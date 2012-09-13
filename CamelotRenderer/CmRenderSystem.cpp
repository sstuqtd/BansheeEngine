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
// RenderSystem implementation
// Note that most of this class is abstract since
//  we cannot know how to implement the behaviour without
//  being aware of the 3D API. However there are a few
//  simple functions which can have a base implementation

#include "CmRenderSystem.h"

#include "CmViewport.h"
#include "CmException.h"
#include "CmRenderTarget.h"
#include "CmRenderWindow.h"
#include "CmHardwarePixelBuffer.h"
#include "CmHardwareOcclusionQuery.h"

namespace CamelotEngine {

    static const TexturePtr sNullTexPtr;

    //-----------------------------------------------------------------------
    RenderSystem::RenderSystem()
        : mActiveRenderTarget(0)
        , mTextureManager(0)
        , mActiveViewport(0)
        // This means CULL clockwise vertices, i.e. front of poly is counter-clockwise
        // This makes it the same as OpenGL and other right-handed systems
        , mCullingMode(CULL_CLOCKWISE)
        , mVSync(true)
		, mVSyncInterval(1)
		, mWBuffer(false)
        , mInvertVertexWinding(false)
        , mDisabledTexUnitsFrom(0)
        , mCurrentPassIterationCount(0)
		, mDerivedDepthBias(false)
        , mVertexProgramBound(false)
		, mGeometryProgramBound(false)
        , mFragmentProgramBound(false)
		, mClipPlanesDirty(true)
		, mRealCapabilities(0)
		, mCurrentCapabilities(0)
		, mUseCustomCapabilities(false)
		, mTexProjRelative(false)
		, mTexProjRelativeOrigin(Vector3::ZERO)
    {
    }

    //-----------------------------------------------------------------------
    RenderSystem::~RenderSystem()
    {
        shutdown();
		delete mRealCapabilities;
		mRealCapabilities = 0;
		// Current capabilities managed externally
		mCurrentCapabilities = 0;
    }
    //-----------------------------------------------------------------------
    void RenderSystem::_initRenderTargets(void)
    {

    }
    //-----------------------------------------------------------------------
    void RenderSystem::_updateAllRenderTargets(bool swapBuffers)
    {
        // Update all in order of priority
        // This ensures render-to-texture targets get updated before render windows
		RenderTargetPriorityMap::iterator itarg, itargend;
		itargend = mPrioritisedRenderTargets.end();
		for( itarg = mPrioritisedRenderTargets.begin(); itarg != itargend; ++itarg )
		{
			if( itarg->second->isActive())
				itarg->second->update(swapBuffers);
		}
    }
    //-----------------------------------------------------------------------
    void RenderSystem::_swapAllRenderTargetBuffers(bool waitForVSync)
    {
        // Update all in order of priority
        // This ensures render-to-texture targets get updated before render windows
		RenderTargetPriorityMap::iterator itarg, itargend;
		itargend = mPrioritisedRenderTargets.end();
		for( itarg = mPrioritisedRenderTargets.begin(); itarg != itargend; ++itarg )
		{
			if( itarg->second->isActive())
				itarg->second->swapBuffers(waitForVSync);
		}
    }
    //-----------------------------------------------------------------------
    RenderWindow* RenderSystem::_initialise(bool autoCreateWindow, const String& windowTitle)
    {
        // Have I been registered by call to Root::setRenderSystem?
		/** Don't do this anymore, just allow via Root
        RenderSystem* regPtr = Root::getSingleton().getRenderSystem();
        if (!regPtr || regPtr != this)
            // Register self - library user has come to me direct
            Root::getSingleton().setRenderSystem(this);
		*/


        // Subclasses should take it from here
        // They should ALL call this superclass method from
        //   their own initialise() implementations.
        
        mVertexProgramBound = false;
		mGeometryProgramBound = false;
        mFragmentProgramBound = false;

        return 0;
    }

	//---------------------------------------------------------------------------------------------
	void RenderSystem::useCustomRenderSystemCapabilities(RenderSystemCapabilities* capabilities)
	{
    if (mRealCapabilities != 0)
    {
      OGRE_EXCEPT(Exception::ERR_INTERNAL_ERROR, 
          "Custom render capabilities must be set before the RenderSystem is initialised.",
          "RenderSystem::useCustomRenderSystemCapabilities");
    }

		mCurrentCapabilities = capabilities;
		mUseCustomCapabilities = true;
	}

	//---------------------------------------------------------------------------------------------
	bool RenderSystem::_createRenderWindows(const RenderWindowDescriptionList& renderWindowDescriptions, 
		RenderWindowList& createdWindows)
	{
		unsigned int fullscreenWindowsCount = 0;

		// Grab some information and avoid duplicate render windows.
		for (unsigned int nWindow=0; nWindow < renderWindowDescriptions.size(); ++nWindow)
		{
			const RenderWindowDescription* curDesc = &renderWindowDescriptions[nWindow];

			// Count full screen windows.
			if (curDesc->useFullScreen)			
				fullscreenWindowsCount++;	

			bool renderWindowFound = false;

			if (mRenderTargets.find(curDesc->name) != mRenderTargets.end())
				renderWindowFound = true;
			else
			{
				for (unsigned int nSecWindow = nWindow + 1 ; nSecWindow < renderWindowDescriptions.size(); ++nSecWindow)
				{
					if (curDesc->name == renderWindowDescriptions[nSecWindow].name)
					{
						renderWindowFound = true;
						break;
					}					
				}
			}

			// Make sure we don't already have a render target of the 
			// same name as the one supplied
			if(renderWindowFound)
			{
				String msg;

				msg = "A render target of the same name '" + String(curDesc->name) + "' already "
					"exists.  You cannot create a new window with this name.";
				OGRE_EXCEPT( Exception::ERR_INTERNAL_ERROR, msg, "RenderSystem::createRenderWindow" );
			}
		}
		
		// Case we have to create some full screen rendering windows.
		if (fullscreenWindowsCount > 0)
		{
			// Can not mix full screen and windowed rendering windows.
			if (fullscreenWindowsCount != renderWindowDescriptions.size())
			{
				OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
					"Can not create mix of full screen and windowed rendering windows",
					"RenderSystem::createRenderWindows");
			}					
		}

		return true;
	}

    //---------------------------------------------------------------------------------------------
    void RenderSystem::destroyRenderWindow(const String& name)
    {
        destroyRenderTarget(name);
    }
    //---------------------------------------------------------------------------------------------
    void RenderSystem::destroyRenderTexture(const String& name)
    {
        destroyRenderTarget(name);
    }
    //---------------------------------------------------------------------------------------------
    void RenderSystem::destroyRenderTarget(const String& name)
    {
        RenderTarget* rt = detachRenderTarget(name);
        delete rt;
    }
    //---------------------------------------------------------------------------------------------
    void RenderSystem::attachRenderTarget( RenderTarget &target )
    {
		assert( target.getPriority() < OGRE_NUM_RENDERTARGET_GROUPS );

        mRenderTargets.insert( RenderTargetMap::value_type( target.getName(), &target ) );
        mPrioritisedRenderTargets.insert(
            RenderTargetPriorityMap::value_type(target.getPriority(), &target ));
    }

    //---------------------------------------------------------------------------------------------
    RenderTarget * RenderSystem::getRenderTarget( const String &name )
    {
        RenderTargetMap::iterator it = mRenderTargets.find( name );
        RenderTarget *ret = NULL;

        if( it != mRenderTargets.end() )
        {
            ret = it->second;
        }

        return ret;
    }

    //---------------------------------------------------------------------------------------------
    RenderTarget * RenderSystem::detachRenderTarget( const String &name )
    {
        RenderTargetMap::iterator it = mRenderTargets.find( name );
        RenderTarget *ret = NULL;

        if( it != mRenderTargets.end() )
        {
            ret = it->second;
			
			/* Remove the render target from the priority groups. */
            RenderTargetPriorityMap::iterator itarg, itargend;
            itargend = mPrioritisedRenderTargets.end();
			for( itarg = mPrioritisedRenderTargets.begin(); itarg != itargend; ++itarg )
            {
				if( itarg->second == ret ) {
					mPrioritisedRenderTargets.erase( itarg );
					break;
				}
            }

            mRenderTargets.erase( it );
        }
        /// If detached render target is the active render target, reset active render target
        if(ret == mActiveRenderTarget)
            mActiveRenderTarget = 0;

        return ret;
    }
    //-----------------------------------------------------------------------
    Viewport* RenderSystem::_getViewport(void)
    {
        return mActiveViewport;
    }
    //-----------------------------------------------------------------------
    void RenderSystem::_setTextureUnitSettings(size_t texUnit, const TexturePtr& tex, TextureState& tl)
    {
        // This method is only ever called to set a texture unit to valid details
        // The method _disableTextureUnit is called to turn a unit off

		// Vertex texture binding?
		if (mCurrentCapabilities->hasCapability(RSC_VERTEX_TEXTURE_FETCH) &&
			!mCurrentCapabilities->getVertexTextureUnitsShared())
		{
			if (tl.getBindingType() == TextureState::BT_VERTEX)
			{
				// Bind vertex texture
				_setVertexTexture(texUnit, tex);
				// bind nothing to fragment unit (hardware isn't shared but fragment
				// unit can't be using the same index
				_setTexture(texUnit, true, sNullTexPtr);
			}
			else
			{
				// vice versa
				_setVertexTexture(texUnit, sNullTexPtr);
				_setTexture(texUnit, true, tex);
			}
		}
		else
		{
			// Shared vertex / fragment textures or no vertex texture support
			// Bind texture (may be blank)
			_setTexture(texUnit, true, tex);
		}

        // Set texture layer filtering
        _setTextureUnitFiltering(texUnit, 
            tl.getTextureFiltering(FT_MIN), 
            tl.getTextureFiltering(FT_MAG), 
            tl.getTextureFiltering(FT_MIP));

        // Set texture layer filtering
        _setTextureLayerAnisotropy(texUnit, tl.getTextureAnisotropy());

		// Set mipmap biasing
		_setTextureMipmapBias(texUnit, tl.getTextureMipmapBias());

        // Texture addressing mode
        const TextureState::UVWAddressingMode& uvw = tl.getTextureAddressingMode();
        _setTextureAddressingMode(texUnit, uvw);
    }
	//-----------------------------------------------------------------------
	void RenderSystem::_setVertexTexture(size_t unit, const TexturePtr& tex)
	{
		OGRE_EXCEPT(Exception::ERR_NOT_IMPLEMENTED, 
			"This rendersystem does not support separate vertex texture samplers, "
			"you should use the regular texture samplers which are shared between "
			"the vertex and fragment units.", 
			"RenderSystem::_setVertexTexture");
	}
    //-----------------------------------------------------------------------
    void RenderSystem::_disableTextureUnit(size_t texUnit)
    {
        _setTexture(texUnit, false, sNullTexPtr);
    }
    //---------------------------------------------------------------------
    void RenderSystem::_disableTextureUnitsFrom(size_t texUnit)
    {
        size_t disableTo = CM_MAX_TEXTURE_LAYERS;
        if (disableTo > mDisabledTexUnitsFrom)
            disableTo = mDisabledTexUnitsFrom;
        mDisabledTexUnitsFrom = texUnit;
        for (size_t i = texUnit; i < disableTo; ++i)
        {
            _disableTextureUnit(i);
        }
    }
    //-----------------------------------------------------------------------
    void RenderSystem::_setTextureUnitFiltering(size_t unit, FilterOptions minFilter,
            FilterOptions magFilter, FilterOptions mipFilter)
    {
        _setTextureUnitFiltering(unit, FT_MIN, minFilter);
        _setTextureUnitFiltering(unit, FT_MAG, magFilter);
        _setTextureUnitFiltering(unit, FT_MIP, mipFilter);
    }
    //-----------------------------------------------------------------------
    CullingMode RenderSystem::_getCullingMode(void) const
    {
        return mCullingMode;
    }
    //-----------------------------------------------------------------------
    bool RenderSystem::getWaitForVerticalBlank(void) const
    {
        return mVSync;
    }
    //-----------------------------------------------------------------------
    void RenderSystem::setWaitForVerticalBlank(bool enabled)
    {
        mVSync = enabled;
    }
    bool RenderSystem::getWBufferEnabled(void) const
    {
        return mWBuffer;
    }
    //-----------------------------------------------------------------------
    void RenderSystem::setWBufferEnabled(bool enabled)
    {
        mWBuffer = enabled;
    }
    //-----------------------------------------------------------------------
    void RenderSystem::shutdown(void)
    {
		// Remove occlusion queries
		for (HardwareOcclusionQueryList::iterator i = mHwOcclusionQueries.begin();
			i != mHwOcclusionQueries.end(); ++i)
		{
			delete *i;
		}
		mHwOcclusionQueries.clear();

        // Remove all the render targets.
		// (destroy primary target last since others may depend on it)
		RenderTarget* primary = 0;
		for (RenderTargetMap::iterator it = mRenderTargets.begin(); it != mRenderTargets.end(); ++it)
		{
			if (!primary && it->second->isPrimary())
				primary = it->second;
			else
				delete it->second;
		}
		delete primary;
		mRenderTargets.clear();

		mPrioritisedRenderTargets.clear();
    }
    //-----------------------------------------------------------------------
    void RenderSystem::_beginGeometryCount(void)
    {
        mBatchCount = mFaceCount = mVertexCount = 0;

    }
    //-----------------------------------------------------------------------
    unsigned int RenderSystem::_getFaceCount(void) const
    {
        return static_cast< unsigned int >( mFaceCount );
    }
    //-----------------------------------------------------------------------
    unsigned int RenderSystem::_getBatchCount(void) const
    {
        return static_cast< unsigned int >( mBatchCount );
    }
    //-----------------------------------------------------------------------
    unsigned int RenderSystem::_getVertexCount(void) const
    {
        return static_cast< unsigned int >( mVertexCount );
    }
    //-----------------------------------------------------------------------
	void RenderSystem::convertColourValue(const Color& colour, UINT32* pDest)
	{
		*pDest = VertexElement::convertColourValue(colour, getColourVertexElementType());

	}
    //-----------------------------------------------------------------------
    void RenderSystem::_setWorldMatrices(const Matrix4* m, unsigned short count)
    {
        // Do nothing with these matrices here, it never used for now,
		// derived class should take care with them if required.

        // Set hardware matrix to nothing
        _setWorldMatrix(Matrix4::IDENTITY);
    }
    //-----------------------------------------------------------------------
    void RenderSystem::_render(const RenderOperation& op)
    {
        // Update stats
        size_t val;

        if (op.useIndexes)
            val = op.indexData->indexCount;
        else
            val = op.vertexData->vertexCount;

        // account for a pass having multiple iterations
        if (mCurrentPassIterationCount > 1)
            val *= mCurrentPassIterationCount;
		mCurrentPassIterationNum = 0;

        switch(op.operationType)
        {
		case RenderOperation::OT_TRIANGLE_LIST:
            mFaceCount += val / 3;
            break;
        case RenderOperation::OT_TRIANGLE_STRIP:
        case RenderOperation::OT_TRIANGLE_FAN:
            mFaceCount += val - 2;
            break;
	    case RenderOperation::OT_POINT_LIST:
	    case RenderOperation::OT_LINE_LIST:
	    case RenderOperation::OT_LINE_STRIP:
	        break;
	    }

        mVertexCount += op.vertexData->vertexCount;
        mBatchCount += mCurrentPassIterationCount;

		// sort out clip planes
		// have to do it here in case of matrix issues
		if (mClipPlanesDirty)
		{
			setClipPlanesImpl(mClipPlanes);
			mClipPlanesDirty = false;
		}
    }
    //-----------------------------------------------------------------------
    void RenderSystem::setInvertVertexWinding(bool invert)
    {
        mInvertVertexWinding = invert;
    }
	//-----------------------------------------------------------------------
	bool RenderSystem::getInvertVertexWinding(void) const
	{
		return mInvertVertexWinding;
	}
	//---------------------------------------------------------------------
	void RenderSystem::addClipPlane (const Plane &p)
	{
		mClipPlanes.push_back(p);
		mClipPlanesDirty = true;
	}
	//---------------------------------------------------------------------
	void RenderSystem::addClipPlane (float A, float B, float C, float D)
	{
		addClipPlane(Plane(A, B, C, D));
	}
	//---------------------------------------------------------------------
	void RenderSystem::setClipPlanes(const PlaneList& clipPlanes)
	{
		if (clipPlanes != mClipPlanes)
		{
			mClipPlanes = clipPlanes;
			mClipPlanesDirty = true;
		}
	}
	//---------------------------------------------------------------------
	void RenderSystem::resetClipPlanes()
	{
		if (!mClipPlanes.empty())
		{
			mClipPlanes.clear();
			mClipPlanesDirty = true;
		}
	}
    //-----------------------------------------------------------------------
    void RenderSystem::_notifyCameraRemoved(const Camera* cam)
    {
		// TODO PORT - Not used in the port. Should probably be removed
    }

	//---------------------------------------------------------------------
    bool RenderSystem::updatePassIterationRenderState(void)
    {
        if (mCurrentPassIterationCount <= 1)
            return false;

        --mCurrentPassIterationCount;
		++mCurrentPassIterationNum;
        if (mActiveVertexGpuProgramParameters != nullptr)
        {
            mActiveVertexGpuProgramParameters->incPassIterationNumber();
            bindGpuProgramPassIterationParameters(GPT_VERTEX_PROGRAM);
        }
        if (mActiveGeometryGpuProgramParameters != nullptr)
        {
            mActiveGeometryGpuProgramParameters->incPassIterationNumber();
            bindGpuProgramPassIterationParameters(GPT_GEOMETRY_PROGRAM);
        }
        if (mActiveFragmentGpuProgramParameters != nullptr)
        {
            mActiveFragmentGpuProgramParameters->incPassIterationNumber();
            bindGpuProgramPassIterationParameters(GPT_FRAGMENT_PROGRAM);
        }
        return true;
    }

	//-----------------------------------------------------------------------
	void RenderSystem::addListener(Listener* l)
	{
		mEventListeners.push_back(l);
	}
	//-----------------------------------------------------------------------
	void RenderSystem::removeListener(Listener* l)
	{
		mEventListeners.remove(l);
	}
	//-----------------------------------------------------------------------
	void RenderSystem::fireEvent(const String& name, const NameValuePairList* params)
	{
		for(ListenerList::iterator i = mEventListeners.begin(); 
			i != mEventListeners.end(); ++i)
		{
			(*i)->eventOccurred(name, params);
		}
	}
	//-----------------------------------------------------------------------
	void RenderSystem::destroyHardwareOcclusionQuery( HardwareOcclusionQuery *hq)
	{
		HardwareOcclusionQueryList::iterator i =
			std::find(mHwOcclusionQueries.begin(), mHwOcclusionQueries.end(), hq);
		if (i != mHwOcclusionQueries.end())
		{
			mHwOcclusionQueries.erase(i);
			delete hq;
		}
	}
	//-----------------------------------------------------------------------
	void RenderSystem::bindGpuProgram(GpuProgram* prg)
	{
	    switch(prg->getType())
	    {
        case GPT_VERTEX_PROGRAM:
			// mark clip planes dirty if changed (programmable can change space)
			if (!mVertexProgramBound && !mClipPlanes.empty())
				mClipPlanesDirty = true;

            mVertexProgramBound = true;
	        break;
        case GPT_GEOMETRY_PROGRAM:
			mGeometryProgramBound = true;
			break;
        case GPT_FRAGMENT_PROGRAM:
            mFragmentProgramBound = true;
	        break;
	    }
	}
	//-----------------------------------------------------------------------
	void RenderSystem::unbindGpuProgram(GpuProgramType gptype)
	{
	    switch(gptype)
	    {
        case GPT_VERTEX_PROGRAM:
			// mark clip planes dirty if changed (programmable can change space)
			if (mVertexProgramBound && !mClipPlanes.empty())
				mClipPlanesDirty = true;
            mVertexProgramBound = false;
	        break;
        case GPT_GEOMETRY_PROGRAM:
			mGeometryProgramBound = false;
			break;
        case GPT_FRAGMENT_PROGRAM:
            mFragmentProgramBound = false;
	        break;
	    }
	}
	//-----------------------------------------------------------------------
	bool RenderSystem::isGpuProgramBound(GpuProgramType gptype)
	{
	    switch(gptype)
	    {
        case GPT_VERTEX_PROGRAM:
            return mVertexProgramBound;
        case GPT_GEOMETRY_PROGRAM:
            return mGeometryProgramBound;
        case GPT_FRAGMENT_PROGRAM:
            return mFragmentProgramBound;
	    }
        // Make compiler happy
        return false;
	}
	//---------------------------------------------------------------------
	void RenderSystem::_setTextureProjectionRelativeTo(bool enabled, const Vector3& pos)
	{
		mTexProjRelative = enabled;
		mTexProjRelativeOrigin = pos;

	}
	//---------------------------------------------------------------------
	RenderSystem::RenderSystemContext* RenderSystem::_pauseFrame(void)
	{
		_endFrame();
		return new RenderSystem::RenderSystemContext;
	}
	//---------------------------------------------------------------------
	void RenderSystem::_resumeFrame(RenderSystemContext* context)
	{
		_beginFrame();
		delete context;
	}

}
