// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_client.h"
#include "pragma/c_engine.h"
#include "pragma/rendering/c_msaa.h"
#include "prosper_util.hpp"
#include <mathutil/umath.h>

extern DLLCLIENT CEngine *c_engine;

int GetMaxMSAASampleCount()
{
	auto props = c_engine->GetRenderContext().GetPhysicalDeviceImageFormatProperties(
	  {prosper::ImageCreateFlags {}, prosper::Format::R16G16B16A16_SFloat, prosper::ImageType::e2D, prosper::ImageTiling::Optimal, prosper::ImageUsageFlags::SampledBit | prosper::ImageUsageFlags::ColorAttachmentBit | prosper::ImageUsageFlags::TransferSrcBit});
	if(props.has_value() == false) {
		Con::cwar << "Unable to retrieve max MSAA sample count! Setting sample count to 1..." << Con::endl;
		return 1;
	}
	return umath::get_highest_bit(umath::to_integral(props->sampleCount));
}
unsigned char ClampMSAASampleCount(unsigned int *samples)
{
	auto maxSamples = CUInt32(GetMaxMSAASampleCount());
	if(*samples > maxSamples) {
		*samples = maxSamples;
		return 1;
	}
	else if(*samples == 1 || (*samples & (*samples - 1)) != 0) {
		if(*samples == 1)
			*samples = 1;
		else {
			unsigned int newSamples = 2;
			while(newSamples <= *samples)
				newSamples *= 2;
			newSamples /= 2;
			*samples = newSamples;
		}
		return 2;
	}
	if(*samples == 0)
		*samples = 1;
	return 0;
}
