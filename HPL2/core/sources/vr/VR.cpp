#include "vr/VR.h"

#include <string>
#include <iostream>
#include <map>
#include <algorithm>
#include <initializer_list>
#include "openxr.h"
#include "openxr_platform.h"
#include "GL/glew.h"

#define CHK_STRINGIFY(x) #x
#define TOSTRING(x) CHK_STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)
#define CHECK_XRCMD(cmd) CheckXrResult(cmd, #cmd, FILE_AND_LINE);
#define PI 3.1415926

cVR *gpVR;

cVR::cVR()
{
	// OPENXR START
	AllocConsole();
	FILE *fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);

	mSwapchainImage = nullptr;
	std::vector<const char *> extensions{};
	extensions.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
	XrInstanceCreateInfo createInfo{ XR_TYPE_INSTANCE_CREATE_INFO };
	createInfo.next = nullptr;
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.enabledExtensionNames = extensions.data();

	strcpy(createInfo.applicationInfo.applicationName, "HelloXR");
	createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

	CHECK_XRCMD(xrCreateInstance(&createInfo, &mInstance));

	XrSystemId _systemId;
	XrSystemGetInfo systemInfo{ XR_TYPE_SYSTEM_GET_INFO };
	systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	CHECK_XRCMD(xrGetSystem(mInstance, &systemInfo, &_systemId));

	PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = nullptr;
	CHECK_XRCMD(xrGetInstanceProcAddr(mInstance, "xrGetOpenGLGraphicsRequirementsKHR",
		reinterpret_cast<PFN_xrVoidFunction *>(&pfnGetOpenGLGraphicsRequirementsKHR)));
	XrGraphicsRequirementsOpenGLKHR graphicsRequirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
	pfnGetOpenGLGraphicsRequirementsKHR(mInstance, _systemId, &graphicsRequirements);

	GLint major = 0;
	GLint minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	const XrVersion desiredApiVersion = XR_MAKE_VERSION(major, minor, 0);
	if (graphicsRequirements.minApiVersionSupported > desiredApiVersion)
	{
	}

	XrGraphicsBindingOpenGLWin32KHR graphicsBinding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
	XrSessionCreateInfo _sessionCreateInfo{ XR_TYPE_SESSION_CREATE_INFO };
	_sessionCreateInfo.systemId = _systemId;
	graphicsBinding.hGLRC = wglGetCurrentContext();
	graphicsBinding.hDC = wglGetCurrentDC();
	_sessionCreateInfo.next = &graphicsBinding;
	CHECK_XRCMD(xrCreateSession(mInstance, &_sessionCreateInfo, &mSession));

	InitializeActions();

	XrReferenceSpaceCreateInfo viewSpaceCreateInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	viewSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	viewSpaceCreateInfo.poseInReferenceSpace.orientation.w = 1;
	CHECK_XRCMD(xrCreateReferenceSpace(mSession, &viewSpaceCreateInfo, &mViewSpace));

	XrReferenceSpaceCreateInfo localSpaceCreateInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	localSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	localSpaceCreateInfo.poseInReferenceSpace.orientation.w = 1;
	CHECK_XRCMD(xrCreateReferenceSpace(mSession, &localSpaceCreateInfo, &mLocalSpace));

	XrReferenceSpaceCreateInfo stageSpaceCreateInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	stageSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
	stageSpaceCreateInfo.poseInReferenceSpace.orientation.w = 1;
	CHECK_XRCMD(xrCreateReferenceSpace(mSession, &stageSpaceCreateInfo, &mStageSpace));

	XrSystemProperties systemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
	CHECK_XRCMD(xrGetSystemProperties(mInstance, _systemId, &systemProperties));

	XrViewConfigurationType viewConfig{ XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO };
	uint32_t viewCount;
	CHECK_XRCMD(xrEnumerateViewConfigurationViews(mInstance, _systemId, viewConfig, 0, &viewCount, nullptr));
	std::vector<XrViewConfigurationView> m_configViews{};
	m_configViews.resize(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
	CHECK_XRCMD(xrEnumerateViewConfigurationViews(mInstance, _systemId, viewConfig, viewCount, &viewCount, m_configViews.data()));

	m_views.resize(viewCount, { XR_TYPE_VIEW });

	uint32_t swapchainFormatCount;
	CHECK_XRCMD(xrEnumerateSwapchainFormats(mSession, 0, &swapchainFormatCount, nullptr));
	std::vector<int64_t> swapchainFormats(swapchainFormatCount);
	CHECK_XRCMD(xrEnumerateSwapchainFormats(mSession, (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));

	// Create a swapchain for each view.
	for (uint32_t i = 0; i < viewCount; i++) {
		const XrViewConfigurationView &vp = m_configViews[i];

		// Create the swapchain.
		XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
		swapchainCreateInfo.arraySize = 1;
		swapchainCreateInfo.format = GL_SRGB8_ALPHA8;
		swapchainCreateInfo.width = vp.recommendedImageRectWidth;
		swapchainCreateInfo.height = vp.recommendedImageRectHeight;
		swapchainCreateInfo.mipCount = 1;
		swapchainCreateInfo.faceCount = 1;
		swapchainCreateInfo.sampleCount = vp.recommendedSwapchainSampleCount;
		swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		Swapchain swapchain;
		swapchain.width = swapchainCreateInfo.width;
		swapchain.height = swapchainCreateInfo.height;
		CHECK_XRCMD(xrCreateSwapchain(mSession, &swapchainCreateInfo, &swapchain.handle));

		m_swapchains.push_back(swapchain);

		uint32_t imageCount;
		CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount, nullptr));
		// XXX This should really just return XrSwapchainImageBaseHeader*
		std::vector<XrSwapchainImageBaseHeader *> swapchainImages =
			AllocateSwapchainImageStructs(imageCount, swapchainCreateInfo);
		CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount, &imageCount, swapchainImages[0]));

		m_swapchainImages.insert(std::make_pair(swapchain.handle, std::move(swapchainImages)));
	}

	// Get eye projection info
	XrViewState viewState{ XR_TYPE_VIEW_STATE };
	uint32_t viewCapacityInput = (uint32_t)m_views.size();
	uint32_t viewCountOutput;

	XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
	viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	viewLocateInfo.displayTime = 1;
	viewLocateInfo.space = mLocalSpace;

	CHECK_XRCMD(xrLocateViews(mSession, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views.data()));

	float tanHalfFov[2];

	float l_tanLeft = tanf(m_views[0].fov.angleLeft);
	float l_tanRight = tanf(m_views[0].fov.angleRight);
	float l_tanDown = tanf(m_views[0].fov.angleDown);
	float l_tanUp = tanf(m_views[0].fov.angleUp);

	float r_tanLeft = tanf(m_views[1].fov.angleLeft);
	float r_tanRight = tanf(m_views[1].fov.angleRight);
	float r_tanDown = tanf(m_views[1].fov.angleDown);
	float r_tanUp = tanf(m_views[1].fov.angleUp);

	tanHalfFov[0] = std::max({ -l_tanLeft, l_tanRight, -r_tanLeft, r_tanRight });
	tanHalfFov[1] = std::max({ l_tanUp, -l_tanDown, r_tanUp, -r_tanDown });

	mAspect = tanHalfFov[0] / tanHalfFov[1];
	mFOV = 2.0f * atan(tanHalfFov[1]) * 360 / (3.14159265358979323846 * 2);

	m_TextureBounds[0].uMin = 0.5f + 0.5f * l_tanLeft / tanHalfFov[0];
	m_TextureBounds[0].uMax = 0.5f + 0.5f * l_tanRight / tanHalfFov[0];
	m_TextureBounds[0].vMin = 0.5f - 0.5f * l_tanUp / tanHalfFov[1];
	m_TextureBounds[0].vMax = 0.5f - 0.5f * l_tanDown / tanHalfFov[1];

	m_TextureBounds[1].uMin = 0.5f + 0.5f * r_tanLeft / tanHalfFov[0];
	m_TextureBounds[1].uMax = 0.5f + 0.5f * r_tanRight / tanHalfFov[0];
	m_TextureBounds[1].vMin = 0.5f - 0.5f * r_tanUp / tanHalfFov[1];
	m_TextureBounds[1].vMax = 0.5f - 0.5f * r_tanDown / tanHalfFov[1];

	// Set to tanAngleDown - tanAngleUp for a clip space with positive Y down (Vulkan).
	// Set to tanAngleUp - tanAngleDown for a clip space with positive Y up (OpenGL / D3D / Metal).
	//const float tanAngleWidth = tanRight - tanLeft;
	//const float tanAngleHeight = (tanUp - tanDown);
}

