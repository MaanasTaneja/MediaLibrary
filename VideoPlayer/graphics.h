#pragma once
#include <iostream>
#include "../external/gl/glad/glad.h"
#include "../external/gl/GLFW/glfw3.h"
#include "../external/stb_image.h"

//Sample graphics layer to display video frames, not part of the library core, but often used to debug library, users can feel free to implement their own window systems (or use pre existing 
// ones like SDL)

static float vertices[] = {
	// positions          // texture coords
	 1.0f,  1.0f, 0.0f,   1.0f, 1.0f, // top right
	 1.0f, -1.0f, 0.0f,   1.0f, 0.0f, // bottom right
	-1.0f, -1.0f, 0.0f,   0.0f, 0.0f, // bottom left
	-1.0f,  1.0f, 0.0f,   0.0f, 1.0f  // top left 
};
static unsigned int indices[] = {
	0, 1, 3, // first triangle
	1, 2, 3  // second triangle
};

const char* vertexShaderSource = "#version 450 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec2 aTexCoord;\n"
"out vec2 texcoord;"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
"   texcoord = vec2(aTexCoord.x, 1.0f -  aTexCoord.y);\n"
"}\0";


const char* fragmentShaderSource = "#version 450 core\n"
"out vec4 FragColor;\n"
"in vec2 texcoord;\n"
"uniform sampler2D textureY;\n"
"uniform sampler2D textureU;\n"
"uniform sampler2D textureV;\n"
"void main()\n"
"{\n"
"	float Y = texture(textureY, texcoord).r;\n"
"	float U = texture(textureU, texcoord).r - 0.5;\n"
"	float V = texture(textureV, texcoord).r - 0.5;\n"
"	float R = Y + 1.402 * V;\n"
"	float G = Y - 0.344 * U - 0.714 * V;\n"
"	float B = Y + 1.772 * U;\n"
"	FragColor = vec4(R, G, B, 1.0); \n"
"}\n\0";

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}



class Graphics {
private:
	GLFWwindow* window;
	int32_t m_width;
	int32_t m_height;

	unsigned int m_rect_vbo;
	unsigned int m_rect_vao;
	unsigned int m_rect_ebo;

	unsigned int m_texture_y;
	unsigned int m_texture_u;
	unsigned int m_texture_v;

	unsigned int m_rect_shader_program;
	unsigned int m_rect_vshader;
	unsigned int m_rect_fshader;

	int shader_get_info(unsigned int shader, unsigned int param) {
		int status;
		glGetShaderiv(shader, param, &status);
		return status;
	}

	int generic_get_error_info() {
		int status;
		status = glGetError();
		return status;
	}

public:
	Graphics(int32_t width, int32_t height) : m_width{ width }, m_height{ height }, m_rect_vbo{ 0 }, m_rect_ebo{ 0 },
		m_rect_shader_program{ 0 }, m_rect_vao{ 0 }, m_texture_y{ 0 }, m_texture_u{ 0 },m_texture_v{ 0 },
		window{ nullptr }, m_rect_vshader{ 0 }, m_rect_fshader{ 0 }
	{
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	}

	void error_check() {
		if (!shader_get_info(m_rect_vshader, GL_COMPILE_STATUS)) {
			std::cout << "Vertex Shader Compilation error!" << std::endl;
		}

		if (!shader_get_info(m_rect_fshader, GL_COMPILE_STATUS)) {
			std::cout << "Fragment Shader Compilation error!" << std::endl;
		}

		if (!shader_get_info(m_rect_shader_program, GL_LINK_STATUS)) {
			std::cout << "Program Linking error!" << std::endl;
		}

		GLenum message = generic_get_error_info();
		if (message != GL_NO_ERROR) {
			std::cout << "Error detected in VBO, Texture procedures: " << message << std::endl;
		}

	}
	int init() {
		window = glfwCreateWindow(m_width, m_height, "Video Player", NULL, NULL);
		if (window == NULL)
		{
			std::cout << "Failed to create GLFW window" << std::endl;
			glfwTerminate();
			return -1;
		}
		glfwMakeContextCurrent(window);
		glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			std::cout << "Failed to initialize GLAD" << std::endl;
			return -1;
		}


		//Generate Textures
		glGenTextures(1, &m_texture_y);

		glBindTexture(GL_TEXTURE_2D, m_texture_y);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		
		glGenTextures(1, &m_texture_u);
		glBindTexture(GL_TEXTURE_2D, m_texture_u);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		
		glGenTextures(1, &m_texture_v);
		glBindTexture(GL_TEXTURE_2D, m_texture_v);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		glBindTexture(GL_TEXTURE_2D, 0);

		//Generate Shaders
		m_rect_vshader = glCreateShader(GL_VERTEX_SHADER);
		m_rect_fshader = glCreateShader(GL_FRAGMENT_SHADER);

		glShaderSource(m_rect_vshader, 1, &vertexShaderSource, NULL);
		glCompileShader(m_rect_vshader);

		glShaderSource(m_rect_fshader, 1, &fragmentShaderSource, NULL);
		glCompileShader(m_rect_fshader);

		m_rect_shader_program = glCreateProgram();
		glAttachShader(m_rect_shader_program, m_rect_vshader);
		glAttachShader(m_rect_shader_program, m_rect_fshader);

		glLinkProgram(m_rect_shader_program);
		glDeleteShader(m_rect_vshader);
		glDeleteShader(m_rect_fshader);

		//Buffers and VAO
		glGenVertexArrays(1, &m_rect_vao);
		glBindVertexArray(m_rect_vao);

		glGenBuffers(1, &m_rect_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, m_rect_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

		glGenBuffers(1, &m_rect_ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rect_ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_STATIC_DRAW);

		glEnableVertexArrayAttrib(m_rect_vao, 0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, reinterpret_cast <const void*> (0));
		glEnableVertexArrayAttrib(m_rect_vao, 1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, reinterpret_cast<const void*> (sizeof(float) * 3)); //Pointer says - this data (uv) start after 3 floats in packed structure.

		glfwSetTime(0.0);
		return 0;
	}

	int bindScreenQuad() {
		GLint t = -1;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &t);

		if (t == m_rect_vao) {
			return 0;
			
		}
	}

	void bindTexture(uint8_t** data) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_texture_y);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_width, m_height, 0, GL_RED, GL_UNSIGNED_BYTE, data[0]);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_texture_u);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_width/2, m_height/2, 0, GL_RED, GL_UNSIGNED_BYTE, data[1]);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, m_texture_v);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_width/2, m_height/2, 0, GL_RED, GL_UNSIGNED_BYTE, data[2]);
	}

	void bindTestTexture(unsigned char* data, int w, int h) {
		//Unisgnec har data is unsigned byte not float!
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_texture_y);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_texture_u);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, m_texture_v);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	}

	void startFrame() {
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void draw(double presentation_time) {

		while (presentation_time > glfwGetTime()) {
			glfwWaitEventsTimeout(presentation_time - glfwGetTime());
		}

		glUseProgram(m_rect_shader_program);

		glUniform1i(glGetUniformLocation(m_rect_shader_program, "textureY"), 0);
		glUniform1i(glGetUniformLocation(m_rect_shader_program, "textureU"), 1);
		glUniform1i(glGetUniformLocation(m_rect_shader_program, "textureV"), 2);

		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}

	void endFrame() {
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	int resetState() {
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
	}

	int livestatus() {
		return glfwWindowShouldClose(window);
	}

	~Graphics() {
		glfwTerminate();
	}
	
};