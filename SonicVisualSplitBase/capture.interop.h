// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
#pragma once
#include <Unknwn.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <windows.graphics.capture.interop.h>

inline auto CreateCaptureItemForWindow(HWND hwnd) {
	auto activation_factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
	auto interop_factory = activation_factory.as<IGraphicsCaptureItemInterop>();
	winrt::Windows::Graphics::Capture::GraphicsCaptureItem item = {nullptr};
	interop_factory->CreateForWindow(hwnd, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), reinterpret_cast<void**>(winrt::put_abi(item)));
	return item;
}
