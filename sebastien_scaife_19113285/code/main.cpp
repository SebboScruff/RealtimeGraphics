#define SDL_MAIN_HANDLED
#include <GL/glew.h>
#include <SDL.h>
#include <iostream>
#include <exception>
#include <cmath>
#include "glhelper/ShaderProgram.hpp"
#include "glhelper/RotateViewer.hpp"
#include "glhelper/FlyViewer.hpp"
#include "glhelper/Mesh.hpp"
#include "glhelper/Texture.hpp"
#include "glhelper/Matrices.hpp"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/mesh.h"
#include "assimp/scene.h"
#include <opencv2/opencv.hpp>
#include <SebScaifeCMP7172/NoiseMap.h>
#define GLT_IMPLEMENTATION
#include <gltext.h>
#include <fstream>
#include <iostream>


// Global scope variables here, like Window Size and hardset framerate
const int windowWidth = 800, windowHeight = 600;

const Uint64 desiredFrameTime = 30;
int frameCounter = 0;
int renderIteration = 1;

const std::string assetsDirectory = "../../bin/Assets/";
const std::string queryOutDirectory = "../../bin/Profiling/";

// Main lighting settings here:
float lightTheta = 0.0f;
Eigen::Vector3f lightPosition(0.f, 10.f, 0.f);
float lightIntensity = 60.f;
float testSpecularExponent = -8.f;
// Lighting directions etc. are done locally to each face in shaders where appropriate

// This will eventually control which rendering style is being used: photorealism, or watercolor-style artistic shading, or some other type if I get round to it
int currentRenderMode = 0;

NoiseMap noiseMapMaster;

void loadMesh(glhelper::Mesh* mesh, const std::string& filename)
{
	Assimp::Importer importer;
	importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_GenSmoothNormals);
	const aiScene* aiscene = importer.GetScene();
	const aiMesh* aimesh = aiscene->mMeshes[0];

	std::vector<Eigen::Vector3f> verts(aimesh->mNumVertices);
	std::vector<Eigen::Vector3f> norms(aimesh->mNumVertices);
	std::vector<Eigen::Vector2f> uvs(aimesh->mNumVertices);
	std::vector<GLuint> elems(aimesh->mNumFaces * 3);
	memcpy(verts.data(), aimesh->mVertices, aimesh->mNumVertices * sizeof(aiVector3D));
	memcpy(norms.data(), aimesh->mNormals, aimesh->mNumVertices * sizeof(aiVector3D));
	for (size_t v = 0; v < aimesh->mNumVertices; ++v) {
		uvs[v][0] = aimesh->mTextureCoords[0][v].x;
		uvs[v][1] = 1.f - aimesh->mTextureCoords[0][v].y;
	}
	for (size_t f = 0; f < aimesh->mNumFaces; ++f) {
		for (size_t i = 0; i < 3; ++i) {
			elems[f * 3 + i] = aimesh->mFaces[f].mIndices[i];
		}
	}

	mesh->vert(verts);
	mesh->norm(norms);
	mesh->elems(elems);
	mesh->tex(uvs);
}

void WriteTimeQuery(float _timeMilli, std::string _queriedObjName, std::ofstream& _file)
{
	std::string timeMSStr = std::to_string(_timeMilli);
	_file << "Time taken for " << _queriedObjName << ", " << timeMSStr << "ms \n";
}

float DegToRad(float _deg)
{
	float rad = _deg * (M_PI / 180);
	return rad;
}

