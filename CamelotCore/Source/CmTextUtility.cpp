#include "CmTextUtility.h"
#include "CmFont.h"
#include "CmVector2.h"
#include "CmDebug.h"

namespace CamelotFramework
{
	const int SPACE_CHAR = 32;

	TextUtility::TextWord::TextWord(bool spacer)
		:mWidth(0), mHeight(0), mSpacer(spacer), mSpaceWidth(0)
	{ }

	void TextUtility::TextWord::addChar(const CHAR_DESC& desc)
	{
		mChars.push_back(desc);

		calculateWidth();
	}

	void TextUtility::TextWord::addSpace(UINT32 spaceWidth)
	{
		mSpaceWidth += spaceWidth;

		calculateWidth();
	}

	void TextUtility::TextWord::removeLastChar()
	{
		if(mChars.size() > 0)
		{
			mChars.erase(mChars.end() - 1);
			calculateWidth();
		}
	}

	void TextUtility::TextWord::calculateWidth()
	{
		if(isSpacer())
		{
			mWidth = mSpaceWidth;
			mHeight = 0;
			return;
		}

		if(mChars.size() == 0)
		{
			mWidth = 0;
			mHeight = 0;
			return;
		}

		mWidth = 0;
		mHeight = 0;
		UINT32 kerning = 0;
		for(size_t i = 0; i < mChars.size() - 1; i++)
		{
			mWidth += mChars[i].xAdvance + kerning;
			mHeight = std::max(mHeight, mChars[i].height);

			kerning = 0;
			for(size_t j = 0; j < mChars[i].kerningPairs.size(); j++)
			{
				if(mChars[i].kerningPairs[j].otherCharId == mChars[i + 1].charId)
				{
					kerning = mChars[i].kerningPairs[j].amount;
					break;
				}
			}
		}

		mWidth += mChars[mChars.size() - 1].xAdvance + kerning;
	}

	TextUtility::TextLine::TextLine()
		:mWidth(0), mHeight(0), mLastWord(nullptr)
	{

	}

	TextUtility::TextLine::~TextLine()
	{
		for(auto iter = mWords.begin(); iter != mWords.end(); ++iter)
			CM_DELETE(*iter, TextWord, ScratchAlloc);
	}

	void TextUtility::TextLine::add(const CHAR_DESC& charDesc)
	{
		if(mLastWord == nullptr)
		{
			TextWord* newWord = CM_NEW(TextWord, ScratchAlloc) TextWord(false);
			mLastWord = newWord;

			mWords.push_back(mLastWord);

			mLastWord->addChar(charDesc);
		}
		else
		{
			if(mLastWord->isSpacer())
			{
				TextWord* newWord = CM_NEW(TextWord, ScratchAlloc) TextWord(false);
				mLastWord = newWord;

				mWords.push_back(mLastWord);
			}

			mLastWord->addChar(charDesc);
		}

		calculateBounds();
	}

	void TextUtility::TextLine::addSpace(UINT32 spaceWidth)
	{
		if(mLastWord == nullptr)
		{
			TextWord* newWord = CM_NEW(TextWord, ScratchAlloc) TextWord(true);
			mLastWord = newWord;

			mWords.push_back(mLastWord);

			mLastWord->addSpace(spaceWidth);
		}
		else
		{
			TextWord* newWord = CM_NEW(TextWord, ScratchAlloc) TextWord(true); // Each space is counted as its own word, to make certain operations easier
			mLastWord = newWord;

			mWords.push_back(mLastWord);

			mLastWord->addSpace(spaceWidth);
		}

		calculateBounds();
	}

	void TextUtility::TextLine::addWord(TextWord* word)
	{
		mWords.push_back(word);
		mLastWord = word;

		calculateBounds();
	}

	TextUtility::TextWord* TextUtility::TextLine::removeLastWord()
	{
		if(mWords.size() == 0)
			return nullptr;

		TextWord* word = mWords[mWords.size() - 1];
		mWords.erase(mWords.end() - 1);

		if(mWords.size() > 0)
			mLastWord = mWords[mWords.size() - 1];
		else
			mLastWord = nullptr;

		calculateBounds();

		return word;
	}

	std::vector<UINT32> TextUtility::TextLine::getNumQuadsPerPage() const
	{
		std::vector<UINT32> quadsPerPage;
		for(auto wordIter = mWords.begin(); wordIter != mWords.end(); ++wordIter)
		{
			if(!(*wordIter)->isSpacer())
			{
				const vector<CHAR_DESC>::type& chars = (*wordIter)->getChars();
				UINT32 kerning = 0;
				for(auto charIter = chars.begin(); charIter != chars.end(); ++charIter)
				{
					if(charIter->page > (UINT32)quadsPerPage.size())
						quadsPerPage.resize(charIter->page + 1);

					quadsPerPage[charIter->page]++;
				}
			}
		}

		return quadsPerPage;
	}

