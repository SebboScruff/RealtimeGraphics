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
#define GLT_IMPLEMENTATION
#include <gltext.h>


// Global scope variables here, like Window Size and hardset framerate
const int windowWidth = 800, windowHeight = 600;

const Uint64 desiredFrameTime = 30;

const std::string assetsDirectory = "../../bin/Assets/";

// Main lighting settings here:
float lightTheta = 0.0f;
Eigen::Vector3f lightPosition(5.f * sinf(lightTheta), 5.f, 5.f*cosf(lightTheta));
float lightIntensity = 60.f;
// Lighting directions etc. are done locally to each face in shaders where appropriate

// This will eventually control which rendering style is being used: photorealism, or watercolor-style artistic shading, or some other type if I get round to it
int currentRenderMode = 0;

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
		// TODO Define remaining Shader Programs
		glhelper::ShaderProgram fixedColorShader({ "../shaders/FixedColor.vert", "../shaders/FixedColor.frag"});
		//glhelper::ShaderProgram blinnPhongShader({ "../shaders/BlinnPhong.vert", "../shaders/BlinnPhong.frag" });

		// Create a viewer from the glhelper set - WASD movement and mouse rotation
		glhelper::FlyViewer viewer(windowWidth, windowHeight);

		// TODO Define the rest of the meshes
		glhelper::Mesh swordMesh;

		// TODO Set up mesh translation matrices
		Eigen::Matrix4f swordModelToWorld = Eigen::Matrix4f::Identity();
		swordModelToWorld = makeTranslationMatrix(Eigen::Vector3f(0.f, 0.f, 0.f));

		// TODO Load in remaining meshes from file extensions, apply shaders & transformations
		loadMesh(&swordMesh, assetsDirectory + "models/sword.obj");
		swordMesh.modelToWorld(swordModelToWorld);
		swordMesh.shaderProgram(&fixedColorShader);

		// TODO Set remaining shader Uniforms
		glProgramUniform4f(fixedColorShader.get(), fixedColorShader.uniformLoc("color"), 0.f, 1.f, 1.f, 1.f);  // sets the Fixed Color shader to simply render everything a nice turquoise by default

		// TODO Define and create remaining textures, normalmaps, mipmaps etc for all the models
		/*cv::Mat swordTextureImage = cv::imread(assetsDirectory + "textures/Sword/swordAlbedo.png");
		cv::cvtColor(swordTextureImage, swordTextureImage, cv::COLOR_BGR2RGB);
		glhelper::Texture swordTexture(
			GL_TEXTURE_2D, GL_RGB8,
			swordTextureImage.cols, swordTextureImage.rows,
			0, GL_RGB, GL_UNSIGNED_BYTE, swordTextureImage.data,
			GL_LINEAR_MIPMAP_LINEAR,
			GL_LINEAR);
		swordTexture.genMipmap();*/

		// Enable any GL functions here:
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Input and controls
		bool shouldQuit = false;
		SDL_Event event;

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
					}
					else if (event.key.keysym.sym == SDLK_2) {
						//  TODO Watercolor custom renderer
					}
					else if (event.key.keysym.sym == SDLK_3) {
						//  EXTENSION Pencil Sketch custom renderer
					}
					else if (event.key.keysym.sym == SDLK_4) {
						//  EXTENSION Cel Shader
					}
				}
			}
			// End of Input and Controls
			glClearColor(0.1f, 0.1f, 0.1f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Rendering and Draw Calls here!
			glDisable(GL_CULL_FACE);
			
			//TODO Texture Binding
			//swordTexture.bindToImageUnit(0);
			swordMesh.render();

			// If rendering text, do it here 0 e.g. 
			// gltBeginDraw();
			// gltColor(1.f, 1.f, 1.f, 1.f);
			// gltDrawText2D(text, 10.f, 10.f, 1.f);
			// gltEndDraw();

			SDL_GL_SwapWindow(window);

			Uint64 elapsedFrameTime = SDL_GetTicks64() - frameStartTime; // Part 2 of Capping Framerate
			if (elapsedFrameTime < desiredFrameTime) {
				SDL_Delay(desiredFrameTime - elapsedFrameTime);
			}
		}

	}

	// Clean-Up
	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}