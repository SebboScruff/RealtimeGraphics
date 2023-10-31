#include "Renderable.hpp"

namespace glhelper {

	Renderable::Renderable(std::string name, const Eigen::Matrix4f &modelToWorld) :name(name), Entity(modelToWorld)
	{
		glGenQueries(1, &query);
	}

	void Renderable::renderWithQuery()
	{
		glBeginQuery(GL_TIME_ELAPSED, query);
		this->render();
		glEndQuery(GL_TIME_ELAPSED);
	}

	float Renderable::getRenderTime()
	{
		GLuint64 timeTaken;
		GLint available = 0;

		//while(available == 0){
		//	glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
		//}
		glGetQueryObjectui64v(query, GL_QUERY_RESULT, &timeTaken); // gets the query time in nanoseconds

		return (timeTaken * 1e-6); // convert Nanoseconds to Milliseconds and return as a float
	}

	Renderable::~Renderable() {
		glDeleteQueries(1, &query);
	}
}