	void TextUtility::TextLine::fillBuffer(UINT32 page, Vector2* vertices, Vector2* uvs, UINT32* indexes, UINT32 offset, UINT32 size, const FontData& fontData) const
	{
		UINT32 penX = 0;
		UINT32 baselineY = fontData.fontDesc.baselineOffset;
		for(auto wordIter = mWords.begin(); wordIter != mWords.end(); ++wordIter)
		{
			if((*wordIter)->isSpacer())
			{
				penX += fontData.fontDesc.spaceWidth;
			}
			else
			{
				const vector<CHAR_DESC>::type& chars = (*wordIter)->getChars();
				UINT32 kerning = 0;
				for(auto charIter = chars.begin(); charIter != chars.end(); ++charIter)
				{
					INT32 curX = penX + charIter->xOffset;
					INT32 curY = ((INT32)baselineY - charIter->yOffset);

					penX += charIter->xAdvance + kerning;
					
					kerning = 0;
					if((charIter + 1) != chars.end())
					{
						for(size_t j = 0; j < charIter->kerningPairs.size(); j++)
						{
							if(charIter->kerningPairs[j].otherCharId == (charIter + 1)->charId)
							{
								kerning = charIter->kerningPairs[j].amount;
								break;
							}
						}
					}

					if(charIter->page != page)
						continue;

					UINT32 curVert = offset * 4;
					UINT32 curIndex = offset * 6;

					vertices[curVert + 0] = Vector2((float)curX, (float)curY);
					vertices[curVert + 1] = Vector2((float)(curX + charIter->width), (float)curY);
					vertices[curVert + 2] = Vector2((float)curX, (float)curY + (float)charIter->height);
					vertices[curVert + 3] = Vector2((float)(curX + charIter->width), (float)curY + (float)charIter->height);

					uvs[curVert + 0] = Vector2(charIter->uvX, charIter->uvY);
					uvs[curVert + 1] = Vector2(charIter->uvX + charIter->uvWidth, charIter->uvY);
					uvs[curVert + 2] = Vector2(charIter->uvX, charIter->uvY + charIter->uvHeight);
					uvs[curVert + 3] = Vector2(charIter->uvX + charIter->uvWidth, charIter->uvY + charIter->uvHeight);

					indexes[curIndex + 0] = curVert + 0;
					indexes[curIndex + 1] = curVert + 1;
					indexes[curIndex + 2] = curVert + 2;
					indexes[curIndex + 3] = curVert + 1;
					indexes[curIndex + 4] = curVert + 3;
					indexes[curIndex + 5] = curVert + 2;

					offset++;
				}
			}
		}
	}

	void TextUtility::TextLine::calculateBounds()
	{
		mWidth = 0;
		mHeight = 0;
		for(auto iter = mWords.begin(); iter != mWords.end(); ++iter)
		{
			mWidth += (*iter)->getWidth();
			mHeight = std::max(mHeight, (*iter)->getHeight());
		}
	}

	TextUtility::TextData::~TextData()
	{
		for(size_t i = 0; i < mLines.size(); i++)
			CM_DELETE(mLines[i], TextLine, ScratchAlloc);
	}

	std::shared_ptr<TextUtility::TextData> TextUtility::getTextData(const String& text, const HFont& font, UINT32 fontSize, UINT32 width, UINT32 height, bool wordWrap)
	{
		const FontData* fontData = nullptr;
		if(font != nullptr)
		{
			UINT32 nearestSize = font->getClosestAvailableSize(fontSize);
			fontData = font->getFontDataForSize(nearestSize);
		}

		if(fontData == nullptr)
			return nullptr;

		if(fontData->size != fontSize)
		{
			LOGWRN("Unable to find font with specified size (" + toString(fontSize) + "). Using nearest available size: " + toString(fontData->size));
		}

		bool heightIsLimited = height > 0;
		bool widthIsLimited = width > 0;

		std::shared_ptr<TextUtility::TextData> textData(CM_NEW(TextData, PoolAlloc) TextData(), &MemAllocDeleter<TextData, PoolAlloc>::deleter);
		TextLine* curLine = CM_NEW(TextLine, ScratchAlloc) TextLine();
		textData->mLines.push_back(curLine);

		UINT32 curHeight = fontData->fontDesc.lineHeight;
		UINT32 charIdx = 0;

		while(true)
		{
			if(charIdx >= text.size())
				break;

			if(text[charIdx] == '\n')
			{
				if(heightIsLimited && (curHeight + fontData->fontDesc.lineHeight * 2) > height)
					break; // Max height reached

				curLine = CM_NEW(TextLine, ScratchAlloc) TextLine();
				textData->mLines.push_back(curLine);
				curHeight += fontData->fontDesc.lineHeight;

				charIdx++;
				continue;
			}

			UINT32 charId = text[charIdx];
			const CHAR_DESC& charDesc = fontData->getCharDesc(charId);

			if(charId != SPACE_CHAR)
			{
				curLine->add(charDesc);

				if(charDesc.page >= (UINT32)textData->mQuadsPerPage.size())
					textData->mQuadsPerPage.resize(charDesc.page + 1);

				textData->mQuadsPerPage[charDesc.page]++;
			}
			else
				curLine->addSpace(fontData->fontDesc.spaceWidth);

			if(widthIsLimited && curLine->getWidth() > width)
			{
				if(wordWrap)
				{
					TextWord* lastWord = curLine->removeLastWord();
					if(lastWord->isSpacer())
						curLine->addWord(lastWord); // Spaces can stay on previous line even if they don't technically fit

					// No more lines fit vertically so we're done
					if(heightIsLimited && curHeight > height)
						break;

					curLine = CM_NEW(TextLine, ScratchAlloc) TextLine();
					textData->mLines.push_back(curLine);
					curHeight += fontData->fontDesc.lineHeight;

					if(!lastWord->isSpacer())
						curLine->addWord(lastWord);
				}
				else
				{
					// Nothing else we can do, chars don't fit so we are done
					break;
				}
			}

			charIdx++;
		}

		return textData;
	}
}