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
#include <fstream>
#include <iostream>
#include <bullet/btBulletDynamicsCommon.h>
#include <SebScaifeCMP7172/NoiseMap.h>
#include <SebScaifeCMP7172/BulletHelper.h>
#include <SebScaifeCMP7172/FireParticles.h>
#define GLT_IMPLEMENTATION
#include <gltext.h>


// Global scope variables here, like Window Size and hardset framerate
const int windowWidth = 800, windowHeight = 600;

const Uint64 desiredFrameTime = 30; // hardset to 30 fps
int frameCounter = 0; // only run profiling on every N frames, to save processing power
int renderIteration = 1; // used in dividing up the csv profiling for ease of reading

const std::string assetsDirectory = "../../bin/Assets/";
const std::string queryOutDirectory = "../../bin/Profiling/";

// Main lighting settings here:
// NOTE: Lighting directions etc. are done locally to each fragment in shaders where appropriate
// float lightTheta = 0.0f; // would be used to move a light around a circular dolly path
Eigen::Vector3f lightPosition(0.f, 10.f, -4.f);
float lightIntensity = 60.f;
float specularExponent = -8.f;

// [TODO Probably extract those out and turn them into a 'Light' Class, so that it's easier to read]
// 
// Shadowmapping Settings
const int shadowCubemapSize = 512; // resolution of shadow cubemap sections
const float shadowMapNear = 0.5f, shadowMapFar = 100.f; // clipping planes for shadow mapping perspective
const float shadowMapBias = 0.7f;

// returns a rotation matrix about a specific axis [UNUSED]
Eigen::Matrix4f angleAxisMat4(float angle, const Eigen::Vector3f& axis)
{
	Eigen::Matrix4f output = Eigen::Matrix4f::Identity();
	output.block<3, 3>(0, 0) = Eigen::AngleAxisf(angle, axis).matrix();
	return output;
}

// Standard Cubemap rotations for the shadowmapping shader
const std::array<Eigen::Matrix4f, 6> cubemapRotations{
			angleAxisMat4(float(M_PI_2), Eigen::Vector3f(0,1,0)),//POSITIVE_X - rotate right 90 degrees
			angleAxisMat4(float(-M_PI_2), Eigen::Vector3f(0,1,0)),//NEGATIVE_X - rotate left 90 degrees
			angleAxisMat4(float(-M_PI_2), Eigen::Vector3f(1,0,0)) * angleAxisMat4(float(M_PI), Eigen::Vector3f(0,1,0)),//POSITIVE_Y - rotate up 90 degrees
			angleAxisMat4(float(M_PI_2), Eigen::Vector3f(1,0,0)) * angleAxisMat4(float(M_PI), Eigen::Vector3f(0,1,0)),//NEGATIVE_Y - rotate down 90 degrees
			angleAxisMat4(float(M_PI), Eigen::Vector3f(0,1,0)),     //POSITIVE_Z - rotate right 180 degrees
			Eigen::Matrix4f::Identity()                           //NEGATIVE_Z - unchanged
}; // pre-defined rotation matrices for each face of a cube

// This controls which rendering style is being used:
// 1 = non-stylised/realistic, 2 = Watercolour Shader, 3 = Standard->InvertedColour oscillation
// Most of the Independently Researched Watercolour Rendering Style is done through post-processing!
int currentRenderMode = 1;

NoiseMap noiseMapMaster; // Noisemap Generator for helping with the Watercolor Shader
BulletHelper btHelper; // Contains a few helper functions relating to Bullet and Physics

// Basic Model File -> In-Scene Object loader function
// Only stores verts, normals, and uvs; does not store tangents or bitangents
void loadMeshStandard(glhelper::Mesh* mesh, const std::string& filename)
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

// Separate this into a different function because not every model uses normal mapping
// so I don't want to unnecessarily load thousands and thousands of tangents and bitans if I'm not going to use them!
void loadMeshForNormalMapping(glhelper::Mesh* mesh, const std::string& filename)
{
	Assimp::Importer importer;
	importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);
	const aiScene* aiscene = importer.GetScene();
	const aiMesh* aimesh = aiscene->mMeshes[0];

	std::vector<Eigen::Vector3f> verts(aimesh->mNumVertices);
	std::vector<Eigen::Vector3f> norms(aimesh->mNumVertices);
	std::vector<Eigen::Vector2f> uvs(aimesh->mNumVertices);
	// Load in Tangents and Bitangents for use in normal mapping
	std::vector<Eigen::Vector3f> tangents(aimesh->mNumVertices);
	std::vector<Eigen::Vector3f> bitangents(aimesh->mNumVertices);

	std::vector<GLuint> elems(aimesh->mNumFaces * 3);

	memcpy(verts.data(), aimesh->mVertices, aimesh->mNumVertices * sizeof(aiVector3D));
	memcpy(norms.data(), aimesh->mNormals, aimesh->mNumVertices * sizeof(aiVector3D));
	memcpy(tangents.data(), aimesh->mTangents, aimesh->mNumVertices * sizeof(aiVector3D));
	memcpy(bitangents.data(), aimesh->mBitangents, aimesh->mNumVertices * sizeof(aiVector3D));

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
	mesh->tangent(tangents);
	mesh->bitangent(bitangents);
	mesh->elems(elems);
	mesh->tex(uvs);
}

