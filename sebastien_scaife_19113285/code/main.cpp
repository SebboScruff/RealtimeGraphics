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


// Global scope variables here, like Window Size and 
const int windowWidth = 800, windowHeight = 600;

const Uint64 desiredFrameTime = 30;


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
		// TODO Define Shader Programs

		// Create a viewer from the glhelper set - WASD movement and mouse rotation
		glhelper::FlyViewer viewer(windowWidth, windowHeight);

		// TODO Define all meshes

		// TODO Set up mesh translation matrices

		// TODO Load in meshes from file extensions, apply shaders & transformations

		// TODO Set shader Uniforms

		// TODO Define and create textures, normals, mipmaps etc for all the models

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

			// Rendering and Draw Calls here!

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