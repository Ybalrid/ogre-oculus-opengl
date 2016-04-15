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
#include <RenderSystems/GL/OgreGLTextureManager.h>
#include <RenderSystems/GL/OgreGLRenderSystem.h>
#include <RenderSystems/GL/OgreGLTexture.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <Extras/OVR_Math.h>


enum eyes{left, right, nbEyes};

mainFunc()
{
	//Create Root object
	Ogre::Root* root = new Ogre::Root("plugin.cfg", "ogre.cfg");
	//opengl
	root->loadPlugin("RenderSystem_GL");//1
	root->setRenderSystem(root->getRenderSystemByName("OpenGL Rendering Subsystem"));

	//Initialize Root
	root->initialise(false);

	//initialize oculus
	ovrSession session;
	ovrHmdDesc hmdDesc;
	ovrGraphicsLuid luid;
	ovr_Initialize(nullptr);//2
	if(ovr_Create(&session, &luid) != ovrSuccess)
		exit(-1);
	hmdDesc = ovr_GetHmdDesc(session);//3

	Ogre::NameValuePairList misc;
	misc["top"] = "0";
	misc["left"] = "0";

	Ogre::ResourceGroupManager::getSingleton().addResourceLocation("media", "FileSystem");
	Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

	//create a window
	Ogre::RenderWindow* window = root->createRenderWindow("Oculus Test", 
		hmdDesc.Resolution.w/2, hmdDesc.Resolution.h/2, false, &misc);

	//Create scene manager and cameras
	Ogre::SceneManager* smgr = root->createSceneManager(Ogre::ST_GENERIC);
	auto sinbadEntity = smgr->createEntity("Sinbad.mesh");
	auto Sinbad = smgr->getRootSceneNode()->createChildSceneNode();
	Sinbad->attachObject(sinbadEntity);
	Sinbad->setPosition(Ogre::Vector3(0,0,-7));
	smgr->setAmbientLight(Ogre::ColourValue(1,1,1));

	ovr_RecenterTrackingOrigin(session);

	Ogre::Camera* cams[nbEyes];
	cams[left] = smgr->createCamera("leftcam");
	cams[right] = smgr->createCamera("rightcam");

	float X(16), Y(9);
	float x(X/2), y(Y/2);

	Ogre::SceneManager* debugSmgr = root->createSceneManager(Ogre::ST_GENERIC);
	debugSmgr->setAmbientLight(Ogre::ColourValue::White);

	Ogre::Camera* debugCam = debugSmgr->createCamera("debugcam");
	debugCam->setAutoAspectRatio(true);
	debugCam->setNearClipDistance(0.001f);

	Ogre::SceneNode* debugCamNode = debugSmgr->getRootSceneNode()->createChildSceneNode();
	debugCamNode->attachObject(debugCam);
	debugCamNode->setPosition(Ogre::Vector3(0,0,10));
	//debugCam->lookAt(Ogre::Vector3(0,0,0));

	debugCam->setProjectionType(Ogre::ProjectionType::PT_ORTHOGRAPHIC);
	debugCam->setOrthoWindow(X,Y);

	Ogre::MaterialPtr DebugPlaneMaterial = Ogre::MaterialManager::getSingleton().create("debugMat", "General", true);
	Ogre::TextureUnitState* debugTexturePlane = DebugPlaneMaterial.getPointer()->getTechnique(0)->getPass(0)->createTextureUnitState();

	Ogre::ManualObject* debugRenderPlane = debugSmgr->createManualObject("debugplane");
	debugRenderPlane->begin("debugMat",Ogre::RenderOperation::OT_TRIANGLE_STRIP);
	debugRenderPlane->position(-x, y, 0);
	debugRenderPlane->textureCoord(0, 0);
	debugRenderPlane->position(-x, -y,0);
	debugRenderPlane->textureCoord(0, 1);
	debugRenderPlane->position(x, y,  0);
	debugRenderPlane->textureCoord(1, 0);
	debugRenderPlane->position(x, -y, 0);
	debugRenderPlane->textureCoord(1, 1);
	debugRenderPlane->end();

	debugSmgr->getRootSceneNode()->createChildSceneNode()->attachObject(debugRenderPlane);

	Ogre::Viewport* debugViewport = window->addViewport(debugCam);
	debugViewport->setBackgroundColour(Ogre::ColourValue::Green);

	//init glew
	if(glewInit() != GLEW_OK)
		exit(-3);

	//get texture sizes
	ovrSizei texSizeL, texSizeR;
	texSizeL = ovr_GetFovTextureSize(session, ovrEye_Left, hmdDesc.DefaultEyeFov[left], 1);
	texSizeR = ovr_GetFovTextureSize(session, ovrEye_Right, hmdDesc.DefaultEyeFov[right], 1);
	//calculate render buffer size
	ovrSizei bufferSize;
	bufferSize.w = texSizeL.w + texSizeR.w;
	bufferSize.h = std::max(texSizeL.h, texSizeR.h);

	//Set texture parameters
	ovrTextureSwapChainDesc textureSwapChainDesc = {};
	textureSwapChainDesc.Type = ovrTexture_2D;
	textureSwapChainDesc.ArraySize = 1;
	textureSwapChainDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	textureSwapChainDesc.Width = bufferSize.w;
	textureSwapChainDesc.Height = bufferSize.h;
	textureSwapChainDesc.MipLevels = 1;
	textureSwapChainDesc.SampleCount = 1;
	textureSwapChainDesc.StaticImage = ovrFalse;

	//create render texture set
	ovrTextureSwapChain textureSwapChain;
	if(ovr_CreateTextureSwapChainGL(session, &textureSwapChainDesc, &textureSwapChain) != ovrSuccess)
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
	vpts[left]->setBackgroundColour(Ogre::ColourValue::White);
	vpts[right]->setBackgroundColour(Ogre::ColourValue::White);

	//Fill in MirrorTexture parameters
	ovrMirrorTextureDesc mirrorTextureDesc = {};
	mirrorTextureDesc.Width = hmdDesc.Resolution.w;
	mirrorTextureDesc.Height = hmdDesc.Resolution.h;
	mirrorTextureDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;	

	ovrMirrorTexture mirrorTexture;
	if (ovr_CreateMirrorTextureGL(session, &mirrorTextureDesc, &mirrorTexture) != ovrSuccess)
		exit(-5);
	Ogre::TexturePtr mirror_texture(textureManager->createManual("MirrorTex", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
		Ogre::TEX_TYPE_2D, hmdDesc.Resolution.w, hmdDesc.Resolution.h, 0, Ogre::PF_R8G8B8, Ogre::TU_RENDERTARGET));

	//get GLIDs
	GLuint ogreMirrorTextureID = static_cast<Ogre::GLTexture*>(Ogre::GLTextureManager::getSingleton().getByName("MirrorTex").getPointer())->getGLID();
	GLuint oculusMirrorTextureID ;

	ovr_GetMirrorTextureBufferGL(session, mirrorTexture, &oculusMirrorTextureID);

	//Create EyeRenderDesc
	ovrEyeRenderDesc EyeRenderDesc[nbEyes];
	EyeRenderDesc[left] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[left]);
	EyeRenderDesc[right] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[right]);

	//Get offsets
	ovrVector3f offset[nbEyes];
	offset[left]=EyeRenderDesc[left].HmdToEyeOffset;
	offset[right]=EyeRenderDesc[right].HmdToEyeOffset;

	//Compositor layer
	ovrLayerEyeFov layer;
	//Create a layer with our single swaptexture on it. Each side is an eye.
	layer.Header.Type = ovrLayerType_EyeFov;
	layer.Header.Flags = 0;
	layer.ColorTexture[left]  = textureSwapChain;
	layer.ColorTexture[right] = textureSwapChain;
	layer.Fov[left]  = EyeRenderDesc[left].Fov;
	layer.Fov[right] = EyeRenderDesc[right].Fov;

	//Define the two viewports : 
	ovrRecti leftRect, rightRect;
	leftRect.Size = bufferSize;			//same size than the buffer
	leftRect.Size.w /= 2;				//but half the width
	rightRect = leftRect;				//The two rect are of the same size, but not at the same position
	ovrVector2i leftPos, rightPos;		
	leftPos.x = 0;						//The left one start at the bottom left corner
	leftPos.y = 0;
	rightPos = leftPos;
	rightPos.x = bufferSize.w/2;		//But the right start at half the buffer width
	leftRect.Pos = leftPos;				
	rightRect.Pos = rightPos;

	//Assign the viewports
	layer.Viewport[left]  = leftRect;
	layer.Viewport[right] = rightRect;

	//Get projection matrices
	for(size_t eyeIndex(0); eyeIndex < ovrEye_Count; eyeIndex++)
	{
		//Get the projection matrix
		ovrMatrix4f proj = ovrMatrix4f_Projection(EyeRenderDesc[eyeIndex].Fov, 
			static_cast<float>(0.01f), 
			4000, 
			0);

		//Convert it to Ogre matrix
		Ogre::Matrix4 OgreProj;
		for(size_t x(0); x < 4; x++)
			for(size_t y(0); y < 4; y++)
				OgreProj[x][y] = proj.M[x][y];

		//Set the matrix
		cams[eyeIndex]->setCustomProjectionMatrix(true, OgreProj);
	}

	ovr_SetInt(session, "PerfHudMode", ovrPerfHud_CompRenderTiming);

	bool render(true);
	Ogre::Vector3 cameraPosition;
	Ogre::Quaternion cameraOrientation;
	ovrTrackingState ts;
	OVR::Posef pose;
	OVR::Quatf oculusOrient;
	OVR::Vector3f oculusPos;
	ovrLayerHeader* layers;
	double currentFrameDisplayTime;
	GLuint oculusRenderTextureGLID;

	int currentIndex;

	debugTexturePlane->setTextureName("MirrorTex");
	debugTexturePlane->setTextureFiltering(Ogre::FO_POINT, Ogre::FO_POINT, Ogre::FO_NONE);
	debugViewport->setAutoUpdated(false);

	//rgm->initialiseAllResourceGroups();
	//	smgr->createEntity("Sinbad.mesh");

	while(render)
	{
		Ogre::WindowEventUtilities::messagePump();
		//advance textureset index
		ovr_GetTextureSwapChainCurrentIndex(session, textureSwapChain, &currentIndex);
		ovr_GetTextureSwapChainBufferGL(session, textureSwapChain, currentIndex, &oculusRenderTextureGLID);
		ovr_GetMirrorTextureBufferGL(session, mirrorTexture, &oculusMirrorTextureID);

		//Get the tracking state 
		ts = ovr_GetTrackingState(session, 
			currentFrameDisplayTime = ovr_GetPredictedDisplayTime(session, 0),
			ovrTrue);

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
				EyeRenderDesc[eye].HmdToEyeOffset.x, //view adjust vector.
				EyeRenderDesc[eye].HmdToEyeOffset.y, //The translations has to occur in function of the current head orientation.
				EyeRenderDesc[eye].HmdToEyeOffset.z) //That's why just multiply by the quaternion we just calculated. 

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
			oculusRenderTextureGLID, GL_TEXTURE_2D, 0, 0, 0, 0, 
			bufferSize.w,bufferSize.h, 1);

		layers = &layer.Header;
		ovr_CommitTextureSwapChain(session, textureSwapChain);
		ovr_SubmitFrame(session, 0, nullptr, &layers, 1);

		//Put the mirrored view available for OGRE
		glCopyImageSubData(oculusMirrorTextureID, GL_TEXTURE_2D, 0, 0, 0, 0, 
			ogreMirrorTextureID, GL_TEXTURE_2D, 0, 0, 0, 0, 
			hmdDesc.Resolution.w, hmdDesc.Resolution.h, 1);

		debugViewport->update();
		window->update();
		if(window->isClosed()) render = false;
	}

	ovr_SetInt(session, "PerfHudMode", ovrPerfHud_Off);
	ovr_Shutdown();


	DebugPlaneMaterial.getPointer()->getTechnique(0)->getPass(0)->removeAllTextureUnitStates();

	delete root;
	return EXIT_SUCCESS;
}