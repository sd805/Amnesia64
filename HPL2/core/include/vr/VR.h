#pragma once

#include <vector>
#include <list>
#include <map>
#include <array>

#include "openxr.h"
#include "openxr_platform.h"
#include "GL/glew.h"

#include "math/MathTypes.h"
#include "math/Math.h"
#include "math/Matrix.h"


struct EulerAngles
{
	float roll, pitch, yaw;
};

struct EulerAngleXYZ
{
	float x, y, z;
};

struct TextureBounds
{
	float uMin, vMin;
	float uMax, vMax;
};

struct Swapchain
{
	XrSwapchain handle;
	int32_t width;
	int32_t height;
};

class cVR;

extern cVR *gpVR;

class cVR
{
public:
	enum Side
	{
		LEFT = 0,
		RIGHT = 1,
		COUNT = 2
	};

	struct InputState 
	{
		XrActionSet actionSet{ XR_NULL_HANDLE };
		XrAction grabAction{ XR_NULL_HANDLE };
		XrAction poseAction{ XR_NULL_HANDLE };
		XrAction vibrateAction{ XR_NULL_HANDLE };
		XrAction quitAction{ XR_NULL_HANDLE };
		XrAction jumpAction{ XR_NULL_HANDLE };
		XrAction moveAction{ XR_NULL_HANDLE };
		std::array<XrPath, Side::COUNT> handSubactionPath;
		std::array<XrSpace, Side::COUNT> handSpace;
		std::array<float, Side::COUNT> handScale = { {1.0f, 1.0f} };
		std::array<XrBool32, Side::COUNT> handActive;
	};

	struct InputValues
	{
		XrActionStateBoolean jumpValue{ XR_TYPE_ACTION_STATE_BOOLEAN }; 
		XrActionStateVector2f moveValue{ XR_TYPE_ACTION_STATE_VECTOR2F };
	};

	cVR();
	~cVR();

	void Update();

	std::vector<XrSwapchainImageBaseHeader *> AllocateSwapchainImageStructs(uint32_t capacity, const XrSwapchainCreateInfo &);
	inline void CheckXrResult(XrResult res, const char *originator = nullptr, const char *sourceLocation = nullptr);
	EulerAngles ToEulerAngles(const XrQuaternionf &q);
	const XrEventDataBaseHeader *TryReadNextEvent(XrInstance &m_instance);
	void AcquireSwapchainTexture(int eye);
	void CopyFrameBufferToSwapchain();
	void ReleaseSwapchain(int eye);
	void PreRender();
	void PostRender();
	EulerAngleXYZ ToEulerAnglesXYZ(const XrQuaternionf &q); 
	void QuaternionToRotationMat(hpl::cMatrixf *result, const XrQuaternionf *quat);
	void InitializeActions();
	void PollActions();


	std::list<std::vector<XrSwapchainImageOpenGLKHR>> mSwapchainImageBuffers;
	XrSwapchainImageBaseHeader *mSwapchainImage;
	XrEventDataBuffer mEventDataBuffer;
	XrSessionState mSessionState{ XR_SESSION_STATE_UNKNOWN };
	bool mSessionRunning;
	XrInstance mInstance;
	XrSession mSession{ XR_NULL_HANDLE };
	InputState mInput;
	InputValues mInputValues;
	std::vector<Swapchain> m_swapchains;
	std::vector<XrView> m_views{};
	std::vector<XrCompositionLayerProjectionView> projectionLayerViews;
	std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader *>> m_swapchainImages;
	TextureBounds m_TextureBounds[2];
	XrSpace mViewSpace;
	XrSpace mLocalSpace;
	XrSpace mStageSpace;
	bool mRenderLayer;
	XrFrameState mFrameState{ XR_TYPE_FRAME_STATE };
	int mCurrentEye = -1;
	EulerAngles mHmdAngles;
	EulerAngleXYZ mHmdAnglesXYZ;
	XrQuaternionf mRawHmdOrientation;
	XrSpaceLocation view_in_stage = { XR_TYPE_SPACE_LOCATION, NULL, 0, {{0, 0, 0, 1}, {0, 0, 0}} };
	XrSpaceLocation view_in_stage2 = { XR_TYPE_SPACE_LOCATION, NULL, 0, {{0, 0, 0, 1}, {0, 0, 0}} };

	float mAspect;
	float mFOV;
};