// This is being kept here for now, TODO eventually extract this out into its own class
void SetUpTexture(cv::Mat _textureSource, GLuint _texture)
{
	glBindTexture(GL_TEXTURE_2D, _texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8,
		_textureSource.cols, _textureSource.rows,
		0, GL_RGB, GL_UNSIGNED_BYTE, _textureSource.data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateTextureMipmap(_texture);
}

int main()
{
	// INITIALISATIONS
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);

	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);
	// Turns on 4x MSAA
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	// Prepare window
	SDL_Window* window = SDL_CreateWindow("Seb Scaife - CMP7172", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(window);

	GLenum result = glewInit();
	if (result != GLEW_OK) {
		throw std::runtime_error("GLEW couldn't initialize.");
	}
	gltInit();

	glEnable(GL_MULTISAMPLE);

	// Everything runtime-related in here:
	{
		// Profiling and Querying Set Up implemented into glhelper::Renderable.hpp and glhelper::Renderable.cpp

		// TODO Define remaining Shader Programs
		glhelper::ShaderProgram defaultBlueShader({ "../shaders/FixedColor.vert", "../shaders/FixedColor.frag" }); // mainly for checking that meshes are loaded correctly, delete this eventually
		glhelper::ShaderProgram lightSphereShader({ "../shaders/FixedColor.vert", "../shaders/FixedColor.frag" }); // plain white to signify where the light in the scene is
		glhelper::ShaderProgram lambertShader({ "../shaders/Lambert.vert", "../shaders/Lambert.frag" }); // mainly rfor checking that textures are loaded correctly
		glhelper::ShaderProgram blinnPhongShader({ "../shaders/BlinnPhong.vert", "../shaders/BlinnPhong.frag" }); // TODO this needs a few fixes, eventually apply to Sword & Shield models

		// Create a viewer from the glhelper set - WASD movement and mouse rotation
		glhelper::FlyViewer viewer(windowWidth, windowHeight);
		viewer.position(Eigen::Vector3f(-8.3f, -1.0f, 3.f));
		viewer.rotation(4.49f, 0.05f);

		// Define all necessary meshes
		std::vector<glhelper::Renderable*> renderables; // store all renderables in a Vector so that it is easy to write out Render Queries en masse
		glhelper::Mesh lightSphere("light");
		glhelper::Mesh floorMesh("floor"), swordMesh("sword"), shieldMesh("shield"), logSeatMesh1("seat1"), logSeatMesh2("seat2"), campfireBaseMesh("campfireBase");

		// for writing GL Queries
		renderables = { &lightSphere, &floorMesh, &swordMesh, &shieldMesh, &logSeatMesh1, &logSeatMesh2, &campfireBaseMesh };

		// -- Translation Matrix Set-up -- \\

		// Light Sphere
		Eigen::Matrix4f lightSphereM2W = makeTranslationMatrix(lightPosition);

		// Floor
		Eigen::Matrix4f floorModelToWorld = Eigen::Matrix4f::Identity();

		// Sword
		Eigen::Matrix4f swordModelToWorld = Eigen::Matrix4f::Identity();
		swordModelToWorld *= makeTranslationMatrix(Eigen::Vector3f(-1.95f, 0.44f, -2.64f)); 
		swordModelToWorld *= makeRotationMatrix(Eigen::Vector3f(DegToRad(15.4f), DegToRad(59.8f), DegToRad(-0.68f)));
		swordModelToWorld *= makeScaleMatrix(1.59f);

		// Shield
		Eigen::Matrix4f shieldModelToWorld = Eigen::Matrix4f::Identity();
		shieldModelToWorld *= makeTranslationMatrix(Eigen::Vector3f(-1.87f, 0.27f, -1.3f));
		shieldModelToWorld *= makeRotationMatrix(Eigen::Vector3f(DegToRad(-66.8f), DegToRad(61.6f), DegToRad(0.f)));
		shieldModelToWorld *= makeScaleMatrix(3.1f);

		// Campfire Base
		Eigen::Matrix4f campfireModelToWorld = Eigen::Matrix4f::Identity();
		campfireModelToWorld *= makeScaleMatrix(0.6f);
		campfireModelToWorld *= makeTranslationMatrix(Eigen::Vector3f(0.f, -0.15f, 0.f));

		// Seats
		Eigen::Matrix4f seat1ModelToWorld = Eigen::Matrix4f::Identity();

		Eigen::Matrix4f seat2ModelToWorld = Eigen::Matrix4f::Identity();
		// -- End of Translation Matrix Set-up -- \\


		// -- Model Loading -- \\
		// Light Sphere
		loadMesh(&lightSphere, assetsDirectory + "models/sphere.obj");
		lightSphere.modelToWorld(lightSphereM2W);
		lightSphere.shaderProgram(&lightSphereShader);

		// Floor
		loadMesh(&floorMesh, assetsDirectory + "models/fromBlend/floor.obj");
		floorMesh.modelToWorld(floorModelToWorld);
		floorMesh.shaderProgram(&lambertShader);

		// Sword
		loadMesh(&swordMesh, assetsDirectory + "models/sword.obj");
		swordMesh.modelToWorld(swordModelToWorld);
		swordMesh.shaderProgram(&blinnPhongShader);

		// Shield
		loadMesh(&shieldMesh, assetsDirectory + "models/shield.obj");
		shieldMesh.modelToWorld(shieldModelToWorld);
		shieldMesh.shaderProgram(&blinnPhongShader);

		// Seats
		loadMesh(&logSeatMesh1, assetsDirectory + "models/fromBlend/logSeat1.obj");
		logSeatMesh1.modelToWorld(seat1ModelToWorld);
		logSeatMesh1.shaderProgram(&lambertShader);

		loadMesh(&logSeatMesh2, assetsDirectory + "models/fromBlend/logSeat2.obj");
		logSeatMesh2.modelToWorld(seat2ModelToWorld);
		logSeatMesh2.shaderProgram(&lambertShader);

		// Campfire Base
		loadMesh(&campfireBaseMesh, assetsDirectory + "models/fromBlend/campfireBase.obj");
		campfireBaseMesh.modelToWorld(campfireModelToWorld);
		campfireBaseMesh.shaderProgram(&lambertShader);

		// -- End of Model Loading -- \\

		// -- Shader Uniform Setup -- \\ 
		// TODO Set remaining shader Uniforms
		glProgramUniform4f(defaultBlueShader.get(), defaultBlueShader.uniformLoc("color"), 0.f, 1.f, 1.f, 1.f);  // sets the Fixed Color shader to simply render everything a nice turquoise by default
		glProgramUniform4f(lightSphereShader.get(), lightSphereShader.uniformLoc("color"), 1.f, 1.f, 1.f, 1.f); // just set the light to be plain white

		glProgramUniform1i(lambertShader.get(), lambertShader.uniformLoc("albedo"), 0);

		glProgramUniform1i(blinnPhongShader.get(), blinnPhongShader.uniformLoc("albedo"), 0); // NOTE: Albedo Maps bound to Image Unit 0
		glProgramUniform1i(blinnPhongShader.get(), blinnPhongShader.uniformLoc("specularIntensity"), 1); // NOTE: Metallic Maps bound to Image Unit 1
		glProgramUniform1f(blinnPhongShader.get(), blinnPhongShader.uniformLoc("specularExponent"), testSpecularExponent); // Kinda just an arbitrary value for now, feel free to tweak (or set on a per-model basis)
		glProgramUniform3f(blinnPhongShader.get(), blinnPhongShader.uniformLoc("lightPos"), lightPosition.x(), lightPosition.y(), lightPosition.z()); // this will need to be updated if i ever move the light
		glProgramUniform1f(blinnPhongShader.get(), blinnPhongShader.uniformLoc("lightIntensity"), lightIntensity);

		// -- End of Shader Uniform Setup -- \\

		// -- Texture Setup -- \\ 
		// TODO Define and create remaining normal maps, speculars, mipmaps etc for all the models

		// Floor Texture Setup
		cv::Mat floorAlbedoSource = cv::imread(assetsDirectory + "textures/Floor/FloorAlbedo.jpg");
		cv::cvtColor(floorAlbedoSource, floorAlbedoSource, cv::COLOR_BGR2RGB);
		GLuint floorAlbedo;
		glGenTextures(1, &floorAlbedo);
		SetUpTexture(floorAlbedoSource, floorAlbedo);

		// Sword Texture Setup
		cv::Mat swordAlbedoSource = cv::imread(assetsDirectory + "textures/Sword/swordAlbedo.png");
		cv::cvtColor(swordAlbedoSource, swordAlbedoSource, cv::COLOR_BGR2RGB);
		GLuint swordAlbedo;
		glGenTextures(1, &swordAlbedo);
		SetUpTexture(swordAlbedoSource, swordAlbedo);
		// ----
		cv::Mat swordMetallicSource = cv::imread(assetsDirectory + "textures/Sword/swordMetallicSmoothness.png");
		cv::cvtColor(swordMetallicSource, swordMetallicSource, cv::COLOR_BGR2RGB);
		GLuint swordMetallic;
		glGenTextures(1, &swordMetallic);
		SetUpTexture(swordMetallicSource, swordMetallic);

		// Shield Texture Setup
		cv::Mat shieldAlbedoSource = cv::imread(assetsDirectory + "textures/Shield/shieldAlbedo.png");
		cv::cvtColor(shieldAlbedoSource, shieldAlbedoSource, cv::COLOR_BGR2RGB);
		GLuint shieldAlbedo;
		glGenTextures(1, &shieldAlbedo);
		SetUpTexture(shieldAlbedoSource, shieldAlbedo);
		// ----
		cv::Mat shieldMetallicSource = cv::imread(assetsDirectory + "textures/Shield/shieldMetallic.png");
		cv::cvtColor(shieldMetallicSource, shieldMetallicSource, cv::COLOR_BGR2RGB);
		GLuint shieldMetallic;
		glGenTextures(1, &shieldMetallic);
		SetUpTexture(shieldMetallicSource, shieldMetallic);

		// Log Seat Texture Setup
		cv::Mat logAlbedoSource = cv::imread(assetsDirectory + "textures/LogSeat/logAlbedo.jpg");
		cv::cvtColor(logAlbedoSource, logAlbedoSource, cv::COLOR_BGR2RGB);
		GLuint logAlbedo;
		glGenTextures(1, &logAlbedo);
		SetUpTexture(logAlbedoSource, logAlbedo);

		// Campfire Texture Setup
		cv::Mat campfireAlbedoSource = cv::imread(assetsDirectory + "textures/Campfire/campfireBaseAlbedo.jpg");
		cv::cvtColor(campfireAlbedoSource, campfireAlbedoSource, cv::COLOR_BGR2RGB);
		GLuint campfireAlbedo;
		glGenTextures(1, &campfireAlbedo);
		SetUpTexture(campfireAlbedoSource, campfireAlbedo);

		// -- End of Texture Setup -- \\

		// Enable any GL functions here:
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// -- Input and controls -- \\

		bool shouldQuit = false;
		SDL_Event event;

		std::ofstream profilerOut(queryOutDirectory + "profiling.csv");

		noiseMapMaster.GenerateGaussianMap(windowWidth, windowHeight, 4, 0, 180, true);
		noiseMapMaster.GenerateSimplexMap(windowWidth, windowHeight, 0.01f, true);

		while (!shouldQuit) {
			Uint64 frameStartTime = SDL_GetTicks64(); // for capping framerate to the previously defined maximum

			viewer.update();

			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT) { // SDL_QUIT is either closing the window or Alt+F4-ing
					shouldQuit = true; // break out of the application at the end of this frame
				}
				else {
					viewer.processEvent(event);
				}

				if (event.type == SDL_KEYDOWN) {
					// Change rendering style using number commands
					if (event.key.keysym.sym == SDLK_1) {
						// Standard photorealism

						// Sword and shield use Blinn-Phong
						// Floor, Logs, Trees use Lambert with Normalmaps
					}
					else if (event.key.keysym.sym == SDLK_2) {
						//  TODO Watercolor custom renderer

						// Everything uses Watercolor shader
					}
					else if (event.key.keysym.sym == SDLK_UP) {
						// Controls for debugging/setting correct material properties
						lightIntensity += 1.f;
						std::cout << "Light Intensity = " << lightIntensity << std::endl;
						glProgramUniform1f(blinnPhongShader.get(), blinnPhongShader.uniformLoc("lightIntensity"), lightIntensity);
					}
					else if (event.key.keysym.sym == SDLK_DOWN) {
						// Controls for debugging/setting correct material properties
						lightIntensity -= 1.f;
						std::cout << "Light Intensity = " << lightIntensity << std::endl;
						glProgramUniform1f(blinnPhongShader.get(), blinnPhongShader.uniformLoc("lightIntensity"), lightIntensity);
					}
				}
			}
			// -- End of Input and Controls -- \\ 
			glClearColor(0.1f, 0.1f, 0.1f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glDisable(GL_CULL_FACE);

			// -- Texture Binding, Rendering, and Draw Calls -- \\ 

			lightSphere.renderWithQuery();

			// TODO Remaining texture binds to correct image units, to set up Shader Uniforms in the correct way
			// Will need to set Normal Maps, Speculars, etc etc to correct image units before rendering each object
			// This might also be extracted out into a separate function e.g. BindTexsAndRender()

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, floorAlbedo);

			floorMesh.renderWithQuery();

			// ----

			glActiveTexture(GL_TEXTURE0 + 0); // Bind Albedo to Image Unit 0
			glBindTexture(GL_TEXTURE_2D, swordAlbedo);
			glActiveTexture(GL_TEXTURE0 + 1); // Bind Metallic to Image Unit 1
			glBindTexture(GL_TEXTURE_2D, swordMetallic);

			swordMesh.renderWithQuery();

			// ----

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, shieldAlbedo);
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_2D, shieldMetallic);

			shieldMesh.renderWithQuery();

			// ----

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, logAlbedo);

			logSeatMesh1.renderWithQuery();
			logSeatMesh2.renderWithQuery();

			// ----

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, campfireAlbedo);

			campfireBaseMesh.renderWithQuery();

			// ----


			// If rendering text, do it here 0 e.g. 
			// gltBeginDraw();
			// gltColor(1.f, 1.f, 1.f, 1.f);
			// gltDrawText2D(text, 10.f, 10.f, 1.f);
			// gltEndDraw();

			// -- End of Texture Binding, Rendering and Draw Calls -- \\

			SDL_GL_SwapWindow(window);

			Uint64 elapsedFrameTime = SDL_GetTicks64() - frameStartTime; // Part 2 of Capping Framerate
			if (elapsedFrameTime < desiredFrameTime) {
				SDL_Delay(desiredFrameTime - elapsedFrameTime);
			}

			if (++frameCounter >= 30) {

				// write to csv
				profilerOut << "Render Iteration, " << renderIteration << "\n";
				for (glhelper::Renderable* r : renderables) {
					WriteTimeQuery(r->getRenderTime(), r->name, profilerOut);
				}

				frameCounter = 0;
				renderIteration++;
				//SDL_SetWindowTitle(window, "Render Iteration: " + (char)renderIteration);
			}
		}
		// Clean up

		glDeleteTextures(1, &swordAlbedo);
		// TODO DELETE ANY OTHER SWORD TEXTURES E.G. METALLIC

		glDeleteTextures(1, &shieldAlbedo);
		// TODO DELETE ANY OTHER SWORD TEXTURES E.G. METALLIC

		glDeleteTextures(1, &logAlbedo);
		// TODO DELETE ANY OTHER SWORD TEXTURES E.G. NORMAL

		glDeleteTextures(1, &campfireAlbedo);
		// TODO DELETE ANY OTHER CAMPFIRE TEXTURES E.G. PARTICLES
	}


	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}