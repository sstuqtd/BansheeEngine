#include "BsEditorWidget.h"
#include "BsGUIManager.h"
#include "BsGUIWidget.h"
#include "BsGUITexture.h"
#include "BsGUISkin.h"
#include "BsGUILayout.h"
#include "BsEngineGUI.h"
#include "BsGUIArea.h"

using namespace CamelotFramework;
using namespace BansheeEngine;

namespace BansheeEditor
{
	EditorWidget::EditorWidget(const WString& name)
		:mName(name), mParentWidget(nullptr), mContent(nullptr)
	{
		
	}

	EditorWidget::~EditorWidget()
	{

	}

	void EditorWidget::destroy(EditorWidget* widget)
	{
		cm_delete(widget);
	}

	void EditorWidget::_setPosition(INT32 x, INT32 y)
	{
		if(mContent == nullptr)
			return;

		mContent->setPosition(x, y);
	}

	void EditorWidget::_setSize(UINT32 width, UINT32 height)
	{
		if(mContent == nullptr)
			return;

		mContent->setSize(width, height);
	}

	void EditorWidget::_changeParent(BS::GUIWidget& widget)
	{
		if(mParentWidget != &widget) 
		{
			if(mParentWidget == nullptr)
				mContent = GUIArea::create(widget, 0, 0, 0, 0, 10000);
			else
				mContent->changeParentWidget(widget);

			mParentWidget = &widget;
		}
	}

	void EditorWidget::_disable()
	{
		mContent->disable();
	}

	void EditorWidget::_enable()
	{
		mContent->enable();
	}
}