void cVR::Update()
{
	// Process all pending messages.
	while (const XrEventDataBaseHeader *event = TryReadNextEvent(mInstance))
	{
		switch (event->type)
		{
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		{
			auto sessionStateChangedEvent = *reinterpret_cast<const XrEventDataSessionStateChanged *>(event);
			const XrSessionState oldState = mSessionState;
			mSessionState = sessionStateChangedEvent.state;

			printf("XrEventDataSessionStateChanged: state %s->%s session=%lld time=%lld", std::to_string(oldState),
				std::to_string(mSessionState), sessionStateChangedEvent.session, sessionStateChangedEvent.time);

			if ((sessionStateChangedEvent.session != XR_NULL_HANDLE) && (sessionStateChangedEvent.session != mSession)) {
				printf("XrEventDataSessionStateChanged for unknown session");
				return;
			}
			switch (mSessionState) {
			case XR_SESSION_STATE_READY: {
				XrSessionBeginInfo sessionBeginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
				sessionBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
				CHECK_XRCMD(xrBeginSession(mSession, &sessionBeginInfo));
				mSessionRunning = true;
				break;
			}
			case XR_SESSION_STATE_STOPPING: {
				mSessionRunning = false;
				CHECK_XRCMD(xrEndSession(mSession));
				break;
			}
			case XR_SESSION_STATE_EXITING: {
				//*exitRenderLoop = true;
				// Do not attempt to restart because user closed this session.
				//*requestRestart = false;
				break;
			}
			case XR_SESSION_STATE_LOSS_PENDING: {
				//*exitRenderLoop = true;
				// Poll for a new instance.
				//*requestRestart = true;
				break;
			}
			default:
				break;
			}
			break;
		}
		}
	}

	PollActions();
}

