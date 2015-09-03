#ifdef _WIN32
#define mainFunc() int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,LPSTR lpCmdLine, int nCmdShow)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <glew.h>
#else
#define mainFunc() int main(void)
#endif
#include <iostream>
#include <Ogre.h>
#include <OVR.h>
#include <RenderSystems/GL/OgreGLTextureManager.h>
#include <RenderSystems/GL/OgreGLRenderSystem.h>
#include <RenderSystems/GL/OgreGLTexture.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <OVR_CAPI_0_7_0.h>

enum eyes{left, right, nbEyes};

int max(int a, int b)
{
	if (a > b) return a; return b;
}

mainFunc()
{
	//Create Root object
	Ogre::Root* root = new Ogre::Root("plugin.cfg", "ogre.cfg");
	//opengl
    root->loadPlugin("RenderSystem_GL");
    root->setRenderSystem(root->getRenderSystemByName("OpenGL Rendering Subsystem"));

	//Initialize Root
	root->initialise(false);

	//initialize oculus
	ovrHmd hmd;
	ovrHmdDesc hmdDesc;
	ovrGraphicsLuid luid;
	ovr_Initialize(nullptr);
	if(ovr_Create(&hmd, &luid) != ovrSuccess)
		exit(-1);
	hmdDesc = ovr_GetHmdDesc(hmd);
	if(ovr_ConfigureTracking(hmd,
		ovrTrackingCap_Orientation |ovrTrackingCap_MagYawCorrection |ovrTrackingCap_Position,
		0) != ovrSuccess)
		exit(-2);

	//create a window
	Ogre::RenderWindow* window = root->createRenderWindow("Oculus Test", 
		hmdDesc.Resolution.w/2, hmdDesc.Resolution.h/2, false);

	//Create scene manager and cameras
	Ogre::SceneManager* smgr = root->createSceneManager(Ogre::ST_GENERIC);
	Ogre::Camera* cams[nbEyes];
	cams[left] = smgr->createCamera("leftcam");
	cams[right] = smgr->createCamera("rightcam");

	//init glew
	if(glewInit() != GLEW_OK)
		exit(-3);

	//get texture sizes
	ovrSizei texSizeL, texSizeR;
	texSizeL = ovr_GetFovTextureSize(hmd, ovrEye_Left, hmdDesc.DefaultEyeFov[left], 1);
	texSizeR = ovr_GetFovTextureSize(hmd, ovrEye_Right, hmdDesc.DefaultEyeFov[right], 1);
	//calculate render buffer size
	ovrSizei bufferSize;
	bufferSize.w = texSizeL.w + texSizeR.w;
	bufferSize.h = max(texSizeL.h, texSizeR.h);

	//create render texture set
	ovrSwapTextureSet* textureSet;
	if(ovr_CreateSwapTextureSetGL(hmd, GL_RGB, bufferSize.w, bufferSize.h, &textureSet) != ovrSuccess)
		exit(-4);

	//create ogre render texture
	Ogre::GLTextureManager* textureManager = static_cast<Ogre::GLTextureManager*>(Ogre::GLTextureManager::getSingletonPtr());
	Ogre::TexturePtr rtt_texture(textureManager->createManual("RttTex", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
		Ogre::TEX_TYPE_2D, bufferSize.w, bufferSize.h, 0, Ogre::PF_R8G8B8, Ogre::TU_RENDERTARGET));
	Ogre::RenderTexture* rttEyes = rtt_texture->getBuffer(0, 0)->getRenderTarget();
	Ogre::GLTexture* gltex = static_cast<Ogre::GLTexture*>(Ogre::GLTextureManager::getSingleton().getByName("RttTex").getPointer());
	GLuint renderTextureID = gltex->getGLID();
	//put camera viewport on the ogre render texture
	Ogre::Viewport* vpts[nbEyes];
	vpts[left]=rttEyes->addViewport(cams[left], 0, 0, 0, 0.5f);
	vpts[right]=rttEyes->addViewport(cams[right], 1, 0.5f, 0, 0.5f);
	vpts[left]->setBackgroundColour(Ogre::ColourValue::Blue);
	vpts[right]->setBackgroundColour(Ogre::ColourValue::Blue);
	
	ovrTexture* mirrorTexture;
	if(ovr_CreateMirrorTextureGL(hmd, GL_RGB, hmdDesc.Resolution.w, hmdDesc.Resolution.h, &mirrorTexture) != ovrSuccess)
		exit(-5);
	Ogre::TexturePtr mirror_texture(textureManager->createManual("MirrorTex", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
		Ogre::TEX_TYPE_2D, hmdDesc.Resolution.w, hmdDesc.Resolution.h, 0, Ogre::PF_R8G8B8, Ogre::TU_RENDERTARGET));

	//get GLIDs
	GLuint ogreMirrorTextureID = static_cast<Ogre::GLTexture*>(Ogre::GLTextureManager::getSingleton().getByName("MirrorTex").getPointer())->getGLID();
	GLuint oculusMirrorTextureID = ((ovrGLTexture*)mirrorTexture)->OGL.TexId;

	//Create EyeRenderDesc
	ovrEyeRenderDesc EyeRenderDesc[nbEyes];
	EyeRenderDesc[left] = ovr_GetRenderDesc(hmd, ovrEye_Left, hmdDesc.DefaultEyeFov[left]);
	EyeRenderDesc[right] = ovr_GetRenderDesc(hmd, ovrEye_Right, hmdDesc.DefaultEyeFov[right]);
	//Get offsets
	ovrVector3f offset[nbEyes];
	offset[left]=EyeRenderDesc[left].HmdToEyeViewOffset;
	offset[right]=EyeRenderDesc[right].HmdToEyeViewOffset;

	//Compositor layer
	ovrLayerEyeFov layer;
	layer.Header.Type = ovrLayerType_EyeFov;
	layer.Header.Flags = 0;
	layer.ColorTexture[left] = textureSet;
	layer.ColorTexture[right] = textureSet;
	layer.Fov[left] = EyeRenderDesc[left].Fov;
	layer.Fov[right] = EyeRenderDesc[right].Fov;
	layer.Viewport[left] = OVR::Recti(0, 0, bufferSize.w/2, bufferSize.h);
	layer.Viewport[right] = OVR::Recti(bufferSize.w/2, 0, bufferSize.w/2, bufferSize.h);

	//Get projection matrices
	for(size_t eyeIndex(0); eyeIndex < ovrEye_Count; eyeIndex++)
	{
		//Get the projection matrix
		OVR::Matrix4f proj = ovrMatrix4f_Projection(EyeRenderDesc[eyeIndex].Fov, 
			static_cast<float>(0.01f), 
			4000, 
			true);

		//Convert it to Ogre matrix
		Ogre::Matrix4 OgreProj;
		for(size_t x(0); x < 4; x++)
			for(size_t y(0); y < 4; y++)
				OgreProj[x][y] = proj.M[x][y];

		//Set the matrix
		cams[eyeIndex]->setCustomProjectionMatrix(true, OgreProj);
	}

	ovr_SetInt(hmd, "PerfHudMode", ovrPerfHud_RenderTiming);

	bool render(true);
	Ogre::Vector3 cameraPosition;
	Ogre::Quaternion cameraOrientation;
	ovrFrameTiming hmdFrameTiming;
	ovrTrackingState ts;
	OVR::Posef pose;
	OVR::Quatf oculusOrient;
	OVR::Vector3f oculusPos;
	ovrLayerHeader* layers;

	while(render)
	{
		Ogre::WindowEventUtilities::messagePump();
		//advance textureset index
		textureSet->CurrentIndex = (textureSet->CurrentIndex + 1) % textureSet->TextureCount;

		hmdFrameTiming = ovr_GetFrameTiming(hmd, 0);
		ts = ovr_GetTrackingState(hmd, hmdFrameTiming.DisplayMidpointSeconds);
		pose = ts.HeadPose.ThePose;
		ovr_CalcEyePoses(pose, offset, layer.RenderPose);
		oculusOrient = pose.Rotation;
		oculusPos = pose.Translation;

		for(size_t eye(0); eye < nbEyes; eye++)
		{
			cams[eye]->setOrientation(cameraOrientation * Ogre::Quaternion(oculusOrient.w, oculusOrient.x, oculusOrient.y, oculusOrient.z));
			cams[eye]->setPosition
				(cameraPosition  //the "gameplay" position of player's avatar head

			+ (cams[eye]->getOrientation() * Ogre::Vector3( //realword camera orientation + the  
			EyeRenderDesc[eye].HmdToEyeViewOffset.x, //view adjust vector.
			EyeRenderDesc[eye].HmdToEyeViewOffset.y, //The translations has to occur in function of the current head orientation.
			EyeRenderDesc[eye].HmdToEyeViewOffset.z) //That's why just multiply by the quaternion we just calculated. 

			+ cameraOrientation * Ogre::Vector3( //cameraOrientation is in fact the direction the avatar is facing expressed as an Ogre::Quaternion
			oculusPos.x,
			oculusPos.y,
			oculusPos.z)));
		}

		root->_fireFrameRenderingQueued();
		vpts[left]->update();
		vpts[right]->update();

		//Copy the rendered image to the Oculus Swap Texture
		glCopyImageSubData(renderTextureID, GL_TEXTURE_2D, 0, 0, 0, 0, 
		((ovrGLTexture*)(&textureSet->Textures[textureSet->CurrentIndex]))->OGL.TexId, GL_TEXTURE_2D, 0, 0, 0, 0, 
		bufferSize.w,bufferSize.h, 1);

		layers = &layer.Header;
		ovr_SubmitFrame(hmd, 0, nullptr, &layers, 1);

		//Put the mirrored view available for OGRE
		/*glCopyImageSubData(oculusMirrorTextureID, GL_TEXTURE_2D, 0, 0, 0, 0, 
		ogreMirrorTextureID, GL_TEXTURE_2D, 0, 0, 0, 0, 
		hmdDesc.Resolution.w, hmdDesc.Resolution.h, 1);*/

		window->update();
		if(window->isClosed()) render = false;
	}

	ovr_Destroy(hmd);
	ovr_Shutdown();

	delete root;
	return EXIT_SUCCESS;
}