// Combine multiple identical meshes into a single mesh to render them all in a single draw call
// [Unfinished and unused]
void makeModelBatch(glhelper::Mesh* _batchMesh, std::vector<glhelper::Mesh*> _meshesToBatch)
{
	std::vector<Eigen::Vector3f> batchVerts;
	std::vector<Eigen::Vector3f> batchNorms;
	std::vector<Eigen::Vector2f> batchUVs;

	for (glhelper::Mesh* mesh : _meshesToBatch) {
		std::vector<Eigen::Vector3f> meshVerts;
		std::vector<Eigen::Vector3f> meshNorms;
		std::vector<Eigen::Vector2f> meshUVs;

		// add that mesh's vertices to meshVerts
		// add that mesh's normals to meshNorms
		// add that mesh's uvs to meshUVs

		batchVerts.insert(batchVerts.end(), meshVerts.begin(), meshVerts.end());
		batchNorms.insert(batchNorms.end(), meshNorms.begin(), meshNorms.end());
		batchUVs.insert(batchUVs.end(), meshUVs.begin(), meshUVs.end());
	}

	_batchMesh->vert(batchVerts);
	_batchMesh->norm(batchNorms);
	_batchMesh->tex(batchUVs);
}

// Used in profiling, to say how long was taken to render each object in a csv format
void WriteTimeQuery(float _timeMilli, std::string _queriedObjName, std::ofstream& _file)
{
	std::string timeMSStr = std::to_string(_timeMilli);
	_file << "Time taken for " << _queriedObjName << ", " << timeMSStr << "ms \n";
}

// Every project needs a handy-dandy degrees to radians converter
float DegToRad(float _deg)
{
	float rad = _deg * (M_PI / 180);
	return rad;
}

// OpenCV Material -> GL Texture function 
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

	// GL Text Initialisation & Set Up
	gltInit();
	GLTtext* text = gltCreateText();
	gltSetText(text, "Post-Processing: Off. Control Render Mode with Number Keys 1-3.");

	glEnable(GL_MULTISAMPLE);

// Bullet Physics Setup: \\

	// Set up Bullet World to allow for in-scene physics
	// TODO This could probably be extracted into a btHelper.InitialiseWorld function or something if I have time
	std::unique_ptr<btDefaultCollisionConfiguration> collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
	std::unique_ptr<btCollisionDispatcher> dispatcher = std::make_unique<btCollisionDispatcher>(collisionConfig.get());
	std::unique_ptr<btBroadphaseInterface> overlappingPairCache = std::make_unique<btDbvtBroadphase>();
	std::unique_ptr<btSequentialImpulseConstraintSolver> solver = std::make_unique<btSequentialImpulseConstraintSolver>();

	std::unique_ptr<btDiscreteDynamicsWorld> world = std::make_unique<btDiscreteDynamicsWorld>(dispatcher.get(), overlappingPairCache.get(), solver.get(), collisionConfig.get());
	// ----
	world->setGravity(btVector3(0, -1, 0)); // set gravity to some arbitrary downwards vector

	btAlignedObjectArray<btCollisionShape*> collisionShapes; // store all physics objects in this array!