inline void cVR::CheckXrResult(XrResult res, const char *originator, const char *sourceLocation)
{
	if (res != XR_SUCCESS)
	{
		std::string orig(originator);
		std::string source(sourceLocation);
		MessageBox(0, (orig + ": " + source).c_str(), "AmnesiaVR", MB_ICONERROR | MB_OK);
	}
}

std::vector<XrSwapchainImageBaseHeader *> cVR::AllocateSwapchainImageStructs(uint32_t capacity, const XrSwapchainCreateInfo &)
{
	// Allocate and initialize the buffer of image structs (must be sequential in memory for xrEnumerateSwapchainImages).
	// Return back an array of pointers to each swapchain image struct so the consumer doesn't need to know the type/size.
	std::vector<XrSwapchainImageOpenGLKHR> swapchainImageBuffer(capacity);
	std::vector<XrSwapchainImageBaseHeader *> swapchainImageBase;
	for (XrSwapchainImageOpenGLKHR &image : swapchainImageBuffer)
	{
		image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
		swapchainImageBase.push_back(reinterpret_cast<XrSwapchainImageBaseHeader *>(&image));
	}

	// Keep the buffer alive by moving it into the list of buffers.
	mSwapchainImageBuffers.push_back(std::move(swapchainImageBuffer));

	return swapchainImageBase;
}

// this implementation assumes normalized quaternion
// converts to Euler angles in 3-2-1 sequence
EulerAngles cVR::ToEulerAngles(const XrQuaternionf &q)
{
	EulerAngles angles;

	// pitch (x-axis rotation)
	float sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
	float cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
	angles.pitch = std::atan2(sinr_cosp, cosr_cosp);

	// yaw (y-axis rotation)
	float sinp = std::sqrt(1 + 2 * (q.w * q.y - q.x * q.z));
	float cosp = std::sqrt(1 - 2 * (q.w * q.y - q.x * q.z));
	angles.yaw = 2 * std::atan2(sinp, cosp) - PI / 2;

	// roll (z-axis rotation)
	float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
	float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
	angles.roll = std::atan2(siny_cosp, cosy_cosp);

	return angles;
}

// Return event if one is available, otherwise return null.
const XrEventDataBaseHeader *cVR::TryReadNextEvent(XrInstance &m_instance)
{
	// It is sufficient to clear the just the XrEventDataBuffer header to
	// XR_TYPE_EVENT_DATA_BUFFER
	XrEventDataBaseHeader *baseHeader = reinterpret_cast<XrEventDataBaseHeader *>(&mEventDataBuffer);
	*baseHeader = { XR_TYPE_EVENT_DATA_BUFFER };
	const XrResult xr = xrPollEvent(m_instance, &mEventDataBuffer);
	if (xr == XR_SUCCESS) {
		if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
			const XrEventDataEventsLost *const eventsLost = reinterpret_cast<const XrEventDataEventsLost *>(baseHeader);
			std::cout << "Events lost: " << eventsLost->lostEventCount << std::endl;
		}

		return baseHeader;
	}
	if (xr == XR_EVENT_UNAVAILABLE) {
		return nullptr;
	}
	std::cout << "Error polling event" << std::endl;
	return nullptr;
}

