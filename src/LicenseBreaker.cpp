// =====================================================================================================================

//
// File        : LicenseBreaker.cpp
// Project     : LicenseBreaker
// Author      : Stefan Arentz, stefan.arentz@soze.com
// Version     : $Id$
// Environment : BeOS 4.5/Intel
//
//

// =====================================================================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <be/interface/StringView.h>
#include <be/interface/Slider.h>
#include <be/interface/Font.h>
#include <be/support/SupportDefs.h>

extern "C" {
}

#include "SetupView.h"
#include "ElfUtils.h"
#include "LicenseBreaker.h"

// =====================================================================================================================

// MAIN INSTANTIATION FUNCTION
extern "C" _EXPORT BScreenSaver *instantiate_screen_saver(BMessage *message, image_id image)
{
	return new LicenseBreaker(message, image);
}

// =====================================================================================================================

LicenseBreaker::LicenseBreaker(BMessage *message, image_id image)
 : BScreenSaver(message, image)
{
	if (message->FindInt32("scrollspeed", &mScrollSpeed) != B_OK) {
		mScrollSpeed = 15;
	}
	
	mSymbolTable = NULL;
	mCurrentFunction = NULL;
}

// =====================================================================================================================

status_t LicenseBreaker::SaveState(BMessage *into) const
{
	into->AddInt32("scrollspeed", mScrollSpeed);
	return B_OK;
}

// =====================================================================================================================

void LicenseBreaker::StartConfig(BView *view)
{
	mSetupView = new SetupView(view->Bounds(), "setup", &mScrollSpeed);
	view->AddChild(mSetupView);
}

// =====================================================================================================================

status_t LicenseBreaker::StartSaver(BView *view, bool preview)
{
	//SetTickSize(100000);

	if (preview == true) {
		mRunning = false;
		return B_ERROR;
	}
	
	mView = view;
	
	BFont font(be_fixed_font);
	font.SetSize(16);
	view->SetFont(&font);
	
	mHighColor = 0;
	
	mLineHeight = 20;	
	mMaxLines = (view->Bounds().IntegerHeight() / mLineHeight) - 1;
	mLine = 1;

	SetTickSize((1000 - mScrollSpeed) * 100);
	
	mRunning = true;
	
	return B_OK;
}

// =====================================================================================================================

void LicenseBreaker::Print(char* inString)
{
	mView->MovePenTo(10, (mLine * mLineHeight));
	mView->DrawString(inString);
	
	mLine++;
	if (mLine > mMaxLines)
	{
		mView->ScrollBy(0, mLineHeight);
	}
}

// =====================================================================================================================

void LicenseBreaker::RotateHighColor(void)
{
	mView->SetHighColor(127 + (rand() % 127), 127 + (rand() % 127), 127 + (rand() % 127));
}

// =====================================================================================================================

void LicenseBreaker::Draw(BView *view, int32 frame)
{
	if (mRunning == false) {
		return;
	}

	//
	// Initialize on frame 0.
	//

	if (frame == 0)
	{
		view->SetLowColor(0, 0, 0);
		view->FillRect(view->Bounds(), B_SOLID_LOW);
	}
	
	if (mSymbolTable == NULL)
	{
		char* error;
		GetSymbolTable("/boot/beos/system/kernel_intel", &mSymbolTable, &error);
	}
	
	if (mCurrentFunction == NULL)
	{
		if (mSymbolTable != NULL)
		{
			mCurrentFunction = &(mSymbolTable->functions)[(rand() + clock()) % mSymbolTable->count];
			
			//
			// Read the code into memory
			//
		
			mCode = (unsigned char*) malloc(mCurrentFunction->size);
			if (mCode != NULL)
			{
				FILE* file = fopen("/boot/beos/system/kernel_intel", "rb");
				if (file != NULL)
				{
					fseek(file, mCurrentFunction->offset, SEEK_SET);
					fread(mCode, mCurrentFunction->size, 1, file);
					fclose(file);
				}
			}
			
			mProgramCounter = mCode;
			
			this->RotateHighColor();
			
			char string[1024];
			sprintf(string, "Disassembling from %s (%s):", mCurrentFunction->name, "/boot/beos/system/kernel_intel");
			this->Print(string);
		}
	}

	//
	// Disassemble the code
	//

	if (mProgramCounter != NULL)
	{
		// Check if we're past the end of the current function
		
		char out[2048];
		status_t err = disasm(mProgramCounter, 10, out, sizeof(out), NULL, 0, NULL, NULL);
		if (err > 0)
		{
			// Convert the opcodes to hex
			
			char hex[20];
			hex[0] = 0x00;
			for (int j = 0; j < err; j++)
			{
				char tmp[8];
				sprintf(tmp, " %02x", mProgramCounter[j]);
				strcat(hex, tmp);
			}
	
			// Draw the string
	
			char string[1024];
			sprintf(string, "  + 0x%08x %-40s |%s", mProgramCounter - mCode, out, hex);
			this->Print(string);
	
		}
	
		// Move to the next instruction
	
		mProgramCounter += err;
		
		if (mProgramCounter >= (mCode + mCurrentFunction->size))
		{
			free(mCode);
			mCode = mProgramCounter = NULL;
			mCurrentFunction = NULL;
		}
	}
}

// =====================================================================================================================