// End of Basic Bullet Setup \\

	// Everything runtime-related in here:
	{
		// Profiling and Querying Set Up implemented into glhelper::Renderable.hpp and glhelper::Renderable.cpp

		glhelper::ShaderProgram defaultBlueShader({ "../shaders/FixedColor.vert", "../shaders/FixedColor.frag" }); // mainly for checking that meshes are loaded correctly, delete this eventually
		glhelper::ShaderProgram lightSphereShader({ "../shaders/FixedColor.vert", "../shaders/FixedColor.frag" }); // plain white to signify where the light in the scene is
		glhelper::ShaderProgram lambertShader({ "../shaders/Lambert.vert", "../shaders/Lambert.frag" }); // Flat Lambert Shader, mainly for checking that textures are loaded correctly. Used on the Campfire base
		glhelper::ShaderProgram normalMapShader({ "../shaders/NormalMapped.vert", "../shaders/NormalMapped.frag" }); // Wrap-lighting shader with Normal Mapping applied, used on log seats
		glhelper::ShaderProgram blinnPhongShader({ "../shaders/BlinnPhong.vert", "../shaders/BlinnPhong.frag" }); // Modified, Normalised Blinn-Phong model for metallic objects. Used on Sword & Shield
		glhelper::ShaderProgram shadowCubemapShader({"../shaders/ShadowMap.vert", "../shaders/ShadowMap.frag"}); // Shadow Cubemap Shader for storing depth values
		glhelper::ShaderProgram shadowMappedShader({"../shaders/ShadowMappedSurface.vert", "../shaders/ShadowMappedSurface.frag"}); // Shader that uses the shadowcast cubemap to darken areas that should be in shadow. Used on the Floor
		glhelper::ShaderProgram watercolourShader({ "../shaders/Watercolour.vert", "../shaders/Watercolour.frag" }); // This will handle the light abstraction stage (effectively Cel-Shading) for the Watercolour Renderer. TODO Needs some refinement
		glhelper::ShaderProgram fireShader({ "../shaders/FireParticle.vert", "../shaders/FireParticle.geom", "../shaders/FireParticle.frag" }); // Shader pipeline for managing fire particles, from generating billboards to applying colour and shape
		glhelper::ShaderProgram postProcessingShader({ "../shaders/PostProcessing.vert", "../shaders/PostProcessing.frag" }); // Post-Processing Shader which can be set up for any number of render modes

		// Create a viewer from the glhelper set - WASD movement and mouse rotation
		glhelper::FlyViewer viewer(windowWidth, windowHeight);
		// Some initial positioning for the camera so it doesnt spawn in the middle of the floor.
		viewer.position(Eigen::Vector3f(-8.3f, -1.0f, 3.f));
		viewer.rotation(4.49f, 0.05f);

		// Define all meshes
		std::vector<glhelper::Renderable*> renderables; // store all renderables in a Vector so it's easier to write out Render Queries en masse
		//std::vector<glhelper::Mesh*> rocksForBatching;

		glhelper::Mesh lightSphere("light");
		lightSphere.setCastsShadow(false); // the light visualiser doesnt want to cast a shadow

		// Defining all the meshes in the scene
		glhelper::Mesh floorMesh("floor"), swordMesh("sword"), shieldMesh("shield"), logSeatMesh1("seat1"), logSeatMesh2("seat2"), physicsLogMesh("fallingLog"), physicsLogMesh2("fallingLog2"), campfireBaseMesh("campfireBase");
		
		//glhelper::Mesh rock1Mesh("rock1"), rock2Mesh("rock2"), rock3Mesh("rock3"), rock4Mesh("rock4"), rockBatch("rockBatch"); // ANYTHING TO DO WITH BATCHED ROCKS HAS BEEN COMMENTED OUT
		
		// Additional Property Setup
		
		// --Floor--
		floorMesh.setCastsShadow(false); // The floor doesn't cast any shadows

		FireParticles fireParticles("fire");
		fireParticles.drawMode(GL_POINTS); // The fire is in Draw Mode: Points to allow the geometry shader to function
		fireParticles.setCastsShadow(false); // The fire doesn't cast any shadows

		// fill up the renderables list with all the meshes
		renderables = { &lightSphere, &floorMesh, &swordMesh, &shieldMesh, &logSeatMesh1, &logSeatMesh2, &physicsLogMesh, &physicsLogMesh2, &campfireBaseMesh, &fireParticles };
		//rocksForBatching = { &rock1Mesh, &rock2Mesh, &rock3Mesh, &rock4Mesh };

// -- Translation Matrix Set-up -- \\

		// Light Sphere
		Eigen::Matrix4f lightSphereM2W = makeTranslationMatrix(lightPosition);

		// Floor
		Eigen::Matrix4f floorModelToWorld = Eigen::Matrix4f::Identity();
		// Floor Physics
		btHelper.AddBoxShape(collisionShapes, world.get(), Eigen::Vector3f(0,0,1.f), Eigen::Vector3f(8.5f, 0.25f, 8.5f), 0, 1, btVector3(0,0,0));

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
		// ----
		Eigen::Matrix4f fireModelToWorld = Eigen::Matrix4f::Identity();
		fireModelToWorld *= makeTranslationMatrix(Eigen::Vector3f(0.f, -0.2f, 0.f));

		// Seats
		Eigen::Matrix4f seat1ModelToWorld = Eigen::Matrix4f::Identity();

		Eigen::Matrix4f seat2ModelToWorld = Eigen::Matrix4f::Identity();

		// Falling Log
		Eigen::Matrix4f fallingLogModelToWorld = Eigen::Matrix4f::Identity();
		auto fallingLogScale = makeScaleMatrix(1.f, 2.f, 1.f) * 0.5f;
		// Falling Log Physics
		btHelper.AddCylinderShape(collisionShapes, world.get(), Eigen::Vector3f(0, 4.f, 4.f), Eigen::Vector3f(1.f, 2.f, 1.f), 1, 0.5, btVector3(65,50,32));
		btHelper.AddCylinderShape(collisionShapes, world.get(), Eigen::Vector3f(0, 8.f, 5.f), Eigen::Vector3f(1.f, 2.f, 1.f), 1, 0.5, btVector3(65, 50, 32));


		// Batch-Rendered Rocks
		/*Eigen::Matrix4f rockModelToWorld = Eigen::Matrix4f::Identity();
		rockModelToWorld *= makeTranslationMatrix(Eigen::Vector3f(5.f,0.f,5.f));*/

// -- End of Translation Matrix Set-up -- \\


// -- Model Loading -- \\

		// Light Sphere
		loadMeshStandard(&lightSphere, assetsDirectory + "models/sphere.obj");
		lightSphere.modelToWorld(lightSphereM2W);
		lightSphere.shaderProgram(&lightSphereShader);

		// Floor
		loadMeshStandard(&floorMesh, assetsDirectory + "models/fromBlend/floor.obj");
		floorMesh.modelToWorld(floorModelToWorld);
		floorMesh.shaderProgram(&shadowMappedShader);

		// Sword
		loadMeshStandard(&swordMesh, assetsDirectory + "models/sword.obj");
		swordMesh.modelToWorld(swordModelToWorld);
		swordMesh.shaderProgram(&blinnPhongShader);

		// Shield
		loadMeshStandard(&shieldMesh, assetsDirectory + "models/shield.obj");
		shieldMesh.modelToWorld(shieldModelToWorld);
		shieldMesh.shaderProgram(&blinnPhongShader);

		// Seats
		loadMeshForNormalMapping(&logSeatMesh1, assetsDirectory + "models/fromBlend/logSeat1.obj");
		logSeatMesh1.modelToWorld(seat1ModelToWorld);
		logSeatMesh1.shaderProgram(&normalMapShader);

		loadMeshForNormalMapping(&logSeatMesh2, assetsDirectory + "models/fromBlend/logSeat2.obj");
		logSeatMesh2.modelToWorld(seat2ModelToWorld);
		logSeatMesh2.shaderProgram(&normalMapShader);

		// Falling Log
		loadMeshForNormalMapping(&physicsLogMesh, assetsDirectory + "models/logSeat.obj");
		physicsLogMesh.modelToWorld(fallingLogModelToWorld);
		physicsLogMesh.shaderProgram(&normalMapShader);
		//----
		loadMeshForNormalMapping(&physicsLogMesh2, assetsDirectory + "models/logSeat.obj");
		physicsLogMesh2.modelToWorld(fallingLogModelToWorld);
		physicsLogMesh2.shaderProgram(&normalMapShader);

		// Campfire Base
		loadMeshStandard(&campfireBaseMesh, assetsDirectory + "models/fromBlend/campfireBase.obj");
		campfireBaseMesh.modelToWorld(campfireModelToWorld);
		campfireBaseMesh.shaderProgram(&lambertShader);

		fireParticles.modelToWorld(fireModelToWorld);
		fireParticles.shaderProgram(&fireShader);

		// Rocks
		/*loadMeshStandard(&rock1Mesh, assetsDirectory + "models/rocks/rock1.obj");
		rock1Mesh.modelToWorld(rockModelToWorld);
		rockModelToWorld *= makeTranslationMatrix(Eigen::Vector3f(2.f, 0.f, 0.f));

		loadMeshStandard(&rock2Mesh, assetsDirectory + "models/rocks/rock2.obj");
		rock2Mesh.modelToWorld(rockModelToWorld);
		rockModelToWorld *= makeTranslationMatrix(Eigen::Vector3f(2.f, 0.f, 0.f));

		loadMeshStandard(&rock3Mesh, assetsDirectory + "models/rocks/rock3.obj");
		rock3Mesh.modelToWorld(rockModelToWorld);
		rockModelToWorld *= makeTranslationMatrix(Eigen::Vector3f(2.f, 0.f, 0.f));

		loadMeshStandard(&rock3Mesh, assetsDirectory + "models/rocks/rock1.obj");
		rock3Mesh.modelToWorld(rockModelToWorld);

		makeModelBatch(&rockBatch, rocksForBatching);
		rockBatch.shaderProgram(&lambertShader);*/

// -- End of Model Loading -- \\

// -- Shader Uniform Setup -- \\ 
		glProgramUniform4f(defaultBlueShader.get(), defaultBlueShader.uniformLoc("color"), 0.f, 1.f, 1.f, 1.f);  // sets the Fixed Color shader to simply render everything a nice turquoise by default
		glProgramUniform4f(lightSphereShader.get(), lightSphereShader.uniformLoc("color"), 1.f, 1.f, 1.f, 1.f); // just set the light to be plain white

		glProgramUniform1i(lambertShader.get(), lambertShader.uniformLoc("albedo"), 0);

		// Normal Map Uniform Set-up:
		{
			glProgramUniform1i(normalMapShader.get(), normalMapShader.uniformLoc("albedo"), 0); // NOTE: Albedo Maps bound to Image Unit 0
			glProgramUniform1i(normalMapShader.get(), normalMapShader.uniformLoc("normalMap"), 2); // NOTE: Normal Maps bound to Image Unit 2
			glProgramUniform3f(normalMapShader.get(), normalMapShader.uniformLoc("lightPos"), lightPosition.x(), lightPosition.y(), lightPosition.z());
			glProgramUniform1f(normalMapShader.get(), normalMapShader.uniformLoc("lightIntensity"), lightIntensity);
		}
		// Blinn-Phong Uniform Set-up:
		{
			glProgramUniform1i(blinnPhongShader.get(), blinnPhongShader.uniformLoc("albedo"), 0); // NOTE: Albedo Maps bound to Image Unit 0
			glProgramUniform1i(blinnPhongShader.get(), blinnPhongShader.uniformLoc("specularIntensity"), 1); // NOTE: Metallic Maps bound to Image Unit 1
			glProgramUniform1f(blinnPhongShader.get(), blinnPhongShader.uniformLoc("specularExponent"), specularExponent); // Kinda just an arbitrary value for now, feel free to tweak (or set on a per-model basis)
			glProgramUniform3f(blinnPhongShader.get(), blinnPhongShader.uniformLoc("lightPos"), lightPosition.x(), lightPosition.y(), lightPosition.z()); // this will need to be updated if i ever move the light
			glProgramUniform1f(blinnPhongShader.get(), blinnPhongShader.uniformLoc("lightIntensity"), lightIntensity);
		}
		// Shadowmapping Uniform Set-up
		{
			glProgramUniform1f(shadowCubemapShader.get(), shadowCubemapShader.uniformLoc("nearPlane"), shadowMapNear);
			glProgramUniform1f(shadowCubemapShader.get(), shadowCubemapShader.uniformLoc("farPlane"), shadowMapFar);
			// ----
			glProgramUniform1i(shadowMappedShader.get(), shadowMappedShader.uniformLoc("color"), 0); // NOTE: Albedo Maps bound to Image Unit 0
			glProgramUniform1f(shadowMappedShader.get(), shadowMappedShader.uniformLoc("nearPlane"), shadowMapNear);
			glProgramUniform1f(shadowMappedShader.get(), shadowMappedShader.uniformLoc("farPlane"), shadowMapFar);
			glProgramUniform3f(shadowMappedShader.get(), shadowMappedShader.uniformLoc("lightPosWorld"), lightPosition.x(), lightPosition.y(), lightPosition.z());
			glProgramUniform1i(shadowMappedShader.get(), shadowMappedShader.uniformLoc("shadowMap"), 3); // NOTE: Shadow Map bound to Image Unit 3
			glProgramUniform1f(shadowMappedShader.get(), shadowMappedShader.uniformLoc("bias"), shadowMapBias);
		}
		// Fire Particle Shader Uniform Set-up
		{
			glProgramUniform1i(fireShader.get(), fireShader.uniformLoc("flameColorTex"), 0);
			glProgramUniform1i(fireShader.get(), fireShader.uniformLoc("flameAlphaTex"), 1);

			auto flameCoeffts = fireParticles.SolveForFlameCubic();
			glProgramUniform4f(fireShader.get(), fireShader.uniformLoc("flameCubicCoeffts"), flameCoeffts[0], flameCoeffts[1], flameCoeffts[2], flameCoeffts[3]);
			glProgramUniform1f(fireShader.get(), fireShader.uniformLoc("duration"), fireParticles.GetDuration());
			glProgramUniform1f(fireShader.get(), fireShader.uniformLoc("flameHeight"), fireParticles.GetHeight());
			glProgramUniform1f(fireShader.get(), fireShader.uniformLoc("texWindowSize"), fireParticles.GetTexWindowSize());
			glProgramUniform1f(fireShader.get(), fireShader.uniformLoc("flameFadeoutStart"), fireParticles.GetFadeoutStart());
			glProgramUniform1f(fireShader.get(), fireShader.uniformLoc("flameFadeinEnd"), fireParticles.GetFadeinEnd());
		}
		// Post-Processing Uniform Setup
		{
			glProgramUniform1i(postProcessingShader.get(), postProcessingShader.uniformLoc("renderMode"), currentRenderMode);
			glProgramUniform1i(postProcessingShader.get(), postProcessingShader.uniformLoc("albedo"), 0); // NOTE: Albedo bound to Image Unit 0
			glProgramUniform1i(postProcessingShader.get(), postProcessingShader.uniformLoc("gauss"), 1); // NOTE: Gaussian Map bound to Image Unit 1
			glProgramUniform1i(postProcessingShader.get(), postProcessingShader.uniformLoc("simplex"), 2); // NOTE: Simplex Map bound to Image Unit 2
			glProgramUniform1i(postProcessingShader.get(), postProcessingShader.uniformLoc("paper"), 3); // NOTE: Paper Layer bound to Image Unit 3

		}
		// TODO Watercolour Shader Uniform Set-up:
		{
			glProgramUniform1i(watercolourShader.get(), watercolourShader.uniformLoc("albedo"), 0);	// NOTE: Albedo bound to Image Unit 0
			glProgramUniform1i(watercolourShader.get(), watercolourShader.uniformLoc("lightingTexture"), 4);	// NOTE: Cel Shading Reference Texture bound to Image Unit 4
			glProgramUniform3f(watercolourShader.get(), watercolourShader.uniformLoc("lightPosWorld"), lightPosition.x(), lightPosition.y(), lightPosition.z()); // Pass in Light Position so that Cel Shading works
		}

// -- End of Shader Uniform Setup -- \\

// -- Texture Setup -- \\ 

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
		// ----
		cv::Mat logNormalSource = cv::imread(assetsDirectory + "textures/LogSeat/logNormalMap.jpg");
		cv::cvtColor(logNormalSource, logNormalSource, cv::COLOR_BGR2RGB);
		GLuint logNormalMap;
		glGenTextures(1, &logNormalMap);
		SetUpTexture(logNormalSource, logNormalMap);


		// Campfire Base Texture Setup
		cv::Mat campfireAlbedoSource = cv::imread(assetsDirectory + "textures/Campfire/campfireBaseAlbedo.jpg");
		cv::cvtColor(campfireAlbedoSource, campfireAlbedoSource, cv::COLOR_BGR2RGB);
		GLuint campfireAlbedo;
		glGenTextures(1, &campfireAlbedo);
		SetUpTexture(campfireAlbedoSource, campfireAlbedo);

		// --FireParticles--
		cv::Mat fireColourSource = cv::imread(assetsDirectory + "textures/Campfire/flameColor.png");
		// Don't need to run cv::cvtColor here because the fire colour source is already in the right colour space
		GLuint fireColour;
		glGenTextures(1, &fireColour); // Specifically not using my SetUpTexture() method here because that function generates 2D textures, and this only wants to be 1D
		glBindTexture(GL_TEXTURE_1D, fireColour);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, fireColourSource.cols, 0, GL_BGR, GL_UNSIGNED_BYTE, fireColourSource.data);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_1D, 0);
		// ----
		cv::Mat fireAlphaSource = cv::imread(assetsDirectory + "textures/Campfire/fireAlpha.png", cv::IMREAD_UNCHANGED);
		GLuint fireAlpha;
		glGenTextures(1, &fireAlpha);
		glBindTexture(GL_TEXTURE_2D, fireAlpha);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, fireAlphaSource.cols, fireAlphaSource.rows, 0, GL_RED, GL_UNSIGNED_BYTE, fireAlphaSource.data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Rock Material
		/*cv::Mat rocksAlbedoSource = cv::imread(assetsDirectory + "textures/Rocks/RockAlbedo.png");
		cv::cvtColor(rocksAlbedoSource, rocksAlbedoSource, cv::COLOR_BGR2RGB);
		GLuint rockAlbedo;
		glGenTextures(1, &rockAlbedo);
		SetUpTexture(rocksAlbedoSource, rockAlbedo);*/

		// --Shadow Mapping--
		GLuint shadowCubemapTexture;
		glGenTextures(1, &shadowCubemapTexture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, shadowCubemapTexture);
		for (int face = 0; face < 6; face++) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT, shadowCubemapSize, shadowCubemapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		}
		glTextureParameteri(shadowCubemapTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		// ----
		Eigen::Matrix4f	shadowMapPerspective = perspective(M_PI_2, 1, shadowMapNear, shadowMapFar);
		Eigen::Matrix4f flipMatrix; // used to make sure that all the shadows are the right way round
		flipMatrix <<	-1, 0, 0, 0,
						0, -1, 0, 0,
						0, 0, 1, 0,
						0, 0, 0, 1;
		// ----
		GLuint shadowCubemapFramebuffer;
		glGenFramebuffers(1, &shadowCubemapFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, shadowCubemapFramebuffer);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowCubemapFramebuffer, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// ----
		
		// Post Processing Setup
		GLuint ppFramebuffer, ppColor, ppRenderbuffer;
		glGenFramebuffers(1, &ppFramebuffer);
		glGenTextures(1, &ppColor);
		glGenRenderbuffers(1, &ppRenderbuffer);

		glBindFramebuffer(GL_FRAMEBUFFER, ppFramebuffer);
		glBindTexture(GL_TEXTURE_2D, ppColor);
		glBindRenderbuffer(GL_RENDERBUFFER, ppRenderbuffer);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, windowWidth, windowHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// ----
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, windowWidth, windowHeight);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ppColor, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, ppRenderbuffer);
		
		// NOTE: glCheckFramebufferStatus is run before render calls to ensure this is all loaded correctly

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0); 
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		// ----

		// Watercolour Noisemap Texture Setup
		cv::Mat celTexImg = cv::imread(assetsDirectory + "textures/wcRefs/celShadeRef.png"); // the 1D Cel-Shader reference texture
		cv::cvtColor(celTexImg, celTexImg, cv::COLOR_BGR2GRAY);
		GLuint celRefTex;
		glGenTextures(1, &celRefTex);
		glBindTexture(GL_TEXTURE_1D, celRefTex);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_R8, celTexImg.cols, 0, GL_RED, GL_UNSIGNED_BYTE, celTexImg.data);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

		// ----
		bool needNewNoiseMaps = false; // Switch this to True if you need a different set of noise maps!
		if (needNewNoiseMaps) {
			noiseMapMaster.GenerateGaussianMap(windowWidth, windowHeight, 2, 0, 180, true);
			noiseMapMaster.GenerateSimplexMap(windowWidth, windowHeight, 0.007f, true);
			needNewNoiseMaps = false;
		}

		GLuint gaussTex, simplexTex, paperTex;

		cv::Mat gaussMap = cv::imread(assetsDirectory + "textures/wcRefs/noiseMaps/gaussianMap.jpg");
		cv::cvtColor(gaussMap, gaussMap, cv::COLOR_BGR2GRAY);
		glGenTextures(1, &gaussTex);
		glBindTexture(GL_TEXTURE_2D,gaussTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, gaussMap.cols, gaussMap.rows, 0, GL_RED, GL_UNSIGNED_BYTE, gaussMap.data);
		glGenerateMipmap(GL_TEXTURE_2D);

		cv::Mat simplexMap = cv::imread(assetsDirectory + "textures/wcRefs/noiseMaps/simplexMap.jpg");
		cv::cvtColor(simplexMap, simplexMap, cv::COLOR_BGR2GRAY);
		glGenTextures(1, &simplexTex);
		glBindTexture(GL_TEXTURE_2D, simplexTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, simplexMap.cols, simplexMap.rows, 0, GL_RED, GL_UNSIGNED_BYTE, simplexMap.data);
		glGenerateMipmap(GL_TEXTURE_2D);

		cv::Mat paperImg = cv::imread(assetsDirectory + "textures/wcRefs/noiseMaps/paperTexture.jpg");
		cv::cvtColor(paperImg, paperImg, cv::COLOR_BGR2RGB);
		glGenTextures(1, &paperTex);
		SetUpTexture(paperImg, paperTex);

		glBindTexture(GL_TEXTURE_2D, 0); // unbind at the end