void cVR::AcquireSwapchainTexture(int eye)
{
	if (!mFrameState.shouldRender)
		return;

	mCurrentEye = eye;

	// Each view has a separate swapchain which is acquired, rendered to, and released.
	const Swapchain viewSwapchain = m_swapchains[eye];

	XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };

	uint32_t swapchainImageIndex;
	CHECK_XRCMD(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo, &swapchainImageIndex));

	XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	waitInfo.timeout = XR_INFINITE_DURATION;
	CHECK_XRCMD(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

	projectionLayerViews[eye] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
	projectionLayerViews[eye].pose = m_views[eye].pose;
	projectionLayerViews[eye].fov = m_views[eye].fov;
	projectionLayerViews[eye].subImage.swapchain = viewSwapchain.handle;

	int xOffset = m_TextureBounds[eye].uMin * viewSwapchain.width;
	int yOffset = m_TextureBounds[eye].vMin * viewSwapchain.height;
	int xExtent = m_TextureBounds[eye].uMax * viewSwapchain.width;
	int yExtent = m_TextureBounds[eye].vMax * viewSwapchain.height;

	//yExtent -= yOffset;
	//yOffset = 0;

	projectionLayerViews[eye].subImage.imageRect.offset = { xOffset, yOffset };
	projectionLayerViews[eye].subImage.imageRect.extent = { xExtent - xOffset, yExtent - yOffset };

	//const XrSwapchainImageBaseHeader *const swapchainImage = m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];
	mSwapchainImage = m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];
}

void cVR::CopyFrameBufferToSwapchain()
{
	if (!mFrameState.shouldRender)
		return;

	const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLKHR *>(mSwapchainImage)->image;

	glBindTexture(GL_TEXTURE_2D, colorTexture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 2704, 2736);
}

void cVR::ReleaseSwapchain(int eye)
{
	if (!mFrameState.shouldRender)
		return;

	const Swapchain viewSwapchain = m_swapchains[eye];

	XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
	CHECK_XRCMD(xrReleaseSwapchainImage(viewSwapchain.handle, &releaseInfo));

	mCurrentEye = -1;
}

void cVR::PreRender()
{
	XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
	CHECK_XRCMD(xrWaitFrame(mSession, &frameWaitInfo, &mFrameState));

	XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
	CHECK_XRCMD(xrBeginFrame(mSession, &frameBeginInfo));

	// HEADSET STUFF
	xrLocateSpace(mViewSpace, mLocalSpace, mFrameState.predictedDisplayTime, &view_in_stage);
	xrLocateSpace(mViewSpace, mStageSpace, mFrameState.predictedDisplayTime, &view_in_stage2);

	mRawHmdOrientation = view_in_stage2.pose.orientation;
	mHmdAngles = ToEulerAngles(view_in_stage2.pose.orientation);
	mHmdAnglesXYZ = ToEulerAnglesXYZ(view_in_stage2.pose.orientation);

	if (mFrameState.shouldRender == XR_TRUE)
	{
		mRenderLayer = true;
		// START OF RenderLayer
		XrViewState viewState{ XR_TYPE_VIEW_STATE };
		uint32_t viewCapacityInput = (uint32_t)m_views.size();
		uint32_t viewCountOutput;

		XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
		viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		viewLocateInfo.displayTime = mFrameState.predictedDisplayTime;
		viewLocateInfo.space = mStageSpace;

		CHECK_XRCMD(xrLocateViews(mSession, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views.data()));
		if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
			(viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0)
		{
			mRenderLayer = false;  // There is no valid tracking poses for the views.
		}

		projectionLayerViews.resize(viewCountOutput);
	}
}
void cVR::PostRender()
{

	std::vector<XrCompositionLayerBaseHeader *> layers;
	XrCompositionLayerProjection layer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };

	layer.space = mLocalSpace;
	layer.layerFlags = 0;
	//layer.layerFlags =
	//	m_options->Parsed.EnvironmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND
	//	? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT
	//	: 0;
	layer.viewCount = (uint32_t)projectionLayerViews.size();
	layer.views = projectionLayerViews.data();

	// END OF RenderLayer
	if (mRenderLayer)
	{
		layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&layer));
	}

	XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
	frameEndInfo.displayTime = mFrameState.predictedDisplayTime;
	frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	frameEndInfo.layerCount = (uint32_t)layers.size();
	frameEndInfo.layers = layers.data();
	auto endFrameResult = xrEndFrame(mSession, &frameEndInfo);
	if (endFrameResult != XR_SUCCESS)
		std::cout << "xrEndFrame failed with code " << endFrameResult << std::endl;
}

