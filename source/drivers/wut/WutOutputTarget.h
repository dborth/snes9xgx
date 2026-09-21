/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutOutputTarget.h
 ***************************************************************************/
#pragma once

//!The two physical render targets every Wii U frame is submitted to.
//!Their pixel dimensions are not fixed: the TV follows the console's output
//!setting (480p/720p/1080p), the GamePad is always 854x480. See
//!WutVideoDriver::getTargetWidth()/getTargetHeight().
enum class OutputTarget
{
	TV = 0,
	DRC = 1
};

static const int OUTPUT_TARGET_COUNT = 2;