// -- End of Texture Setup -- \\

		// Enable any GL functions here:
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS); // enable seamless cubemaps so that the shadow cubemap doesnt have annoying lines in it
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

// -- Input and controls -- \\

		std::ofstream profilerOut(queryOutDirectory + "profiling.csv");

		bool shouldQuit = false;
		SDL_Event event;

		// mark the time with a stopwatch to enable flame particle animations - this is not used for any render queries or anything that is
		// actually time-sensitive, it's just used for measuring time passed in-application, so a steady clock is fine.
		auto startTime = std::chrono::steady_clock::now();

		while (!shouldQuit) {
			Uint64 frameStartTime = SDL_GetTicks64(); // for capping framerate to the previously defined maximum

			float animationTimeSeconds = 1e-6f * (float)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTime).count();
			world->stepSimulation((desiredFrameTime / 1000.f), 10);

			// Physics Objects have their meshes moved based on their Bullet Rigidbody Transforms
			physicsLogMesh.modelToWorld(btHelper.getRigidBodyTransform(world.get(), 1) * fallingLogScale);
			physicsLogMesh2.modelToWorld(btHelper.getRigidBodyTransform(world.get(), 2) * fallingLogScale);

			viewer.update();
			
			while (SDL_PollEvent(&event)) {
				switch (event.type) {
					case(SDL_QUIT):
						shouldQuit = true;
						break;
					case (SDL_KEYDOWN):
						switch (event.key.keysym.sym) {
							case(SDLK_1):
								// Change to Render Mode 1 i.e. standard rendering
								gltSetText(text, "Post-Processing: Off. Control Render Mode with Number Keys 1-3.");
								currentRenderMode = 1;

								// Change back to regular shader programs for in-scene objects: 
								swordMesh.shaderProgram(&blinnPhongShader);
								shieldMesh.shaderProgram(&blinnPhongShader);
								logSeatMesh1.shaderProgram(&normalMapShader);
								logSeatMesh1.shaderProgram(&normalMapShader);
								logSeatMesh2.shaderProgram(&normalMapShader);
								physicsLogMesh.shaderProgram(&normalMapShader);
								physicsLogMesh2.shaderProgram(&normalMapShader);
								campfireBaseMesh.shaderProgram(&lambertShader);

								// Also change the post-process uniform!
								glProgramUniform1i(postProcessingShader.get(), postProcessingShader.uniformLoc("renderMode"), currentRenderMode);
								break;

							case (SDLK_2):
								// Change to Render Mode 2 i.e. Watercolour Shader rendering
								gltSetText(text, "Post-Processing: Watercolour Shader. Control Render Mode with Number Keys 1 - 3.");
								currentRenderMode = 2;
								// Sword, Shield, Log Seats, and Campfire Base change to a Cel Shader for Light Abstraction
								swordMesh.shaderProgram(&watercolourShader);
								shieldMesh.shaderProgram(&watercolourShader);
								logSeatMesh1.shaderProgram(&watercolourShader);
								logSeatMesh1.shaderProgram(&watercolourShader);
								logSeatMesh2.shaderProgram(&watercolourShader);
								physicsLogMesh.shaderProgram(&watercolourShader);
								physicsLogMesh2.shaderProgram(&watercolourShader);
								campfireBaseMesh.shaderProgram(&watercolourShader);

								// Turn on Post Processing Effects
								glProgramUniform1i(postProcessingShader.get(), postProcessingShader.uniformLoc("renderMode"), currentRenderMode);
								break;
							case (SDLK_3):
								gltSetText(text, "Post-Processing: Colour Breathing. Control Render Mode with Number Keys 1 - 3.");
								currentRenderMode = 3; // transition between standard color and inverted color and back
								glProgramUniform1i(postProcessingShader.get(), postProcessingShader.uniformLoc("renderMode"), currentRenderMode);
								break;
							default:
								break;
						}
						break;
					default:
						viewer.processEvent(event);
						break;
				}
			}
			// -- End of Input and Controls -- \\ 
			glClearColor(0.1f, 0.1f, 0.1f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			//glDisable(GL_CULL_FACE);

			// Set the time parameter for the fire particles before doing any rendering
			// if something takes a really long time to render then the particles won't hang (hopefully)
			glProgramUniform1f(fireShader.get(), fireShader.uniformLoc("time"), animationTimeSeconds);
			glProgramUniform1f(postProcessingShader.get(), postProcessingShader.uniformLoc("animTime"), animationTimeSeconds);

// -- Texture Binding, Rendering, and Draw Calls -- \\ 

			// Shadow Mapping First, then regular renders.
			glBindFramebuffer(GL_FRAMEBUFFER, shadowCubemapFramebuffer);

			glDisable(GL_CULL_FACE);

			glViewport(0, 0, shadowCubemapSize, shadowCubemapSize);

			for (int face = 0; face < 6; ++face) {
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, shadowCubemapTexture, 0);

				glClear(GL_DEPTH_BUFFER_BIT);

				Eigen::Matrix4f worldToClipMatrix;

				worldToClipMatrix = flipMatrix * shadowMapPerspective * cubemapRotations[face] * makeTranslationMatrix(-lightPosition);

				glProgramUniformMatrix4fv(shadowCubemapShader.get(), shadowCubemapShader.uniformLoc("shadowWorldToClip"), 1, false, worldToClipMatrix.data());

				for (glhelper::Renderable* mesh : renderables) {
					if (mesh->castsShadow()) {
						mesh->render(shadowCubemapShader);
					}
				}
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			// ----
			// For Post Processing, run glBindFramebuffer(GL_FRAMEBUFFER, ppFramebuffer) to do 
			// regular renders to the post-process framebuffer, which should gives me a 
			// 2D render of the scene stored in the ppColor texture file.

			glBindFramebuffer(GL_FRAMEBUFFER, ppFramebuffer);

			glClearColor(0.1f, 0.1f, 0.1f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			auto ppFramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			// std::cout << "PP Framebuffer Status : " << ppFramebufferStatus << std::endl; // Keeping this in as evidence that I did in fact do a framebuffer check at some point

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			}
			
			// Regular Render Calls:

			glViewport(0, 0, windowWidth, windowHeight);
			glEnable(GL_CULL_FACE);

			lightSphere.renderWithQuery();

			// Texture Binding and Rendering:

			if (currentRenderMode == 2) {	// If currently using Watercolour Rendering, set the Cel Shading Reference to Image Unit 4
				glActiveTexture(GL_TEXTURE0 + 4);
				glBindTexture(GL_TEXTURE_1D, celRefTex);
			}
			else {							// Otherwise, unbind Image Unit 4 and empty it out
				glActiveTexture(GL_TEXTURE0 + 4);
				glBindTexture(GL_TEXTURE_1D, 0);
			}

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, floorAlbedo);
			glActiveTexture(GL_TEXTURE0 + 3);
			glBindTexture(GL_TEXTURE_CUBE_MAP, shadowCubemapTexture);
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
			glActiveTexture(GL_TEXTURE0 + 2);
			glBindTexture(GL_TEXTURE_2D, logNormalMap);

			logSeatMesh1.renderWithQuery();
			logSeatMesh2.renderWithQuery();
			physicsLogMesh.renderWithQuery();
			physicsLogMesh2.renderWithQuery();

			// ----

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, campfireAlbedo);

			campfireBaseMesh.renderWithQuery();

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_1D, fireColour);
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_2D, fireAlpha);
			// Turn on GL Blend and turn off Depth Masking specifically for rendering the fire particles, then switch it all back to normal again.
			glDisable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glBlendEquation(GL_FUNC_ADD);
			glDepthMask(GL_FALSE);
			fireParticles.renderWithQuery();	

			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);
			glEnable(GL_CULL_FACE);

			// ----

			// Rock Rendering
			/*glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, rockAlbedo);
			rockBatch.render();*/

			// Text Rendering 
			gltBeginDraw();
			gltColor(1.f, 1.f, 1.f, 1.f);
			gltDrawText2D(text, 10.f, 10.f, 1.f);
			gltEndDraw();