EulerAngleXYZ cVR::ToEulerAnglesXYZ(const XrQuaternionf &in)
{
	EulerAngleXYZ euler;
	const static float PI_OVER_2 = PI * 0.5f;
	const static float EPSILON = 1e-10f;
	float sqw;
	float sqx;
	float sqy;
	float sqz;

	// quick conversion to Euler angles to give tilt to user
	sqw = in.w * in.w;
	sqx = in.x * in.x;
	sqy = in.y * in.y;
	sqz = in.z * in.z;

	euler.y = asinf(2.0f * (in.w * in.y - in.x * in.z));
	if (PI_OVER_2 - fabs(euler.y) > EPSILON) {
		euler.z = atan2f(2.0f * (in.x * in.y + in.w * in.z), sqx - sqy - sqz + sqw);
		euler.x = atan2f(2.0f * (in.w * in.x + in.y * in.z), sqw - sqx - sqy + sqz);
	}
	else {
		// compute heading from local 'down' vector
		euler.z = atan2f(2.f * in.y * in.z - 2.f * in.x * in.w, 2.f * in.x * in.z + 2.f * in.y * in.w);
		euler.x = 0.0f;

		// If facing down, reverse yaw
		if (euler.y < 0.f) {
			euler.z = PI - euler.z;
		}
	}
	return euler;
}

// Creates a matrix from a quaternion.
void cVR::QuaternionToRotationMat(hpl::cMatrixf *result, const XrQuaternionf *quat) 
{
	const float x2 = quat->x + quat->x;
	const float y2 = quat->y + quat->y;
	const float z2 = quat->z + quat->z;

	const float xx2 = quat->x * x2;
	const float yy2 = quat->y * y2;
	const float zz2 = quat->z * z2;

	const float yz2 = quat->y * z2;
	const float wx2 = quat->w * x2;
	const float xy2 = quat->x * y2;
	const float wz2 = quat->w * z2;
	const float xz2 = quat->x * z2;
	const float wy2 = quat->w * y2;

	result->v[0] = 1.0f - yy2 - zz2;
	result->v[1] = xy2 + wz2;
	result->v[2] = xz2 - wy2;
	result->v[3] = 0.0f;

	result->v[4] = xy2 - wz2;
	result->v[5] = 1.0f - xx2 - zz2;
	result->v[6] = yz2 + wx2;
	result->v[7] = 0.0f;

	result->v[8] = xz2 + wy2;
	result->v[9] = yz2 - wx2;
	result->v[10] = 1.0f - xx2 - yy2;
	result->v[11] = 0.0f;

	result->v[12] = 0.0f;
	result->v[13] = 0.0f;
	result->v[14] = 0.0f;
	result->v[15] = 1.0f;
}

