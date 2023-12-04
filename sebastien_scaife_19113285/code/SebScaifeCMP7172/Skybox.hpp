#pragma once

#include "glhelper/Renderable.hpp"

class Skybox : public glhelper::Renderable
{
public:
	virtual void render();
	virtual void render(glhelper::ShaderProgram& program);

	Skybox& shaderProgram(glhelper::ShaderProgram* p);
	glhelper::ShaderProgram* shaderProgram() const;

private:
	void CreateCubemap(std::vector<std::string> filenames);

	glhelper::ShaderProgram* shaderProgram_;
};