// --Post-Processing Steps:-- \\

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glActiveTexture(GL_TEXTURE0 + 0);
			glBindTexture(GL_TEXTURE_2D, ppColor);
			glActiveTexture(GL_TEXTURE0 + 1);
			glBindTexture(GL_TEXTURE_2D, gaussTex);
			glActiveTexture(GL_TEXTURE0 + 2);
			glBindTexture(GL_TEXTURE_2D, simplexTex);
			glActiveTexture(GL_TEXTURE0 + 3);
			glBindTexture(GL_TEXTURE_2D, paperTex);

			// render some quad using a shader that takes in image unit 0 as an albedo
			glhelper::Mesh ppQuad("PostProcessQuad");
			ppQuad.drawMode(GL_TRIANGLES);

			// define the ppQuad mesh and texture coords
			// This is done is a very dodgy hack way because for one reason or another GL_TRIANGLE_STRIP with only 4 vertices was acting up
			std::vector<Eigen::Vector3f> ppQuadVerts{
				Eigen::Vector3f(-1,-1,0),
				Eigen::Vector3f(1,1,0),
				Eigen::Vector3f(-1,1,0),

				Eigen::Vector3f(1,-1,0),
				Eigen::Vector3f(1,1,0),
				Eigen::Vector3f(-1,-1,0),

				Eigen::Vector3f(-1,-1,0),
				Eigen::Vector3f(1,-1,0),
				Eigen::Vector3f(0,0,0)
			};
			ppQuad.vert(ppQuadVerts);

			std::vector<Eigen::Vector2f> ppQuadTexs{
				Eigen::Vector2f(0,0),
				Eigen::Vector2f(1,1),
				Eigen::Vector2f(0,1),

				Eigen::Vector2f(1,0),
				Eigen::Vector2f(1,1),
				Eigen::Vector2f(0,0),

				Eigen::Vector2f(0,0),
				Eigen::Vector2f(1,0),
				Eigen::Vector2f(0.5,0.5)
			};
			ppQuad.tex(ppQuadTexs);
			// ----
			Eigen::Matrix4f ppQuadM2W = Eigen::Matrix4f::Identity();
			ppQuad.modelToWorld(ppQuadM2W);
			ppQuad.shaderProgram(&postProcessingShader);
			ppQuad.render();

			glDisable(GL_CULL_FACE);