void cVR::InitializeActions() 
{
	// Create an action set.
	{
		XrActionSetCreateInfo actionSetInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
		strcpy_s(actionSetInfo.actionSetName, "gameplay");
		strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
		actionSetInfo.priority = 0;
		CHECK_XRCMD(xrCreateActionSet(mInstance, &actionSetInfo, &mInput.actionSet));
	}

	// Get the XrPath for the left and right hands - we will use them as subaction paths.
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left", &mInput.handSubactionPath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right", &mInput.handSubactionPath[Side::RIGHT]));

	// Create actions.
	{
		// Create an input action for grabbing objects with the left and right hands.
		XrActionCreateInfo actionInfo{ XR_TYPE_ACTION_CREATE_INFO };
		actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
		strcpy_s(actionInfo.actionName, "grab_object");
		strcpy_s(actionInfo.localizedActionName, "Grab Object");
		actionInfo.countSubactionPaths = uint32_t(mInput.handSubactionPath.size());
		actionInfo.subactionPaths = mInput.handSubactionPath.data();
		CHECK_XRCMD(xrCreateAction(mInput.actionSet, &actionInfo, &mInput.grabAction));

		// Create an input action getting the left and right hand poses.
		actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
		strcpy_s(actionInfo.actionName, "hand_pose");
		strcpy_s(actionInfo.localizedActionName, "Hand Pose");
		actionInfo.countSubactionPaths = uint32_t(mInput.handSubactionPath.size());
		actionInfo.subactionPaths = mInput.handSubactionPath.data();
		CHECK_XRCMD(xrCreateAction(mInput.actionSet, &actionInfo, &mInput.poseAction));

		// Create output actions for vibrating the left and right controller.
		actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
		strcpy_s(actionInfo.actionName, "vibrate_hand");
		strcpy_s(actionInfo.localizedActionName, "Vibrate Hand");
		actionInfo.countSubactionPaths = uint32_t(mInput.handSubactionPath.size());
		actionInfo.subactionPaths = mInput.handSubactionPath.data();
		CHECK_XRCMD(xrCreateAction(mInput.actionSet, &actionInfo, &mInput.vibrateAction));

		// Create input actions for quitting the session using the left and right controller.
		// Since it doesn't matter which hand did this, we do not specify subaction paths for it.
		// We will just suggest bindings for both hands, where possible.
		actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		strcpy_s(actionInfo.actionName, "quit_session");
		strcpy_s(actionInfo.localizedActionName, "Quit Session");
		actionInfo.countSubactionPaths = 0;
		actionInfo.subactionPaths = nullptr;
		CHECK_XRCMD(xrCreateAction(mInput.actionSet, &actionInfo, &mInput.quitAction));

		actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		strcpy_s(actionInfo.actionName, "jump");
		strcpy_s(actionInfo.localizedActionName, "Jump");
		actionInfo.countSubactionPaths = 0;
		actionInfo.subactionPaths = nullptr;
		CHECK_XRCMD(xrCreateAction(mInput.actionSet, &actionInfo, &mInput.jumpAction));

		actionInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
		strcpy_s(actionInfo.actionName, "move");
		strcpy_s(actionInfo.localizedActionName, "Move");
		actionInfo.countSubactionPaths = 0;
		actionInfo.subactionPaths = nullptr;
		CHECK_XRCMD(xrCreateAction(mInput.actionSet, &actionInfo, &mInput.moveAction));
	}

	std::array<XrPath, Side::COUNT> selectPath;
	std::array<XrPath, Side::COUNT> squeezeValuePath;
	std::array<XrPath, Side::COUNT> squeezeForcePath;
	std::array<XrPath, Side::COUNT> squeezeClickPath;
	std::array<XrPath, Side::COUNT> posePath;
	std::array<XrPath, Side::COUNT> hapticPath;
	std::array<XrPath, Side::COUNT> menuClickPath;
	std::array<XrPath, Side::COUNT> bClickPath;
	std::array<XrPath, Side::COUNT> aClickPath;
	std::array<XrPath, Side::COUNT> triggerValuePath;
	std::array<XrPath, Side::COUNT> thumbstickValuePath;
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left/input/select/click", &selectPath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right/input/select/click", &selectPath[Side::RIGHT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left/input/squeeze/value", &squeezeValuePath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right/input/squeeze/value", &squeezeValuePath[Side::RIGHT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left/input/squeeze/force", &squeezeForcePath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right/input/squeeze/force", &squeezeForcePath[Side::RIGHT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left/input/squeeze/click", &squeezeClickPath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right/input/squeeze/click", &squeezeClickPath[Side::RIGHT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left/input/grip/pose", &posePath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right/input/grip/pose", &posePath[Side::RIGHT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left/output/haptic", &hapticPath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right/output/haptic", &hapticPath[Side::RIGHT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left/input/menu/click", &menuClickPath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right/input/menu/click", &menuClickPath[Side::RIGHT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left/input/b/click", &bClickPath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right/input/b/click", &bClickPath[Side::RIGHT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left/input/a/click", &aClickPath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right/input/a/click", &aClickPath[Side::RIGHT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left/input/trigger/value", &triggerValuePath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right/input/trigger/value", &triggerValuePath[Side::RIGHT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/left/input/thumbstick", &thumbstickValuePath[Side::LEFT]));
	CHECK_XRCMD(xrStringToPath(mInstance, "/user/hand/right/input/thumbstick", &thumbstickValuePath[Side::RIGHT]));
	// Suggest bindings for KHR Simple.
	{
		XrPath khrSimpleInteractionProfilePath;
		CHECK_XRCMD(
			xrStringToPath(mInstance, "/interaction_profiles/khr/simple_controller", &khrSimpleInteractionProfilePath));
		std::vector<XrActionSuggestedBinding> bindings{ {// Fall back to a click input for the grab action.
														{mInput.grabAction, selectPath[Side::LEFT]},
														{mInput.grabAction, selectPath[Side::RIGHT]},
														{mInput.poseAction, posePath[Side::LEFT]},
														{mInput.poseAction, posePath[Side::RIGHT]},
														{mInput.quitAction, menuClickPath[Side::LEFT]},
														{mInput.quitAction, menuClickPath[Side::RIGHT]},
														{mInput.vibrateAction, hapticPath[Side::LEFT]},
														{mInput.vibrateAction, hapticPath[Side::RIGHT]}} };
		XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
		suggestedBindings.interactionProfile = khrSimpleInteractionProfilePath;
		suggestedBindings.suggestedBindings = bindings.data();
		suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
		CHECK_XRCMD(xrSuggestInteractionProfileBindings(mInstance, &suggestedBindings));
	}
	// Suggest bindings for the Oculus Touch.
	{
		XrPath oculusTouchInteractionProfilePath;
		CHECK_XRCMD(
			xrStringToPath(mInstance, "/interaction_profiles/oculus/touch_controller", &oculusTouchInteractionProfilePath));
		std::vector<XrActionSuggestedBinding> bindings{ {
															{mInput.grabAction, squeezeValuePath[Side::LEFT]},
															{mInput.grabAction, squeezeValuePath[Side::RIGHT]},
															{mInput.poseAction, posePath[Side::LEFT]},
															{mInput.poseAction, posePath[Side::RIGHT]},
															{mInput.quitAction, menuClickPath[Side::LEFT]},
															{mInput.vibrateAction, hapticPath[Side::LEFT]},
															{mInput.vibrateAction, hapticPath[Side::RIGHT]},
															{mInput.jumpAction, aClickPath[Side::RIGHT]},
															{mInput.moveAction, thumbstickValuePath[Side::RIGHT]}
														 } 
													   };
		XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
		suggestedBindings.interactionProfile = oculusTouchInteractionProfilePath;
		suggestedBindings.suggestedBindings = bindings.data();
		suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
		CHECK_XRCMD(xrSuggestInteractionProfileBindings(mInstance, &suggestedBindings));
	}
	// Suggest bindings for the Vive Controller.
	{
		XrPath viveControllerInteractionProfilePath;
		CHECK_XRCMD(
			xrStringToPath(mInstance, "/interaction_profiles/htc/vive_controller", &viveControllerInteractionProfilePath));
		std::vector<XrActionSuggestedBinding> bindings{ {{mInput.grabAction, triggerValuePath[Side::LEFT]},
														{mInput.grabAction, triggerValuePath[Side::RIGHT]},
														{mInput.poseAction, posePath[Side::LEFT]},
														{mInput.poseAction, posePath[Side::RIGHT]},
														{mInput.quitAction, menuClickPath[Side::LEFT]},
														{mInput.quitAction, menuClickPath[Side::RIGHT]},
														{mInput.vibrateAction, hapticPath[Side::LEFT]},
														{mInput.vibrateAction, hapticPath[Side::RIGHT]}} };
		XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
		suggestedBindings.interactionProfile = viveControllerInteractionProfilePath;
		suggestedBindings.suggestedBindings = bindings.data();
		suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
		CHECK_XRCMD(xrSuggestInteractionProfileBindings(mInstance, &suggestedBindings));
	}

	// Suggest bindings for the Valve Index Controller.
	{
		XrPath indexControllerInteractionProfilePath;
		CHECK_XRCMD(
			xrStringToPath(mInstance, "/interaction_profiles/valve/index_controller", &indexControllerInteractionProfilePath));
		std::vector<XrActionSuggestedBinding> bindings{ {{mInput.grabAction, squeezeForcePath[Side::LEFT]},
														{mInput.grabAction, squeezeForcePath[Side::RIGHT]},
														{mInput.poseAction, posePath[Side::LEFT]},
														{mInput.poseAction, posePath[Side::RIGHT]},
														{mInput.quitAction, bClickPath[Side::LEFT]},
														{mInput.quitAction, bClickPath[Side::RIGHT]},
														{mInput.vibrateAction, hapticPath[Side::LEFT]},
														{mInput.vibrateAction, hapticPath[Side::RIGHT]}} };
		XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
		suggestedBindings.interactionProfile = indexControllerInteractionProfilePath;
		suggestedBindings.suggestedBindings = bindings.data();
		suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
		CHECK_XRCMD(xrSuggestInteractionProfileBindings(mInstance, &suggestedBindings));
	}

	// Suggest bindings for the Microsoft Mixed Reality Motion Controller.
	{
		XrPath microsoftMixedRealityInteractionProfilePath;
		CHECK_XRCMD(xrStringToPath(mInstance, "/interaction_profiles/microsoft/motion_controller",
			&microsoftMixedRealityInteractionProfilePath));
		std::vector<XrActionSuggestedBinding> bindings{ {{mInput.grabAction, squeezeClickPath[Side::LEFT]},
														{mInput.grabAction, squeezeClickPath[Side::RIGHT]},
														{mInput.poseAction, posePath[Side::LEFT]},
														{mInput.poseAction, posePath[Side::RIGHT]},
														{mInput.quitAction, menuClickPath[Side::LEFT]},
														{mInput.quitAction, menuClickPath[Side::RIGHT]},
														{mInput.vibrateAction, hapticPath[Side::LEFT]},
														{mInput.vibrateAction, hapticPath[Side::RIGHT]}} };
		XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
		suggestedBindings.interactionProfile = microsoftMixedRealityInteractionProfilePath;
		suggestedBindings.suggestedBindings = bindings.data();
		suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
		CHECK_XRCMD(xrSuggestInteractionProfileBindings(mInstance, &suggestedBindings));
	}
	XrActionSpaceCreateInfo actionSpaceInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
	actionSpaceInfo.action = mInput.poseAction;
	actionSpaceInfo.poseInActionSpace.orientation.w = 1.f;
	actionSpaceInfo.subactionPath = mInput.handSubactionPath[Side::LEFT];
	CHECK_XRCMD(xrCreateActionSpace(mSession, &actionSpaceInfo, &mInput.handSpace[Side::LEFT]));
	actionSpaceInfo.subactionPath = mInput.handSubactionPath[Side::RIGHT];
	CHECK_XRCMD(xrCreateActionSpace(mSession, &actionSpaceInfo, &mInput.handSpace[Side::RIGHT]));

	XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
	attachInfo.countActionSets = 1;
	attachInfo.actionSets = &mInput.actionSet;
	CHECK_XRCMD(xrAttachSessionActionSets(mSession, &attachInfo));
}

void cVR::PollActions() 
{
	mInput.handActive = { XR_FALSE, XR_FALSE };

	// Sync actions
	const XrActiveActionSet activeActionSet{ mInput.actionSet, XR_NULL_PATH };
	XrActionsSyncInfo syncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
	syncInfo.countActiveActionSets = 1;
	syncInfo.activeActionSets = &activeActionSet;
	CHECK_XRCMD(xrSyncActions(mSession, &syncInfo));

	// Get pose and grab action state and start haptic vibrate when hand is 90% squeezed.
	for (auto hand : { Side::LEFT, Side::RIGHT }) 
	{
		XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
		getInfo.action = mInput.grabAction;
		getInfo.subactionPath = mInput.handSubactionPath[hand];

		XrActionStateFloat grabValue{ XR_TYPE_ACTION_STATE_FLOAT };
		CHECK_XRCMD(xrGetActionStateFloat(mSession, &getInfo, &grabValue));
		if (grabValue.isActive == XR_TRUE) {
			// Scale the rendered hand by 1.0f (open) to 0.5f (fully squeezed).
			mInput.handScale[hand] = 1.0f - 0.5f * grabValue.currentState;
			if (grabValue.currentState > 0.9f) {
				XrHapticVibration vibration{ XR_TYPE_HAPTIC_VIBRATION };
				vibration.amplitude = 0.5;
				vibration.duration = XR_MIN_HAPTIC_DURATION;
				vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
				std::cout << "Grabbing " << grabValue.currentState << std::endl;

				XrHapticActionInfo hapticActionInfo{ XR_TYPE_HAPTIC_ACTION_INFO };
				hapticActionInfo.action = mInput.vibrateAction;
				hapticActionInfo.subactionPath = mInput.handSubactionPath[hand];
				CHECK_XRCMD(xrApplyHapticFeedback(mSession, &hapticActionInfo, (XrHapticBaseHeader *)&vibration));
			}
		}

		getInfo.action = mInput.poseAction;
		XrActionStatePose poseState{ XR_TYPE_ACTION_STATE_POSE };
		CHECK_XRCMD(xrGetActionStatePose(mSession, &getInfo, &poseState));
		mInput.handActive[hand] = poseState.isActive;
	}

	// There were no subaction paths specified for the quit action, because we don't care which hand did it.
	XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO, nullptr, mInput.jumpAction, XR_NULL_PATH };
	CHECK_XRCMD(xrGetActionStateBoolean(mSession, &getInfo, &mInputValues.jumpValue));
	if ((mInputValues.jumpValue.isActive == XR_TRUE)
		&& (mInputValues.jumpValue.changedSinceLastSync == XR_TRUE)
		&& (mInputValues.jumpValue.currentState == XR_TRUE))
	{
		std::cout << "Jumped" << std::endl;
	}

	getInfo = { XR_TYPE_ACTION_STATE_GET_INFO, nullptr, mInput.quitAction, XR_NULL_PATH };
	XrActionStateBoolean quitValue{ XR_TYPE_ACTION_STATE_BOOLEAN };
	CHECK_XRCMD(xrGetActionStateBoolean(mSession, &getInfo, &quitValue));
	if ((quitValue.isActive == XR_TRUE) && (quitValue.changedSinceLastSync == XR_TRUE) && (quitValue.currentState == XR_TRUE)) 
	{
		std::cout << "Pressed quit" << std::endl;
	}

	getInfo = { XR_TYPE_ACTION_STATE_GET_INFO, nullptr, mInput.moveAction, XR_NULL_PATH };
	XrActionStateVector2f moveValue{ XR_TYPE_ACTION_STATE_VECTOR2F };
	CHECK_XRCMD(xrGetActionStateVector2f(mSession, &getInfo, &moveValue));
	if (moveValue.isActive == XR_TRUE)
	{
		//std::cout << "x: " << moveValue.currentState.x << " y: " << moveValue.currentState.y << std::endl;
	}
}

//void cVR::GetActionState(eLuxAction luxAction)
//{
//	XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO, nullptr, nullptr, XR_NULL_PATH };
//	XrActionStateBoolean jumpValue{ XR_TYPE_ACTION_STATE_BOOLEAN };
//
//	switch (luxAction)
//	{
//	case eLuxAction_Jump:
//		getInfo.action = mInput.jumpAction;
//	}
//
//}