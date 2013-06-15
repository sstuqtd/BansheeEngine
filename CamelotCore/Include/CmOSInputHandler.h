#pragma once

#include "CmPrerequisites.h"
#include <boost/signal.hpp>
#include "CmInt2.h"	

namespace CamelotFramework
{
	/**
		* @brief	Represents a specific way of acquiring OS input. InputManager (which provides a higher level input)
		* 			must have at least one OSInputHandler attached. Attach events handler to the provided signals to handle input.
		* 			
		* @note		Unlike RawInputHandler this class receives input from the operating system, and is used for receiving
		* 			text input, cursor position and similar.
		*/
	class CM_EXPORT OSInputHandler
	{
	public:
		OSInputHandler();
		virtual ~OSInputHandler();

		boost::signal<void(UINT32)> onCharInput;
		boost::signal<void(const Int2&)> onMouseMoved;

		/**
			* @brief	Called every frame by InputManager. Capture input here if needed.
			*/
		virtual void update();

		/**
			* @brief	Called by InputManager whenever window in focus changes.
			*/
		virtual void inputWindowChanged(const RenderWindow& win) {}

	private:
		CM_MUTEX(mOSInputMutex);
		Int2 mLastMousePos;
		Int2 mMousePosition;
		WString mInputString;

		boost::signals::connection mCharInputConn;
		boost::signals::connection mMouseMovedConn;

		/**
		 * @brief	Called from the message loop.
		 */
		void charInput(UINT32 character);

		/**
		 * @brief	Called from the message loop.
		 */
		void mouseMoved(const Int2& mousePos);
	};
}