// End of Post Processing Steps \\

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
		glDeleteFramebuffers(1, &shadowCubemapFramebuffer);
		glDeleteTextures(1, &shadowCubemapTexture);
		// ----
		glDeleteTextures(1, &swordAlbedo);
		glDeleteTextures(1, &swordMetallic);
		// ----
		glDeleteTextures(1, &shieldAlbedo);
		glDeleteTextures(1, &shieldMetallic);
		// ----
		glDeleteTextures(1, &logAlbedo);
		// ----
		glDeleteTextures(1, &campfireAlbedo);
		// ----
		glDeleteTextures(1, &fireColour);
		glDeleteTextures(1, &fireAlpha);
		// ----
		//glDeleteTextures(1, &rockAlbedo);
		// ----
		glDeleteFramebuffers(1, &ppFramebuffer);
		glDeleteTextures(1, &ppColor);
		glDeleteRenderbuffers(1, &ppRenderbuffer);
		// ----
		glDeleteTextures(1, &gaussTex);
		glDeleteTextures(1, &simplexTex);
		glDeleteTextures(1, &paperTex);
		glDeleteTextures(1, &celRefTex);

		// Bullet Cleanup
		for (int i = 0; i < collisionShapes.size(); ++i) {
			delete collisionShapes[i];
		}
	}

	gltDeleteText(text);
	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}